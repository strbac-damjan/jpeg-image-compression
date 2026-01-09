#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <TI/tivx.h>
#include <utils/ipc/include/app_ipc.h>
#include <utils/remote_service/include/app_remote_service.h>
#include <utils/app_init/include/app_init.h>
#include <utils/console_io/include/app_log.h>
#include <utils/mem/include/app_mem.h> 

// Ensure jpeg_compression.h includes the RLESymbol definition we added earlier
#include <jpeg_compression.h> 
#include "bmp_handler.h"

typedef struct {
    uint8_t symbol;    // (Run << 4) | Size
    uint8_t codeBits;  // Number of significant bits
    uint16_t code;     // The amplitude value (variable length bits)
} RLESymbol;


/* * DTO structure shared between A72 (Host) and C7x (DSP).
 * This definition must match the one in the DSP source code (jpeg_compression.h) exactly.
 */
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

    // Physical address of the output buffer (RLE Symbols - final)
    uint64_t rle_phy_ptr;
    
    // OUTPUT: Number of RLE symbols written by DSP
    uint32_t rle_count;

} JPEG_COMPRESSION_DTO;

int main(int argc, char* argv[])
{
    int32_t status;
    
    if (argc < 2) {
        appLogPrintf("Usage: %s <input_bmp_path>\n", argv[0]);
        return -1;
    }
    const char* inputPath = argv[1];
    
    // Application Initialization
    appLogPrintf("JPEG: Initializing App...\n");
    status = appInit();
    if(status != 0)
    {
        appLogPrintf("JPEG: App initialization failed!\n");
        return 1;
    }
    appLogPrintf("JPEG: App initialization successful!\n");

    // Load BMP image (Uses appMemAlloc internally for pixel buffers)
    appLogPrintf("JPEG: Loading BMP image...\n");
    BMPImage* img = loadBMPImage(inputPath);

    if(!img) {
        appLogPrintf("JPEG: Image loading failed!\n");
        appDeInit(); 
        return 1;
    }
    appLogPrintf("JPEG: BMP image loaded (Width: %d, Height: %d)!\n", img->width, img->height);

    uint32_t pixel_count = img->width * img->height;

    /* -------------------------------------------------------------------------
     * Allocate output buffers in Shared Memory
     * -------------------------------------------------------------------------
     */
    
    // A) Y Component Buffer (1 byte per pixel)
    uint8_t* y_output_virt = (uint8_t*)appMemAlloc(APP_MEM_HEAP_DDR, pixel_count, 64);
    
    // B) DCT Coefficients Buffer (4 bytes per pixel - float)
    uint32_t dct_size_bytes = pixel_count * sizeof(float);
    float* dct_output_virt = (float*)appMemAlloc(APP_MEM_HEAP_DDR, dct_size_bytes, 64);

    // C) Quantized Coefficients Buffer (2 bytes per pixel - int16_t)
    uint32_t quant_size_bytes = pixel_count * sizeof(int16_t);
    int16_t* quant_output_virt = (int16_t*)appMemAlloc(APP_MEM_HEAP_DDR, quant_size_bytes, 64);
    
    // D) ZigZag Scanned Buffer (2 bytes per pixel - int16_t)
    int16_t* zigzag_output_virt = (int16_t*)appMemAlloc(APP_MEM_HEAP_DDR, quant_size_bytes, 64);

    // E) RLE Output Buffer (4 bytes per symbol)
    // We allocate a "worst case" size (1 symbol per pixel), though real usage is much lower.
    uint32_t rle_size_bytes = pixel_count * sizeof(RLESymbol);
    RLESymbol* rle_output_virt = (RLESymbol*)appMemAlloc(APP_MEM_HEAP_DDR, rle_size_bytes, 64);

    // Check allocations
    if (!y_output_virt || !dct_output_virt || !quant_output_virt || !zigzag_output_virt || !rle_output_virt) {
        appLogPrintf("JPEG: Failed to allocate output memory!\n");
        if(y_output_virt) appMemFree(APP_MEM_HEAP_DDR, y_output_virt, pixel_count);
        if(dct_output_virt) appMemFree(APP_MEM_HEAP_DDR, dct_output_virt, dct_size_bytes);
        if(quant_output_virt) appMemFree(APP_MEM_HEAP_DDR, quant_output_virt, quant_size_bytes);
        if(zigzag_output_virt) appMemFree(APP_MEM_HEAP_DDR, zigzag_output_virt, quant_size_bytes);
        if(rle_output_virt) appMemFree(APP_MEM_HEAP_DDR, rle_output_virt, rle_size_bytes);
        freeBMPImage(img);
        appDeInit();
        return 1;
    }

    // Initialize buffers to zero
    memset(y_output_virt, 0, pixel_count);
    memset(dct_output_virt, 0, dct_size_bytes);
    memset(quant_output_virt, 0, quant_size_bytes);
    memset(zigzag_output_virt, 0, quant_size_bytes);
    memset(rle_output_virt, 0, rle_size_bytes);

    // Perform Cache Write-back to flush zeros to DDR
    appMemCacheWb(y_output_virt, pixel_count);
    appMemCacheWb(dct_output_virt, dct_size_bytes);
    appMemCacheWb(quant_output_virt, quant_size_bytes);
    appMemCacheWb(zigzag_output_virt, quant_size_bytes);
    appMemCacheWb(rle_output_virt, rle_size_bytes);

    /* -------------------------------------------------------------------------
     * Prepare DTO for IPC
     * -------------------------------------------------------------------------
     */
    appLogPrintf("JPEG: Preparing data for DSP...\n");
    JPEG_COMPRESSION_DTO dto;
    
    dto.width = img->width;
    dto.height = img->height;

    // Convert Virtual addresses to Physical addresses
    dto.r_phy_ptr = appMemGetVirt2PhyBufPtr((uint64_t)img->r, APP_MEM_HEAP_DDR);
    dto.g_phy_ptr = appMemGetVirt2PhyBufPtr((uint64_t)img->g, APP_MEM_HEAP_DDR);
    dto.b_phy_ptr = appMemGetVirt2PhyBufPtr((uint64_t)img->b, APP_MEM_HEAP_DDR);
    
    dto.y_phy_ptr = appMemGetVirt2PhyBufPtr((uint64_t)y_output_virt, APP_MEM_HEAP_DDR);
    dto.dct_phy_ptr = appMemGetVirt2PhyBufPtr((uint64_t)dct_output_virt, APP_MEM_HEAP_DDR);
    dto.quant_phy_ptr = appMemGetVirt2PhyBufPtr((uint64_t)quant_output_virt, APP_MEM_HEAP_DDR);
    dto.zigzag_phy_ptr = appMemGetVirt2PhyBufPtr((uint64_t)zigzag_output_virt, APP_MEM_HEAP_DDR);
    
    // RLE setup
    dto.rle_phy_ptr = appMemGetVirt2PhyBufPtr((uint64_t)rle_output_virt, APP_MEM_HEAP_DDR);
    dto.rle_count = 0; // Initialize count

    /* -------------------------------------------------------------------------
     * Send command via Remote Service
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
        appLogPrintf("JPEG: Error sending data/executing on DSP!\n");
    } 
    else 
    {
        appLogPrintf("JPEG: DSP Processing Done! Verifying results...\n");
        
        /* -------------------------------------------------------------------------
         * Invalidate Cache & Verify Results
         * -------------------------------------------------------------------------
         */

        // Invalidate A72 cache for all output buffers
        appMemCacheInv(y_output_virt, pixel_count);
        appMemCacheInv(dct_output_virt, dct_size_bytes);
        appMemCacheInv(quant_output_virt, quant_size_bytes);
        appMemCacheInv(zigzag_output_virt, quant_size_bytes);
        appMemCacheInv(rle_output_virt, rle_size_bytes);

        // --- Verify Y Component ---
        appLogPrintf("\n--------------------------------------------------\n");
        appLogPrintf("Y Component (First 8x8 Block - Linear View):\n");
        appLogPrintf("--------------------------------------------------\n");
        int8_t* signed_y = (int8_t*)y_output_virt;
        for (int i = 0; i < 64 && i < pixel_count; i++) {
            if (i % 8 == 0) printf("\nRow %d: ", i/8);
            printf("%4d ", signed_y[i]); 
        }
        printf("\n");

        // --- Verify DCT Coefficients ---
        appLogPrintf("\n--------------------------------------------------\n");
        appLogPrintf("DCT Coefficients (First 8x8 Block):\n");
        appLogPrintf("--------------------------------------------------\n");
        for (int row = 0; row < 8; row++) {
            printf("\nRow %d: ", row);
            for (int col = 0; col < 8; col++) {
                int idx = row * img->width + col;
                if (idx < pixel_count) {
                    printf("%7.2f ", dct_output_virt[idx]);
                }
            }
        }
        printf("\n");

        // --- Verify Quantized Coefficients ---
        appLogPrintf("\n--------------------------------------------------\n");
        appLogPrintf("Quantized Coefficients (First 8x8 Block):\n");
        appLogPrintf("--------------------------------------------------\n");
        for (int row = 0; row < 8; row++) {
            printf("\nRow %d: ", row);
            for (int col = 0; col < 8; col++) {
                int idx = row * img->width + col;
                if (idx < pixel_count) {
                    printf("%4d ", quant_output_virt[idx]);
                }
            }
        }
        printf("\n");

        // --- Verify Zig-Zag Output ---
        appLogPrintf("\n--------------------------------------------------\n");
        appLogPrintf("Zig-Zag Output (First 8x8 Block - Linearized):\n");
        appLogPrintf("--------------------------------------------------\n");
        for (int i = 0; i < 64; i++) {
            if (i % 8 == 0) printf("\nLine %d: ", i/8);
            printf("%4d ", zigzag_output_virt[i]);
        }
        printf("\n");

        // --- Verify RLE Output ---
        appLogPrintf("\n--------------------------------------------------\n");
        appLogPrintf("RLE Output (Total Symbols Generated: %d)\n", dto.rle_count);
        appLogPrintf("--------------------------------------------------\n");
        
        if (dto.rle_count == 0) {
            appLogPrintf("WARNING: RLE count is 0. Something went wrong on DSP.\n");
        } else {
            // Print first 40 symbols for verification
            int limit = (dto.rle_count < 40) ? dto.rle_count : 40;
            
            for (int i = 0; i < limit; i++) {
                RLESymbol s = rle_output_virt[i];
                uint8_t run = s.symbol >> 4;
                uint8_t size = s.symbol & 0x0F;
                
                // Format: [Index] SymbolHex (Run, Size) : CodeVal
                printf("[%3d] Sym:0x%02X (Run:%2d, Size:%2d) Code:%5d (Bits:%d)", 
                       i, s.symbol, run, size, s.code, s.codeBits);
                       
                if (s.symbol == 0x00) printf(" <--- EOB");
                if (i == 0) printf(" <--- DC of Block 0");
                
                printf("\n");
            }
            
            // Calculate compression ratio (simplified, assuming input was 1 byte/pixel Y-only for now)
            // Original Y size: pixel_count bytes
            // Compressed size (approx): rle_count * 2 bytes (conservatively, though Huffman packs tighter)
            float compression_ratio = ((float)dto.rle_count * sizeof(RLESymbol)) / (float)pixel_count;
            appLogPrintf("\nIntermediate Compression Info:\n");
            appLogPrintf("Raw Pixels: %d\n", pixel_count);
            appLogPrintf("RLE Symbols: %d\n", dto.rle_count);
            appLogPrintf("RLE Symbol Buffer Usage: %.2f%% of original size\n", compression_ratio * 100.0f);
        }
        printf("\n");
    }

    // Cleanup resources
    appLogPrintf("JPEG: Cleaning up...\n");
    
    if(y_output_virt) appMemFree(APP_MEM_HEAP_DDR, y_output_virt, pixel_count);
    if(dct_output_virt) appMemFree(APP_MEM_HEAP_DDR, dct_output_virt, dct_size_bytes);
    if(quant_output_virt) appMemFree(APP_MEM_HEAP_DDR, quant_output_virt, quant_size_bytes);
    if(zigzag_output_virt) appMemFree(APP_MEM_HEAP_DDR, zigzag_output_virt, quant_size_bytes);
    if(rle_output_virt) appMemFree(APP_MEM_HEAP_DDR, rle_output_virt, rle_size_bytes);
    
    freeBMPImage(img);
    appDeInit();

    return 0;
}
