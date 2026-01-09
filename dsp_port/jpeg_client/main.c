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

#include <jpeg_compression.h> 
#include "bmp_handler.h"

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

    // Physical address of the output buffer (DCT Coefficients - final)
    uint64_t dct_phy_ptr;
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
     * Allocate output buffers (Y Image & DCT Image) in Shared Memory
     * -------------------------------------------------------------------------
     */
    
    // Allocate Y Component Buffer (1 byte per pixel)
    // Alignment to 64 bytes is recommended for C7x cache line / vector optimization.
    uint8_t* y_output_virt = (uint8_t*)appMemAlloc(APP_MEM_HEAP_DDR, pixel_count, 64);
    
    // Allocate DCT Coefficients Buffer (4 bytes per pixel - float)
    uint32_t dct_size_bytes = pixel_count * sizeof(float);
    float* dct_output_virt = (float*)appMemAlloc(APP_MEM_HEAP_DDR, dct_size_bytes, 64);
    
    if (!y_output_virt || !dct_output_virt) {
        appLogPrintf("JPEG: Failed to allocate output memory!\n");
        if(y_output_virt) appMemFree(APP_MEM_HEAP_DDR, y_output_virt, pixel_count);
        if(dct_output_virt) appMemFree(APP_MEM_HEAP_DDR, dct_output_virt, dct_size_bytes);
        freeBMPImage(img);
        appDeInit();
        return 1;
    }

    // Initialize buffers to zero to ensure clean state
    memset(y_output_virt, 0, pixel_count);
    memset(dct_output_virt, 0, dct_size_bytes);

    // Perform Cache Write-back.
    // This pushes the zeros from the CPU cache to the physical DDR RAM 
    // so the DSP sees the initialized state.
    appMemCacheWb(y_output_virt, pixel_count);
    appMemCacheWb(dct_output_virt, dct_size_bytes);

    /* -------------------------------------------------------------------------
     * Prepare DTO for IPC
     * -------------------------------------------------------------------------
     */
    appLogPrintf("JPEG: Preparing data for DSP...\n");
    JPEG_COMPRESSION_DTO dto;
    
    dto.width = img->width;
    dto.height = img->height;

    // Convert Virtual addresses (viewable by A72) to Physical addresses (required by C7x)
    dto.r_phy_ptr = appMemGetVirt2PhyBufPtr((uint64_t)img->r, APP_MEM_HEAP_DDR);
    dto.g_phy_ptr = appMemGetVirt2PhyBufPtr((uint64_t)img->g, APP_MEM_HEAP_DDR);
    dto.b_phy_ptr = appMemGetVirt2PhyBufPtr((uint64_t)img->b, APP_MEM_HEAP_DDR);
    
    dto.y_phy_ptr = appMemGetVirt2PhyBufPtr((uint64_t)y_output_virt, APP_MEM_HEAP_DDR);
    dto.dct_phy_ptr = appMemGetVirt2PhyBufPtr((uint64_t)dct_output_virt, APP_MEM_HEAP_DDR);

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

        // Invalidate A72 cache for both buffers.
        // The DSP wrote directly to physical RAM. We must invalidate our local cache
        // to force a reload from DDR, otherwise we might read stale data (zeros).
        appMemCacheInv(y_output_virt, pixel_count);
        appMemCacheInv(dct_output_virt, dct_size_bytes);

        // Verify Y Component
        appLogPrintf("\n--------------------------------------------------\n");
        appLogPrintf("Y Component (First 8x8 Block - Linear View):\n");
        appLogPrintf("--------------------------------------------------\n");
        
        int8_t* signed_y = (int8_t*)y_output_virt;
        // Printing the first 64 bytes linearly for a quick check
        for (int i = 0; i < 64 && i < pixel_count; i++) {
            if (i % 8 == 0) printf("\nRow %d: ", i/8);
            printf("%4d ", signed_y[i]); 
        }
        printf("\n");

        // Verify DCT Coefficients
        appLogPrintf("\n--------------------------------------------------\n");
        appLogPrintf("DCT Coefficients (First 8x8 Block):\n");
        appLogPrintf("--------------------------------------------------\n");
        
        // Since the image is stored in raster scan order (linear), 
        // a single 8x8 block is not contiguous in memory.
        // We must stride by 'width' to access the same column in the next row.
        for (int row = 0; row < 8; row++) {
            printf("\nRow %d: ", row);
            for (int col = 0; col < 8; col++) {
                int idx = row * img->width + col;
                if (idx < pixel_count) {
                    printf("%7.2f ", dct_output_virt[idx]);
                }
            }
        }
        printf("\n\n");
        
        // Simple heuristic check:
        // The DC coefficient (top-left) usually has the largest magnitude.
        if (dct_output_virt[0] == 0.0f && dct_output_virt[1] == 0.0f) {
            appLogPrintf("WARNING: DCT output seems to be all zeros. Check DSP logic.\n");
        } else {
            appLogPrintf("SUCCESS: DCT Data detected.\n");
        }
    }

    // Cleanup resources
    appLogPrintf("JPEG: Cleaning up...\n");
    
    appMemFree(APP_MEM_HEAP_DDR, y_output_virt, pixel_count);
    appMemFree(APP_MEM_HEAP_DDR, dct_output_virt, dct_size_bytes);
    freeBMPImage(img);
    appDeInit();

    return 0;
}
