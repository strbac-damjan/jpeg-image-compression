
include $(PRELUDE)

TARGET      := app_utils_jpeg_compression
TARGETTYPE  := library

# Add your source file
CSOURCES    := jpeg_compression.c

# Include paths for your headers
# IDIRS       += $(VISION_APPS_PATH)/utils/jpeg_compression/include
IDIRS 		+= $(JPEG_REPO_PATH)/jpeg_compression/include
IDIRS       += $(VISION_APPS_PATH)/utils/console_io/include
IDIRS       += $(VISION_APPS_PATH)/utils/remote_service/include
IDIRS       += $(VISION_APPS_PATH)/utils/ipc/include

include $(FINALE)
