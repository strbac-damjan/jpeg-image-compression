#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h> // for abs, etc.
#include <TI/tivx.h>
#include <utils/ipc/include/app_ipc.h>
#include <utils/remote_service/include/app_remote_service.h>
#include <utils/app_init/include/app_init.h>
#include <utils/console_io/include/app_log.h>
#include <utils/mem/include/app_mem.h> 

// Ensure jpeg_compression.h includes the RLESymbol definition
#include <jpeg_compression.h> 
#include "bmp_handler.h"
#include "jpeg_handler.h" // Koristimo novi header za upis bitstreama

// Definition must match DSP side
typedef struct {
    uint8_t symbol;    // (Run << 4) | Size
    uint8_t codeBits;  // Number of significant bits
    uint16_t code;     // The amplitude value (variable length bits)
} RLESymbol;

/* DTO structure shared between A72 (Host) and C7x (DSP). */
typedef struct JPEG_COMPRESSION_DTO
{
    int32_t width;
    int32_t height;
    
    // Physical addresses of input channels (R, G, B)
    uint64_t r_phy_ptr;
    uint64_t g_phy_ptr;
    uint64_t b_phy_ptr;

    // Physical address of the output buffer (Y - intermediate)
    uint64_t y_phy_ptr;

    // Physical address of the output buffer (DCT Coefficients - intermediate)
    uint64_t dct_phy_ptr;

    // Physical address of the output buffer (Quantized Coefficients - intermediate)
    uint64_t quant_phy_ptr;

    // Physical address of the output buffer (ZigZag Scanned - intermediate)
    uint64_t zigzag_phy_ptr;

    // Physical address of the output buffer (RLE Symbols - intermediate)
    uint64_t rle_phy_ptr;
    uint32_t rle_count; // Output from DSP

    // Physical address of the output buffer (Huffman Bitstream - FINAL)
    uint64_t huff_phy_ptr;
    uint32_t huff_size; // Output from DSP (in bytes)

} JPEG_COMPRESSION_DTO;

// Helper function to align dimensions to next multiple of 8 (MCU size)
int32_t align8(int32_t x) {
    return (x + 7) & ~7;
}

