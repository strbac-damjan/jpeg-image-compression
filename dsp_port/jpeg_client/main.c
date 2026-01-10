#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h> 
#include <TI/tivx.h>
#include <utils/ipc/include/app_ipc.h>
#include <utils/remote_service/include/app_remote_service.h>
#include <utils/app_init/include/app_init.h>
#include <utils/console_io/include/app_log.h>
#include <utils/mem/include/app_mem.h> 

#include <jpeg_compression.h> 
#include "bmp_handler.h"
#include "jpeg_handler.h" 

// Definition must match DSP side
typedef struct {
    uint8_t symbol;    
    uint8_t codeBits;  
    uint16_t code;     
} RLESymbol;

/* * DTO structure shared between A72 (Host) and C7x (DSP). 
 * UPDATED: Includes Profiling Data
 */
typedef struct JPEG_COMPRESSION_DTO
{
    int32_t width;
    int32_t height;
    
    // Inputs (R, G, B)
    uint64_t r_phy_ptr;
    uint64_t g_phy_ptr;
    uint64_t b_phy_ptr;

    // Intermediate buffers
    uint64_t y_phy_ptr;
    uint64_t dct_phy_ptr;
    uint64_t quant_phy_ptr;
    uint64_t zigzag_phy_ptr;

    // Output RLE
    uint64_t rle_phy_ptr;
    uint32_t rle_count; 

    // Output Huffman (Final Bitstream)
    uint64_t huff_phy_ptr;
    uint32_t huff_size; 

    // --- PROFILING DATA (Cycles) ---
    uint64_t cycles_color_conversion;
    uint64_t cycles_dct;
    uint64_t cycles_quantization;
    uint64_t cycles_zigzag;
    uint64_t cycles_rle;
    uint64_t cycles_huffman;
    uint64_t cycles_total;

} JPEG_COMPRESSION_DTO;

// Helper function to align dimensions to next multiple of 32 (Macro Block Width)
int32_t align32(int32_t x) {
    return (x + 31) & ~31;
}

int32_t align8(int32_t x) {
    return (x + 7) & ~7;
}
#include <stdio.h>
#include <string.h>

/**
 * \brief Helper to format numbers with space separators (e.g. 1000000 -> "1 000 000")
 */
void format_cycles(uint64_t num, char* out_buf) {
    char temp[32];
    sprintf(temp, "%llu", (unsigned long long)num); // %llu for 64-bit safety
    int len = strlen(temp);
    int i, j = 0;

    for (i = 0; i < len; i++) {
        out_buf[j++] = temp[i];
        // Add space if remaining digits are divisible by 3, but not at the end
        if ((len - 1 - i) > 0 && (len - 1 - i) % 3 == 0) {
            out_buf[j++] = ' ';
        }
    }
    out_buf[j] = '\0';
}

/**
 * \brief Prints performance statistics (CYCLES ONLY)
 */
void print_profiling_stats(JPEG_COMPRESSION_DTO *dto) 
{
    char buf[32]; // Buffer for formatted numbers

    printf("==========================================\n");
    printf("   DSP C7x PROFILING REPORT (Cycles)      \n");
    printf("==========================================\n");
    printf("Image Resolution : %dx%d\n", dto->width, dto->height);
    printf("------------------------------------------\n");
    printf("Step               Cycles                 \n");
    printf("------------------------------------------\n");
    
    format_cycles(dto->cycles_color_conversion, buf);
    printf("Color Conversion : %15s\n", buf);
           
    format_cycles(dto->cycles_dct, buf);
    printf("DCT              : %15s\n", buf);
           
    format_cycles(dto->cycles_quantization, buf);
    printf("Quantization     : %15s\n", buf);
           
    format_cycles(dto->cycles_zigzag, buf);
    printf("ZigZag           : %15s\n", buf);
           
    format_cycles(dto->cycles_rle, buf);
    printf("RLE              : %15s\n", buf);
           
    format_cycles(dto->cycles_huffman, buf);
    printf("Huffman          : %15s\n", buf);
           
    printf("------------------------------------------\n");
    
    format_cycles(dto->cycles_total, buf);
    printf("TOTAL CYCLES     : %15s\n", buf);
    printf("==========================================\n\n");
}

