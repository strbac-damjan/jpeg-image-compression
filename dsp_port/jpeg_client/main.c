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

// Definition must match DSP side
typedef struct {
    uint8_t symbol;    // (Run << 4) | Size
    uint8_t codeBits;  // Number of significant bits
    uint16_t code;     // The amplitude value (variable length bits)
} RLESymbol;

/* * DTO structure shared between A72 (Host) and C7x (DSP).
 * This definition must match the one in the DSP source code exactly.
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

    // Physical address of the output buffer (RLE Symbols - intermediate)
    uint64_t rle_phy_ptr;
    uint32_t rle_count; // Output from DSP

    // Physical address of the output buffer (Huffman Bitstream - FINAL)
    uint64_t huff_phy_ptr;
    uint32_t huff_size; // Output from DSP (in bytes)

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

    // Load BMP image
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
    
    // A) Y Component Buffer
    uint8_t* y_output_virt = (uint8_t*)appMemAlloc(APP_MEM_HEAP_DDR, pixel_count, 64);
    
    // B) DCT Coefficients Buffer
    uint32_t dct_size_bytes = pixel_count * sizeof(float);
    float* dct_output_virt = (float*)appMemAlloc(APP_MEM_HEAP_DDR, dct_size_bytes, 64);

    // C) Quantized Coefficients Buffer
    uint32_t quant_size_bytes = pixel_count * sizeof(int16_t);
    int16_t* quant_output_virt = (int16_t*)appMemAlloc(APP_MEM_HEAP_DDR, quant_size_bytes, 64);
    
    // D) ZigZag Scanned Buffer
    int16_t* zigzag_output_virt = (int16_t*)appMemAlloc(APP_MEM_HEAP_DDR, quant_size_bytes, 64);

    // E) RLE Output Buffer
    uint32_t rle_size_bytes = pixel_count * sizeof(RLESymbol);
    RLESymbol* rle_output_virt = (RLESymbol*)appMemAlloc(APP_MEM_HEAP_DDR, rle_size_bytes, 64);

    // F) Huffman Output Buffer (Final Bitstream)
    // Allocating pixel_count bytes is safe (compressed is usually much smaller)
    uint32_t huff_capacity_bytes = pixel_count; 
    uint8_t* huff_output_virt = (uint8_t*)appMemAlloc(APP_MEM_HEAP_DDR, huff_capacity_bytes, 64);

    // Check allocations
    if (!y_output_virt || !dct_output_virt || !quant_output_virt || !zigzag_output_virt || !rle_output_virt || !huff_output_virt) {
        appLogPrintf("JPEG: Failed to allocate output memory!\n");
        // Free everything (simplified logic)
        if(y_output_virt) appMemFree(APP_MEM_HEAP_DDR, y_output_virt, pixel_count);
        // ... (other frees omitted for brevity in snippet, but include in real code)
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
    memset(huff_output_virt, 0, huff_capacity_bytes);

    // Perform Cache Write-back to flush zeros to DDR
    appMemCacheWb(y_output_virt, pixel_count);
    appMemCacheWb(dct_output_virt, dct_size_bytes);
    appMemCacheWb(quant_output_virt, quant_size_bytes);
    appMemCacheWb(zigzag_output_virt, quant_size_bytes);
    appMemCacheWb(rle_output_virt, rle_size_bytes);
    appMemCacheWb(huff_output_virt, huff_capacity_bytes);

    /* -------------------------------------------------------------------------
     * Prepare DTO for IPC
     * -------------------------------------------------------------------------
     */
    appLogPrintf("JPEG: Preparing data for DSP...\n");
    JPEG_COMPRESSION_DTO dto;
    memset(&dto, 0, sizeof(dto));
    
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
    
    dto.rle_phy_ptr = appMemGetVirt2PhyBufPtr((uint64_t)rle_output_virt, APP_MEM_HEAP_DDR);
    dto.rle_count = 0; 

    // Huffman Setup
    dto.huff_phy_ptr = appMemGetVirt2PhyBufPtr((uint64_t)huff_output_virt, APP_MEM_HEAP_DDR);
    dto.huff_size = 0;

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
        appLogPrintf("JPEG: Error sending data/executing on DSP! Status: %d\n", status);
    } 
    else 
    {
        appLogPrintf("JPEG: DSP Processing Done! Verifying results...\n");
        
        /* -------------------------------------------------------------------------
         * Invalidate Cache & Verify Results
         * -------------------------------------------------------------------------
         */

        appMemCacheInv(y_output_virt, pixel_count);
        appMemCacheInv(dct_output_virt, dct_size_bytes);
        appMemCacheInv(quant_output_virt, quant_size_bytes);
        appMemCacheInv(zigzag_output_virt, quant_size_bytes);
        appMemCacheInv(rle_output_virt, rle_size_bytes);
        // Important: Invalidate Huffman buffer to see DSP results
        appMemCacheInv(huff_output_virt, huff_capacity_bytes); 

        // --- Verify Y Component ---
        appLogPrintf("\n--------------------------------------------------\n");
        appLogPrintf("1. Y Component (First 8x8 Block - Linear View):\n");
        appLogPrintf("--------------------------------------------------\n");
        int8_t* signed_y = (int8_t*)y_output_virt;
        for (int i = 0; i < 64 && i < pixel_count; i++) {
            if (i % 8 == 0) printf("\nRow %d: ", i/8);
            printf("%4d ", signed_y[i]); 
        }
        printf("\n");

        // --- Verify DCT Coefficients ---
        appLogPrintf("\n--------------------------------------------------\n");
        appLogPrintf("2. DCT Coefficients (First 8x8 Block):\n");
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
        appLogPrintf("3. Quantized Coefficients (First 8x8 Block):\n");
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
        appLogPrintf("4. Zig-Zag Output (First 8x8 Block - Linearized):\n");
        appLogPrintf("--------------------------------------------------\n");
        for (int i = 0; i < 64; i++) {
            if (i % 8 == 0) printf("\nLine %d: ", i/8);
            printf("%4d ", zigzag_output_virt[i]);
        }
        printf("\n");

        // --- Verify RLE Output ---
        appLogPrintf("\n--------------------------------------------------\n");
        appLogPrintf("5. RLE Output (Total Symbols Generated: %d)\n", dto.rle_count);
        appLogPrintf("--------------------------------------------------\n");
        
        if (dto.rle_count == 0) {
            appLogPrintf("WARNING: RLE count is 0. Something went wrong on DSP.\n");
        } else {
            int limit = (dto.rle_count < 20) ? dto.rle_count : 20;
            for (int i = 0; i < limit; i++) {
                RLESymbol s = rle_output_virt[i];
                uint8_t run = s.symbol >> 4;
                uint8_t size = s.symbol & 0x0F;
                printf("[%3d] Sym:0x%02X (Run:%2d, Size:%2d) Code:%5d (Bits:%d)\n", 
                       i, s.symbol, run, size, s.code, s.codeBits);
            }
        }
        printf("\n");

        // --- Verify Huffman Output ---
        appLogPrintf("\n--------------------------------------------------\n");
        appLogPrintf("6. HUFFMAN Output (FINAL BITSTREAM)\n");
        appLogPrintf("--------------------------------------------------\n");
        appLogPrintf("Original Size   : %d bytes (Y-Grayscale)\n", pixel_count);
        appLogPrintf("Compressed Size : %d bytes\n", dto.huff_size);
        
        if(dto.huff_size > 0) {
            float ratio = (float)pixel_count / (float)dto.huff_size;
            appLogPrintf("Compression Ratio: %.2f : 1\n", ratio);
            
            appLogPrintf("\nHex Dump of Compressed Data (First 64 bytes):\n");
            for (int i = 0; i < 64 && i < dto.huff_size; i++) {
                if (i % 16 == 0) printf("\n%04X: ", i);
                printf("%02X ", huff_output_virt[i]);
            }
            printf("\n\n");
            
            // Check for valid JPEG markers (just heuristics)
            // A raw bitstream usually needs headers (SOI, DQT, DHT, SOF, SOS) to be a valid .jpg file.
            // This DSP code outputs the *Entropy Coded Segment* (scan data).
            // Usually valid scan data doesn't start with specific magic bytes unless headers are added.
            appLogPrintf("Note: This contains the Entropy Coded Segment (SOS data).\n");
            appLogPrintf("To view as .jpg, headers (SOI, DQT, DHT, SOF, SOS) must be prepended.\n");
        } else {
            appLogPrintf("ERROR: Huffman size is 0!\n");
        }
    }

    // Cleanup resources
    appLogPrintf("JPEG: Cleaning up...\n");
    
    if(y_output_virt) appMemFree(APP_MEM_HEAP_DDR, y_output_virt, pixel_count);
    if(dct_output_virt) appMemFree(APP_MEM_HEAP_DDR, dct_output_virt, dct_size_bytes);
    if(quant_output_virt) appMemFree(APP_MEM_HEAP_DDR, quant_output_virt, quant_size_bytes);
    if(zigzag_output_virt) appMemFree(APP_MEM_HEAP_DDR, zigzag_output_virt, quant_size_bytes);
    if(rle_output_virt) appMemFree(APP_MEM_HEAP_DDR, rle_output_virt, rle_size_bytes);
    if(huff_output_virt) appMemFree(APP_MEM_HEAP_DDR, huff_output_virt, huff_capacity_bytes);
    
    freeBMPImage(img);
    appDeInit();

    return 0;
}