int main(int argc, char* argv[])
{
    int32_t status;
    
    if (argc < 3) {
        appLogPrintf("Usage: %s <input_bmp_path> <output_jpeg_path>\n", argv[0]);
        return -1;
    }
    const char* inputPath = argv[1];
    const char* outputPath = argv[2];
    
    // Application Initialization
    appLogPrintf("JPEG: Initializing App...\n");
    status = appInit();
    if(status != 0)
    {
        appLogPrintf("JPEG: App initialization failed!\n");
        return 1;
    }

    // Load BMP image
    appLogPrintf("JPEG: Loading BMP image...\n");
    BMPImage* img = loadBMPImage(inputPath);

    if(!img) {
        appLogPrintf("JPEG: Image loading failed!\n");
        appDeInit(); 
        return 1;
    }
    
    /* -------------------------------------------------------------------------
     * 1. CALCULATE ALIGNED DIMENSIONS (PADDING)
     * -------------------------------------------------------------------------
     */
    int32_t orig_w = img->width;
    int32_t orig_h = img->height;
    
    // Align to multiple of 8 (JPEG MCU size)
    int32_t aligned_w = align8(orig_w);
    int32_t aligned_h = align8(orig_h);
    
    uint32_t aligned_pixel_count = aligned_w * aligned_h;
    
    appLogPrintf("JPEG: Image Loaded: %dx%d\n", orig_w, orig_h);
    appLogPrintf("JPEG: Padded Dimensions for DSP: %dx%d (Total pixels: %d)\n", 
                 aligned_w, aligned_h, aligned_pixel_count);

    /* -------------------------------------------------------------------------
     * 2. ALLOCATE & PREPARE INPUT BUFFERS (PADDED)
     * -------------------------------------------------------------------------
     */
    // We allocate new buffers for Input so we can pad the edges properly.
    uint8_t* r_padded = (uint8_t*)appMemAlloc(APP_MEM_HEAP_DDR, aligned_pixel_count, 64);
    uint8_t* g_padded = (uint8_t*)appMemAlloc(APP_MEM_HEAP_DDR, aligned_pixel_count, 64);
    uint8_t* b_padded = (uint8_t*)appMemAlloc(APP_MEM_HEAP_DDR, aligned_pixel_count, 64);

    if (!r_padded || !g_padded || !b_padded) {
        appLogPrintf("JPEG: Failed to allocate input padding buffers!\n");
        // Free and exit logic...
        freeBMPImage(img);
        appDeInit();
        return 1;
    }

    // Copy data with Edge Extension (Clamping)
    // This fills the extra pixels (padding) with the value of the last valid pixel
    // to prevent artifacts at the bottom/right edges.
    for (int y = 0; y < aligned_h; y++) {
        // Clamp Y to valid range [0, orig_h - 1]
        int src_y = (y < orig_h) ? y : (orig_h - 1);
        
        for (int x = 0; x < aligned_w; x++) {
            // Clamp X to valid range [0, orig_w - 1]
            int src_x = (x < orig_w) ? x : (orig_w - 1);
            
            int src_idx = src_y * orig_w + src_x;
            int dst_idx = y * aligned_w + x; // Padded buffer has wider stride
            
            r_padded[dst_idx] = img->r[src_idx];
            g_padded[dst_idx] = img->g[src_idx];
            b_padded[dst_idx] = img->b[src_idx];
        }
    }

    // Flush Input Cache
    appMemCacheWb(r_padded, aligned_pixel_count);
    appMemCacheWb(g_padded, aligned_pixel_count);
    appMemCacheWb(b_padded, aligned_pixel_count);

    /* -------------------------------------------------------------------------
     * 3. ALLOCATE OUTPUT BUFFERS (BASED ON ALIGNED SIZE)
     * -------------------------------------------------------------------------
     */
    
    // A) Y Component Buffer
    uint8_t* y_output_virt = (uint8_t*)appMemAlloc(APP_MEM_HEAP_DDR, aligned_pixel_count, 64);
    
    // B) DCT Coefficients Buffer
    uint32_t dct_size_bytes = aligned_pixel_count * sizeof(float);
    float* dct_output_virt = (float*)appMemAlloc(APP_MEM_HEAP_DDR, dct_size_bytes, 64);

    // C) Quantized Coefficients Buffer
    uint32_t quant_size_bytes = aligned_pixel_count * sizeof(int16_t);
    int16_t* quant_output_virt = (int16_t*)appMemAlloc(APP_MEM_HEAP_DDR, quant_size_bytes, 64);
    
    // D) ZigZag Scanned Buffer
    int16_t* zigzag_output_virt = (int16_t*)appMemAlloc(APP_MEM_HEAP_DDR, quant_size_bytes, 64);

    // E) RLE Output Buffer
    uint32_t rle_size_bytes = aligned_pixel_count * sizeof(RLESymbol);
    RLESymbol* rle_output_virt = (RLESymbol*)appMemAlloc(APP_MEM_HEAP_DDR, rle_size_bytes, 64);

    // F) Huffman Output Buffer (Final Bitstream)
    uint32_t huff_capacity_bytes = aligned_pixel_count; 
    uint8_t* huff_output_virt = (uint8_t*)appMemAlloc(APP_MEM_HEAP_DDR, huff_capacity_bytes, 64);

    // Check allocations
    if (!y_output_virt || !dct_output_virt || !quant_output_virt || !zigzag_output_virt || !rle_output_virt || !huff_output_virt) {
        appLogPrintf("JPEG: Failed to allocate output memory!\n");
        // Free everything
        if(r_padded) appMemFree(APP_MEM_HEAP_DDR, r_padded, aligned_pixel_count);
        if(g_padded) appMemFree(APP_MEM_HEAP_DDR, g_padded, aligned_pixel_count);
        if(b_padded) appMemFree(APP_MEM_HEAP_DDR, b_padded, aligned_pixel_count);
        
        if(y_output_virt) appMemFree(APP_MEM_HEAP_DDR, y_output_virt, aligned_pixel_count);
        if(dct_output_virt) appMemFree(APP_MEM_HEAP_DDR, dct_output_virt, dct_size_bytes);
        if(quant_output_virt) appMemFree(APP_MEM_HEAP_DDR, quant_output_virt, quant_size_bytes);
        if(zigzag_output_virt) appMemFree(APP_MEM_HEAP_DDR, zigzag_output_virt, quant_size_bytes);
        if(rle_output_virt) appMemFree(APP_MEM_HEAP_DDR, rle_output_virt, rle_size_bytes);
        if(huff_output_virt) appMemFree(APP_MEM_HEAP_DDR, huff_output_virt, huff_capacity_bytes);

        freeBMPImage(img);
        appDeInit();
        return 1;
    }

    // Initialize buffers to zero
    memset(y_output_virt, 0, aligned_pixel_count);
    memset(dct_output_virt, 0, dct_size_bytes);
    memset(quant_output_virt, 0, quant_size_bytes);
    memset(zigzag_output_virt, 0, quant_size_bytes);
    memset(rle_output_virt, 0, rle_size_bytes);
    memset(huff_output_virt, 0, huff_capacity_bytes);

    // Perform Cache Write-back to flush zeros to DDR
    appMemCacheWb(y_output_virt, aligned_pixel_count);
    appMemCacheWb(dct_output_virt, dct_size_bytes);
    appMemCacheWb(quant_output_virt, quant_size_bytes);
    appMemCacheWb(zigzag_output_virt, quant_size_bytes);
    appMemCacheWb(rle_output_virt, rle_size_bytes);
    appMemCacheWb(huff_output_virt, huff_capacity_bytes);

    /* -------------------------------------------------------------------------
     * 4. PREPARE DTO FOR IPC
     * -------------------------------------------------------------------------
     */
    appLogPrintf("JPEG: Preparing data for DSP...\n");
    JPEG_COMPRESSION_DTO dto;
    memset(&dto, 0, sizeof(dto));
    
    // IMPORTANT: Send ALIGNED dimensions to DSP
    dto.width = aligned_w;
    dto.height = aligned_h;

    // Use PADDED input buffer addresses
    dto.r_phy_ptr = appMemGetVirt2PhyBufPtr((uint64_t)r_padded, APP_MEM_HEAP_DDR);
    dto.g_phy_ptr = appMemGetVirt2PhyBufPtr((uint64_t)g_padded, APP_MEM_HEAP_DDR);
    dto.b_phy_ptr = appMemGetVirt2PhyBufPtr((uint64_t)b_padded, APP_MEM_HEAP_DDR);
    
    dto.y_phy_ptr = appMemGetVirt2PhyBufPtr((uint64_t)y_output_virt, APP_MEM_HEAP_DDR);
    dto.dct_phy_ptr = appMemGetVirt2PhyBufPtr((uint64_t)dct_output_virt, APP_MEM_HEAP_DDR);
    dto.quant_phy_ptr = appMemGetVirt2PhyBufPtr((uint64_t)quant_output_virt, APP_MEM_HEAP_DDR);
    dto.zigzag_phy_ptr = appMemGetVirt2PhyBufPtr((uint64_t)zigzag_output_virt, APP_MEM_HEAP_DDR);
    
    dto.rle_phy_ptr = appMemGetVirt2PhyBufPtr((uint64_t)rle_output_virt, APP_MEM_HEAP_DDR);
    dto.rle_count = 0; 

    dto.huff_phy_ptr = appMemGetVirt2PhyBufPtr((uint64_t)huff_output_virt, APP_MEM_HEAP_DDR);
    dto.huff_size = 0;

    /* -------------------------------------------------------------------------
     * 5. RUN DSP PROCESS
     * -------------------------------------------------------------------------
     */
    appLogPrintf("JPEG: Sending data via Remote Service...\n");
    status = appRemoteServiceRun(
        APP_IPC_CPU_C7x_1,
        "com.etfbl.sdos.jpeg_compression",
        0, // Command ID
        &dto,
        sizeof(dto),
        0
    );

    if(status != 0) 
    {
        appLogPrintf("JPEG: Error sending data/executing on DSP! Status: %d\n", status);
    } 
    else 
    {
        appLogPrintf("JPEG: DSP Processing Done! Verifying results...\n");
        
        // Invalidate caches to read results from DDR
        appMemCacheInv(y_output_virt, aligned_pixel_count);
        appMemCacheInv(dct_output_virt, dct_size_bytes);
        appMemCacheInv(quant_output_virt, quant_size_bytes);
        appMemCacheInv(zigzag_output_virt, quant_size_bytes);
        appMemCacheInv(rle_output_virt, rle_size_bytes);
        appMemCacheInv(huff_output_virt, huff_capacity_bytes); 

        // --- Verify Y Component (First 8x8) ---
        appLogPrintf("Y Component check (First row):\n");
        int8_t* signed_y = (int8_t*)y_output_virt;
        for (int i = 0; i < 8; i++) printf("%4d ", signed_y[i]); 
        printf("\n");

        // --- HUFFMAN OUTPUT & SAVE ---
        appLogPrintf("\n--------------------------------------------------\n");
        appLogPrintf("HUFFMAN Output (FINAL BITSTREAM)\n");
        appLogPrintf("Compressed Size : %d bytes\n", dto.huff_size);
        
        if(dto.huff_size > 0) {
            float ratio = (float)orig_w * orig_h / (float)dto.huff_size;
            appLogPrintf("Compression Ratio: %.2f : 1\n", ratio);
            
            // IMPORTANT: Save using ORIGINAL dimensions.
            // The bitstream contains padded data (e.g. for 856px height), 
            // but the header will say 853px. Decoders handle this by cropping.
            bool saved = saveJPEG(outputPath, orig_w, orig_h, huff_output_virt, dto.huff_size);
            
            if (saved) {
                appLogPrintf("SUCCESS: Image saved to %s\n", outputPath);
            } else {
                appLogPrintf("ERROR: Failed to save image!\n");
            }

        } else {
            appLogPrintf("ERROR: Huffman size is 0!\n");
        }
        appLogPrintf("--------------------------------------------------\n");
    }

    // Cleanup resources
    appLogPrintf("JPEG: Cleaning up...\n");
    
    // Free PADDED input buffers
    appMemFree(APP_MEM_HEAP_DDR, r_padded, aligned_pixel_count);
    appMemFree(APP_MEM_HEAP_DDR, g_padded, aligned_pixel_count);
    appMemFree(APP_MEM_HEAP_DDR, b_padded, aligned_pixel_count);

    // Free Output buffers
    if(y_output_virt) appMemFree(APP_MEM_HEAP_DDR, y_output_virt, aligned_pixel_count);
    if(dct_output_virt) appMemFree(APP_MEM_HEAP_DDR, dct_output_virt, dct_size_bytes);
    if(quant_output_virt) appMemFree(APP_MEM_HEAP_DDR, quant_output_virt, quant_size_bytes);
    if(zigzag_output_virt) appMemFree(APP_MEM_HEAP_DDR, zigzag_output_virt, quant_size_bytes);
    if(rle_output_virt) appMemFree(APP_MEM_HEAP_DDR, rle_output_virt, rle_size_bytes);
    if(huff_output_virt) appMemFree(APP_MEM_HEAP_DDR, huff_output_virt, huff_capacity_bytes);
    
    freeBMPImage(img);
    appDeInit();

    return 0;
}