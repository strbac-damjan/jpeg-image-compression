#!/bin/bash

# ==============================================================================
# 1. POSTAVKE I PUTANJE
# ==============================================================================

PROJECT_ROOT=$(pwd)
SDK_PATH=$HOME/ti-processor-sdk-rtos-j721e-evm-09_02_00_05

# Komponente SDK-a
CGT7X_ROOT=${SDK_PATH}/psdk_tools/ti-cgt-c7000_4.1.0.LTS
VISION_APPS_PATH=${SDK_PATH}/vision_apps
TIOVX_PATH=${SDK_PATH}/tiovx
APP_UTILS_PATH=${SDK_PATH}/app_utils

# Provjere
if [ ! -d "$CGT7X_ROOT" ]; then echo "GRESKA: Nema kompajlera."; exit 1; fi
if [ ! -d "$APP_UTILS_PATH" ]; then echo "GRESKA: Nema app_utils."; exit 1; fi

BUILD_DIR=${PROJECT_ROOT}/debug_build

# ==============================================================================
# 2. FLAGOVI
# ==============================================================================

CFLAGS="-o3 --gen_opt_info=2 -k --src_interlist --silicon_version=7100 -DSOC_J721E -DTARGET_C71"

# OVDJE JE PROMJENA:
# Umjesto dubokih putanja, dajemo samo korijenske foldere.
# Kompajler ce sam naci "utils/ipc/..." unutar "app_utils"
INCLUDES="-I ${CGT7X_ROOT}/include \
          -I ${APP_UTILS_PATH} \
          -I ${VISION_APPS_PATH} \
          -I ${TIOVX_PATH}/include \
          -I ${TIOVX_PATH}/kernels/include \
          -I ${PROJECT_ROOT}/jpeg_compression/include 
          "

# ==============================================================================
# 3. IZVRŠAVANJE
# ==============================================================================

echo "--- Pripremam build folder: ${BUILD_DIR} ---"
rm -rf ${BUILD_DIR}
mkdir -p ${BUILD_DIR}

SOURCE_DIR="${PROJECT_ROOT}/jpeg_compression/src"
SOURCES=$(find ${SOURCE_DIR} -name "*.c")

echo "--- Pocetak kompilacije ---"

for SRC in $SOURCES; do
    FILENAME=$(basename "$SRC")
    
    echo "Analiziram: $FILENAME"
    
    "${CGT7X_ROOT}/bin/cl7x" $CFLAGS $INCLUDES \
        -fr="${BUILD_DIR}" \
        -fs="${BUILD_DIR}" \
        "$SRC"
        
    if [ $? -eq 0 ]; then
        echo "   [OK] -> ${BUILD_DIR}/${FILENAME%.*}.asm"
    else
        echo "   [GRESKA] Neuspjesna kompilacija."
    fi
    echo "---------------------------"
done

echo "Gotovo."