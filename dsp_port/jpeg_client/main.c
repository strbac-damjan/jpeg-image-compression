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
// The DSP cannot dereference pointers pointing to the A72 heap.
// Therefore, we send physical addresses (uint64_t) and dimensions directly.
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
    const char* inputPath = argv[1];
    
    // App Initialization (Must be done first!)
    // This is critical because loadBMPImage now uses appMemAlloc, 
    // and appMemAlloc requires appInit() to be executed beforehand.
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
    // Inside loadBMPImage, appMemAlloc(..., 64) is now used, 
    // so img->r, img->g, img->b are already in shared memory and aligned.
    BMPImage* img = loadBMPImage(inputPath);

    if(!img) {
        appLogPrintf("JPEG: Image loading failed!\n");
        appDeInit(); // Clean up
        return 1;
    }
    appLogPrintf("JPEG: BMP image loaded (Width: %d, Height: %d)!\n", img->width, img->height);

    // Allocate output buffer (Y Image) in Shared Memory
    // The DSP needs a place to write the result, so this buffer must also be in shared memory.
    uint32_t pixel_count = img->width * img->height;
    uint8_t* y_output_virt = (uint8_t*)appMemAlloc(APP_MEM_HEAP_DDR, pixel_count, 64);
    
    if (!y_output_virt) {
        appLogPrintf("JPEG: Failed to allocate output memory!\n");
        freeBMPImage(img);
        appDeInit();
        return 1;
    }

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
    appLogPrintf("JPEG: Sending data via Remote Service!\n");
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
        appLogPrintf("JPEG: Communication successful! Processing done.\n");
        
        // Here we can check the result in y_output_virt
        // Since the memory is mapped, the A72 immediately sees what the DSP wrote.
        // printf("First pixel of Y image: %d\n", y_output_virt[0]);
    }

    // Cleanup
    appLogPrintf("JPEG: Cleaning up...\n");
    
    // Free Y buffer
    appMemFree(APP_MEM_HEAP_DDR, y_output_virt, pixel_count);
    
    // Free BMP (this internally calls appMemFree for channels)
    freeBMPImage(img);

    appDeInit();

    return 0;
}