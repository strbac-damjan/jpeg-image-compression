$(warning [DEBUG] concerto.mak file in dsp_port/jpeg_compression/src/concerto.mak is found!)

include $(PRELUDE)

TARGET      := app_utils_jpeg_compression
TARGETTYPE  := library

# Add your source file
CSOURCES    := jpeg_compression.c converter.c dct.c quantization.c zigzag.c rle.c

# Include paths for your headers
# IDIRS       += $(VISION_APPS_PATH)/utils/jpeg_compression/include
IDIRS 		+= $(JPEG_REPO_PATH)/jpeg_compression/include
IDIRS       += $(VISION_APPS_PATH)/utils/console_io/include
IDIRS       += $(VISION_APPS_PATH)/utils/remote_service/include
IDIRS       += $(VISION_APPS_PATH)/utils/mem/include
IDIRS       += $(VISION_APPS_PATH)/utils/ipc/include
IDIRS       += $(PSDKR_PATH)/psdk_tools/ti-cgt-c7000_4.1.0.LTS/include

include $(FINALE)
