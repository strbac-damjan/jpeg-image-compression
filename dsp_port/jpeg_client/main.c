#include <stdio.h>
#include <string.h>
#include <TI/tivx.h>
#include <utils/ipc/include/app_ipc.h>
#include <utils/remote_service/include/app_remote_service.h>
#include <utils/app_init/include/app_init.h>
#include <utils/console_io/include/app_log.h>

// Uključiš svoj zajednički header jer si dodao IDIRS u makefile
#include <jpeg_compression.h> 
// Ili <jpeg_compression_common.h> ako si napravio novi fajl

int main()
{
    printf("Hello world");
    return 1;
}