#include <stdio.h>
#include <string.h>
#include <TI/tivx.h>
#include <utils/ipc/include/app_ipc.h>
#include <utils/remote_service/include/app_remote_service.h>
#include <utils/app_init/include/app_init.h>
#include <utils/console_io/include/app_log.h>

#include <jpeg_compression.h> 

#include "bmp_handler.h"

int main(int argc, char* argv[])
{
    const char* inputPath = argv[1];
    const char* outputPath = argv[2];

    BMPImage* img = loadBMPImage(inputPath);

    if(img) {
        saveBMPImage(outputPath, img);
    }
    return 1;
}