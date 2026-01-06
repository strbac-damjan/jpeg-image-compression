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

#define JPEG_COMPRESSION_REMOTE_SERVICE_NAME "com.etfbl.sdos.jpeg_compression"

int32_t JpegCompression_RemoteServiceHandler(char *service_name, uint32_t cmd,
void *prm, uint32_t prm_size, uint32_t flags);

int32_t JpegCompression_Init();

#endif
