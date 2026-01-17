#!/bin/bash

# ==============================================================================
# SETTINGS AND PATHS
# ==============================================================================

PROJECT_ROOT=$(pwd)
SDK_PATH=$HOME/ti-processor-sdk-rtos-j721e-evm-09_02_00_05

CGT7X_ROOT=${SDK_PATH}/psdk_tools/ti-cgt-c7000_4.1.0.LTS
VISION_APPS_PATH=${SDK_PATH}/vision_apps
TIOVX_PATH=${SDK_PATH}/tiovx
APP_UTILS_PATH=${SDK_PATH}/app_utils

if [ ! -d "$CGT7X_ROOT" ]; then echo "[ERROR]: Compiler not found"; exit 1; fi
if [ ! -d "$APP_UTILS_PATH" ]; then echo "[ERROR]: App utils library not found"; exit 1; fi

BUILD_DIR=${PROJECT_ROOT}/debug_build

# ==============================================================================
# COMPILER FLAGS AND INCLUDES
# ==============================================================================

CFLAGS="-o3 --gen_opt_info=2 -k --src_interlist --silicon_version=7100 -DSOC_J721E -DTARGET_C71"


INCLUDES="-I ${CGT7X_ROOT}/include \
          -I ${APP_UTILS_PATH} \
          -I ${VISION_APPS_PATH} \
          -I ${TIOVX_PATH}/include \
          -I ${TIOVX_PATH}/kernels/include \
          -I ${PROJECT_ROOT}/jpeg_compression/include 
          "

# ==============================================================================
# Execution
# ==============================================================================

echo "--- Preparing build folder: ${BUILD_DIR} ---"
rm -rf ${BUILD_DIR}
mkdir -p ${BUILD_DIR}

SOURCE_DIR="${PROJECT_ROOT}/jpeg_compression/src"
SOURCES=$(find ${SOURCE_DIR} -name "*.c")

echo "--- Beginning compilation ---"

for SRC in $SOURCES; do
    FILENAME=$(basename "$SRC")
    
    echo "Compiling: $FILENAME"
    
    "${CGT7X_ROOT}/bin/cl7x" $CFLAGS $INCLUDES \
        -fr="${BUILD_DIR}" \
        -fs="${BUILD_DIR}" \
        "$SRC"
        
    if [ $? -eq 0 ]; then
        echo "   [OK] -> ${BUILD_DIR}/${FILENAME%.*}.asm"
    else
        echo "   [ERROR]: Compilation failed."
    fi
    echo ""
done

echo "Done."