// Helper to print 8x8 block
void print_debug_block(const char* name, void* data, int type) {
    printf("\n--- DEBUG BLOCK: %s ---\n", name);
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            int idx = i * 8 + j;
            if (type == 0) { // uint8 (Y)
                printf("%3d ", ((uint8_t*)data)[idx]);
            } else if (type == 1) { // float (DCT)
                printf("%6.1f ", ((float*)data)[idx]);
            } else if (type == 2) { // int16 (Quant/ZigZag)
                printf("%4d ", ((int16_t*)data)[idx]);
            }
        }
        printf("\n");
    }
    printf("--------------------------\n");
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
    if(status != 0) {
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
    
    // 1. CALCULATE ALIGNED DIMENSIONS
    int32_t orig_w = img->width;
    int32_t orig_h = img->height;
    int32_t aligned_w = align32(orig_w); // Bilo je align8
    int32_t aligned_h = align8(orig_h);  // Visina moze ostati na 8
    uint32_t aligned_pixel_count = aligned_w * aligned_h;
    
    appLogPrintf("JPEG: Loaded %dx%d -> Padded to %dx%d\n", orig_w, orig_h, aligned_w, aligned_h);

    // 2. ALLOCATE INPUT BUFFERS
    uint8_t* r_padded = (uint8_t*)appMemAlloc(APP_MEM_HEAP_DDR, aligned_pixel_count, 64);
    uint8_t* g_padded = (uint8_t*)appMemAlloc(APP_MEM_HEAP_DDR, aligned_pixel_count, 64);
    uint8_t* b_padded = (uint8_t*)appMemAlloc(APP_MEM_HEAP_DDR, aligned_pixel_count, 64);

    if (!r_padded || !g_padded || !b_padded) {
        appLogPrintf("JPEG: Failed to allocate input buffers!\n");
        freeBMPImage(img);
        appDeInit();
        return 1;
    }

    // Copy data with Edge Extension
    for (int y = 0; y < aligned_h; y++) {
        int src_y = (y < orig_h) ? y : (orig_h - 1);
        for (int x = 0; x < aligned_w; x++) {
            int src_x = (x < orig_w) ? x : (orig_w - 1);
            int src_idx = src_y * orig_w + src_x;
            int dst_idx = y * aligned_w + x;
            
            r_padded[dst_idx] = img->r[src_idx];
            g_padded[dst_idx] = img->g[src_idx];
            b_padded[dst_idx] = img->b[src_idx];
        }
    }

    appMemCacheWb(r_padded, aligned_pixel_count);
    appMemCacheWb(g_padded, aligned_pixel_count);
    appMemCacheWb(b_padded, aligned_pixel_count);

    // 3. ALLOCATE OUTPUT BUFFERS
    uint8_t* y_output_virt = (uint8_t*)appMemAlloc(APP_MEM_HEAP_DDR, aligned_pixel_count, 64);
    uint32_t dct_size_bytes = aligned_pixel_count * sizeof(float);
    float* dct_output_virt = (float*)appMemAlloc(APP_MEM_HEAP_DDR, dct_size_bytes, 64);
    uint32_t quant_size_bytes = aligned_pixel_count * sizeof(int16_t);
    int16_t* quant_output_virt = (int16_t*)appMemAlloc(APP_MEM_HEAP_DDR, quant_size_bytes, 64);
    int16_t* zigzag_output_virt = (int16_t*)appMemAlloc(APP_MEM_HEAP_DDR, quant_size_bytes, 64);
    uint32_t rle_size_bytes = aligned_pixel_count * sizeof(RLESymbol);
    RLESymbol* rle_output_virt = (RLESymbol*)appMemAlloc(APP_MEM_HEAP_DDR, rle_size_bytes, 64);
    uint32_t huff_capacity_bytes = aligned_pixel_count; 
    uint8_t* huff_output_virt = (uint8_t*)appMemAlloc(APP_MEM_HEAP_DDR, huff_capacity_bytes, 64);

    if (!y_output_virt || !dct_output_virt || !quant_output_virt || !zigzag_output_virt || !rle_output_virt || !huff_output_virt) {
        appLogPrintf("JPEG: Failed to allocate output memory!\n");
        // Simple cleanup for brevity (in production, free all allocated)
        freeBMPImage(img);
        appDeInit();
        return 1;
    }

    // Clear and Flush Output Buffers
    memset(y_output_virt, 0, aligned_pixel_count);
    appMemCacheWb(y_output_virt, aligned_pixel_count);
    // (Other buffers are write-only by DSP, but good practice to clear/flush if needed)

    // 4. PREPARE DTO
    JPEG_COMPRESSION_DTO dto;
    memset(&dto, 0, sizeof(dto));
    
    dto.width = aligned_w;
    dto.height = aligned_h;

    dto.r_phy_ptr = appMemGetVirt2PhyBufPtr((uint64_t)r_padded, APP_MEM_HEAP_DDR);
    dto.g_phy_ptr = appMemGetVirt2PhyBufPtr((uint64_t)g_padded, APP_MEM_HEAP_DDR);
    dto.b_phy_ptr = appMemGetVirt2PhyBufPtr((uint64_t)b_padded, APP_MEM_HEAP_DDR);
    dto.y_phy_ptr = appMemGetVirt2PhyBufPtr((uint64_t)y_output_virt, APP_MEM_HEAP_DDR);
    dto.dct_phy_ptr = appMemGetVirt2PhyBufPtr((uint64_t)dct_output_virt, APP_MEM_HEAP_DDR);
    dto.quant_phy_ptr = appMemGetVirt2PhyBufPtr((uint64_t)quant_output_virt, APP_MEM_HEAP_DDR);
    dto.zigzag_phy_ptr = appMemGetVirt2PhyBufPtr((uint64_t)zigzag_output_virt, APP_MEM_HEAP_DDR);
    dto.rle_phy_ptr = appMemGetVirt2PhyBufPtr((uint64_t)rle_output_virt, APP_MEM_HEAP_DDR);
    dto.huff_phy_ptr = appMemGetVirt2PhyBufPtr((uint64_t)huff_output_virt, APP_MEM_HEAP_DDR);

    // 5. RUN DSP PROCESS
    appLogPrintf("JPEG: Sending data to DSP...\n");
    status = appRemoteServiceRun(
        APP_IPC_CPU_C7x_1,
        "com.etfbl.sdos.jpeg_compression",
        0, 
        &dto,
        sizeof(dto),
        0
    );

    if(status != 0) 
    {
        appLogPrintf("JPEG: DSP Execution Failed! Status: %d\n", status);
    } 
    else 
    {
        // ----------------------------------------------------
        // SUCCESS: PRINT PROFILING STATS AND DEBUG INFO OF FIRST BLOCK
        // ----------------------------------------------------

        // Invalidate caches to see results
        appMemCacheInv(y_output_virt, 64);
        appMemCacheInv(dct_output_virt, 64 * sizeof(float));
        appMemCacheInv(quant_output_virt, 64 * sizeof(int16_t));
        appMemCacheInv(zigzag_output_virt, 64 * sizeof(int16_t));

        appLogPrintf("\n=== DEBUGGING FIRST BLOCK (0,0) ===\n");
        
        // Type 0=uint8, 1=float, 2=int16
        print_debug_block("Y Input (shifted back +128)", y_output_virt, 0);
        print_debug_block("DCT Coefficients", dct_output_virt, 1);
        print_debug_block("Quantized", quant_output_virt, 2);
        print_debug_block("ZigZag Ordered", zigzag_output_virt, 2);
        print_profiling_stats(&dto);

        // Verify & Save
        appLogPrintf("JPEG: Compressed Size: %d bytes\n", dto.huff_size);
        
        if(dto.huff_size > 0) {
            bool saved = saveJPEG(outputPath, aligned_w, orig_h, huff_output_virt, dto.huff_size);
            if (saved) {
                appLogPrintf("SUCCESS: Saved to %s\n", outputPath);
            } else {
                appLogPrintf("ERROR: Failed to save file!\n");
            }
        } else {
            appLogPrintf("ERROR: DSP returned 0 bytes!\n");
        }
    }

    

    // Cleanup
    appLogPrintf("JPEG: Cleaning up...\n");
    appMemFree(APP_MEM_HEAP_DDR, r_padded, aligned_pixel_count);
    appMemFree(APP_MEM_HEAP_DDR, g_padded, aligned_pixel_count);
    appMemFree(APP_MEM_HEAP_DDR, b_padded, aligned_pixel_count);
    appMemFree(APP_MEM_HEAP_DDR, y_output_virt, aligned_pixel_count);
    appMemFree(APP_MEM_HEAP_DDR, dct_output_virt, dct_size_bytes);
    appMemFree(APP_MEM_HEAP_DDR, quant_output_virt, quant_size_bytes);
    appMemFree(APP_MEM_HEAP_DDR, zigzag_output_virt, quant_size_bytes);
    appMemFree(APP_MEM_HEAP_DDR, rle_output_virt, rle_size_bytes);
    appMemFree(APP_MEM_HEAP_DDR, huff_output_virt, huff_capacity_bytes);
    
    freeBMPImage(img);
    appDeInit();

    return 0;
}