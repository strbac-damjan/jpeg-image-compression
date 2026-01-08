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

// Flattened DTO structure for IPC
typedef struct JPEG_COMPRESSION_DTO
{
    int32_t width;
    int32_t height;
    
    // Physical addresses of input channels (R, G, B)
    uint64_t r_phy_ptr;
    uint64_t g_phy_ptr;
    uint64_t b_phy_ptr;

    // Physical address of the output buffer (Y)
    uint64_t y_phy_ptr;
} JPEG_COMPRESSION_DTO;

int main(int argc, char* argv[])
{
    int32_t status;
    
    if (argc < 2) {
        appLogPrintf("Usage: %s <input_bmp_path>\n", argv[0]);
        return -1;
    }
    const char* inputPath = argv[1];
    
    // 1. App Initialization
    appLogPrintf("JPEG: Initializing App...\n");
    status = appInit();
    if(status != 0)
    {
        appLogPrintf("JPEG: App initialization failed!\n");
        return 1;
    }
    appLogPrintf("JPEG: App initialization successful!\n");

    // Load BMP image (Uses appMemAlloc internally)
    appLogPrintf("JPEG: Loading BMP image...\n");
    BMPImage* img = loadBMPImage(inputPath);

    if(!img) {
        appLogPrintf("JPEG: Image loading failed!\n");
        appDeInit(); 
        return 1;
    }
    appLogPrintf("JPEG: BMP image loaded (Width: %d, Height: %d)!\n", img->width, img->height);

    // Allocate output buffer (Y Image) in Shared Memory
    uint32_t pixel_count = img->width * img->height;
    // Align to 64 bytes (cache line size / vector size)
    uint8_t* y_output_virt = (uint8_t*)appMemAlloc(APP_MEM_HEAP_DDR, pixel_count, 64);
    
    if (!y_output_virt) {
        appLogPrintf("JPEG: Failed to allocate output memory!\n");
        freeBMPImage(img);
        appDeInit();
        return 1;
    }

    // Inicijalizacija izlaznog bafera na 0 (da budemo sigurni da vidimo promjenu)
    memset(y_output_virt, 0, pixel_count);
    // Write-back keša da nule odu u RAM prije nego DSP krene
    appMemCacheWb(y_output_virt, pixel_count); 

    // Prepare DTO for sending
    appLogPrintf("JPEG: Preparing data for DSP...\n");
    JPEG_COMPRESSION_DTO dto;
    
    dto.width = img->width;
    dto.height = img->height;

    // Convert Virtual addresses (A72) to Physical addresses (for C7x)
    dto.r_phy_ptr = appMemGetVirt2PhyBufPtr((uint64_t)img->r, APP_MEM_HEAP_DDR);
    dto.g_phy_ptr = appMemGetVirt2PhyBufPtr((uint64_t)img->g, APP_MEM_HEAP_DDR);
    dto.b_phy_ptr = appMemGetVirt2PhyBufPtr((uint64_t)img->b, APP_MEM_HEAP_DDR);
    
    dto.y_phy_ptr = appMemGetVirt2PhyBufPtr((uint64_t)y_output_virt, APP_MEM_HEAP_DDR);

    // Send data via Remote Service
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
        
        // Since the DSP wrote directly to DDR memory, the A72 cache might still contain 
        // stale data (the zeros we initialized earlier).
        // We must invalidate the A72 cache to force a fresh read from physical RAM.
        appMemCacheInv(y_output_virt, pixel_count);

        // Print the first 64 pixels to verify the output
        // (This gives us a quick look at the first few processed vectors)
        appLogPrintf("--------------------------------------------------\n");
        appLogPrintf("Y Component (First 64 pixels):\n");
        appLogPrintf("--------------------------------------------------\n");
        
        for (int i = 0; i < 64 && i < pixel_count; i++) {
            // Format the output into rows of 16 values for readability
            if (i % 16 == 0) printf("\n[%04d]: ", i);
            int8_t* signed_y = (int8_t*)y_output_virt;
            printf("%3d ", signed_y[i]); // Ovo ce ispisati brojeve od -128 do 127
        }
        printf("\n\n");
        
        // Quick heuristic check: If the values are still 0, the DSP likely didn't write anything.
        if (y_output_virt[0] == 0 && y_output_virt[10] == 0 && y_output_virt[20] == 0) {
            appLogPrintf("WARNING: Output seems to be all zeros. Check DSP logic.\n");
        } else {
            appLogPrintf("SUCCESS: Data detected in output buffer.\n");
        }
    }

    // Cleanup
    appLogPrintf("JPEG: Cleaning up...\n");
    
    appMemFree(APP_MEM_HEAP_DDR, y_output_virt, pixel_count);
    freeBMPImage(img);
    appDeInit();

    return 0;
}
