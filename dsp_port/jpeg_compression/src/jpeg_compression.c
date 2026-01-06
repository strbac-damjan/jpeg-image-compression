#include <jpeg_compression.h>

int32_t JpegCompression_RemoteServiceHandler(char *service_name, uint32_t cmd, void *prm, uint32_t prm_size, uint32_t flags)
{
    return 0;
}
int32_t JpegCompression_Init()
{
    int32_t status = -1;
    appLogPrintf("JPEG Compression: Init ... !!!");
    status = appRemoteServiceRegister(JPEG_COMPRESSION_REMOTE_SERVICE_NAME, JpegCompression_RemoteServiceHandler);
    if(status != 0)
    {
        appLogPrintf("JPEG Compression: ERROR: Unable to register remote service handler\n");
    }
    appLogPrintf("JPEG Compression: Init ... DONE!!!\n");
    return status;
}
