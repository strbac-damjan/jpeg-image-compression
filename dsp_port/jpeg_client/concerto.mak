$(warning [DEBUG] concerto.mak file in dsp_port/jpeg_client/concerto.mak is found!)

# Ensure this compiles only for the A72 core running Linux (or QNX)
ifeq ($(TARGET_CPU),$(filter $(TARGET_CPU), A72))
ifeq ($(TARGET_OS),$(filter $(TARGET_OS), LINUX QNX))

include $(PRELUDE)

# Source files
CSOURCES    := main.c

# Name of the output executable (.out)
TARGET      := jpeg_client_app
TARGETTYPE  := exe

# KEY STEP: This file includes all standard system libraries
# (app_init, ipc, utils, OpenVX framework, etc.)
include $(VISION_APPS_PATH)/apps/concerto_mpu_inc.mak

# Include path for shared headers (e.g., message structures defined in your common folder)
IDIRS       += $(JPEG_REPO_PATH)/jpeg_compression/include

# Uncomment if the Imaging library is needed.
# (Usually not required for basic IPC demos unless you are accessing sensors directly)
# STATIC_LIBS += $(IMAGING_LIBS)
# IDIRS       += $(IMAGING_IDIRS)

include $(FINALE)

endif
endif