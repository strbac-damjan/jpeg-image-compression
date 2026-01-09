# jpeg-image-compression

This project is a custom C implementation of the JPEG image compression standard. It reads raw bitmap (BMP) images, processes them using JPEG compression algorithms (DCT, Quantization, Huffman coding), and outputs the result.

## Project structure

Here is an overview of the repository organization and the purpose of each directory:

```text
.
├── assets
│   ├── input
│   └── output
├── dsp_port
│   └── jpeg_compression
│       ├── include
│       └── src
├── LICENSE
├── natural_c
│   ├── build
│   ├── include
│   ├── Makefile
│   └── src
│       ├── core
│       ├── io
│       └── main.c
└── README.md
```

## How to run natural C version
1. Run the command `make` which will build the project. This will generate `jpeg_compression_app` in the build folder.
2. Run the binary like `./build/jpeg_compression_app {path to input image} {path to output image}`

## How to run the DSP version

1. Download ti-processor-sdk-rtos-j721e-evm-09_02_00_05
2. Update the file `vision_apps/Makefile` to include:
```
JPEG_REPO_PATH := /home/damjans/Desktop/SDOS/Projekat/jpeg-image-compression/dsp_port
export JPEG_REPO_PATH

.
.
.

DIRECTORIES :=
DIRECTORIES += $(JPEG_REPO_PATH)
DIRECTORIES += $(JPEG_REPO_PATH)/jpeg_client
```
Here you should put your own path to the project folder as the JPEG_REPO_PATH

3. Update the file `vision_apps/platform/j721e/rtos/common/concerto.mak` to include:
```
ifeq ($(BUILD_CPU_C7x_1),yes)
ifeq ($(TARGET_CPU),C71)

# CPU_ID must be set before include $(PRELUDE)
CPU_ID=c7x_1

_MODULE=$(CPU_ID)
include $(PRELUDE)

TARGET      := app_rtos_common_$(CPU_ID)
TARGETTYPE  := library
CSOURCES    := $(call all-c-files)

DEFS+=APP_CFG_FILE=\"app_cfg_$(CPU_ID).h\"
DEFS+=CPU_$(CPU_ID)

IDIRS+=$(VISION_APPS_PATH)/platform/$(SOC)/rtos
IDIRS += $(JPEG_REPO_PATH)/jpeg_compression/include


include $(VISION_APPS_PATH)/platform/$(SOC)/rtos/concerto_c7x_inc.mak

include $(FINALE)

endif
endif
```

4. Update the `vision_apps/platform/j721e/rtos/common/app_init.c` file to include:
```c
#if defined(CPU_c7x_1)
#include <jpeg_compression.h>
#endif

.
.
.

int32_t appInit() 
{
    .
    .
    .
    #ifdef CPU_c7x_1
    status = JpegCompression_Init();
    APP_ASSERT_SUCCESS(status);
    #endif
    
}
```

5. Update the `vision_apps/platform/j721e/rtos/concerto_c7x_inc.mak` to include:
`STATIC_LIBS += app_utils_jpeg_compression`

7. Connect to the board then run the `dsp_port/full_build.sh` script to transfer the binaries to the board

8. Run `ssh root@192.168.1.200` to connect to the board, then go to `/opt/vision_apps/`

9. Run `./jpeg_client_app.out` to start the program.