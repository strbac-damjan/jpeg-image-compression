#ifdef __C7000__
#ifndef JPEG_COMPRESSION_H
#define JPEG_COMPRESSION_H

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <TI/tivx.h>
#include <utils/ipc/include/app_ipc.h>
#include <utils/remote_service/include/app_remote_service.h>
//#include <utils/app_init/include/app_init.h>
#include <utils/console_io/include/app_log.h>

#include <c7x.h>


#define JPEG_COMPRESSION_REMOTE_SERVICE_NAME "com.etfbl.sdos.jpeg_compression"

// -------------------------------------------------------------------------------------
// ---------------------------STRUCTURE DEFINITIONS-------------------------------------
// -------------------------------------------------------------------------------------
typedef struct BMPImage{
    int32_t width;
    int32_t height;
    uint8_t* data;  // Pixel data (RGB format). Must be freed manually.
} BMPImage;

typedef struct {
    int width;           // Image width
    int height;          // Image height
    uchar64 *data;       // Pointer to the pixel array (length = width * height)
                         // Values 0-255 (where 0=black, 255=white)
} YImage;


typedef struct JPEG_COMPRESSION_DTO
{
    BMPImage* bmpImage;

    YImage* yImage;
}JPEG_COMPRESSION_DTO;

// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------

// -------------------------------------------------------------------------------------
// --------------------------TI SERVICE FUNCTIONS---------------------------------------
// -------------------------------------------------------------------------------------

// Remote service handler
int32_t JpegCompression_RemoteServiceHandler(char *service_name, uint32_t cmd,
void *prm, uint32_t prm_size, uint32_t flags);

// Service initialization function
int32_t JpegCompression_Init();
// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------

// -------------------------------------------------------------------------------------
// ----------------------- JPEG COMPRESSION FUNCTIONS-----------------------------------
// -------------------------------------------------------------------------------------
int32_t convertToJpeg(JPEG_COMPRESSION_DTO* dto);
YImage* extractYComponent(BMPImage *img);
// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------
#endif

#endif
