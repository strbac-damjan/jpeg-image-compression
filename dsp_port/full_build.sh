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

# Provjere postojanja foldera
if [ ! -d "$CGT7X_ROOT" ]; then echo "GRESKA: Nema kompajlera."; exit 1; fi
if [ ! -d "$TIOVX_PATH" ]; then echo "GRESKA: Nema TIOVX."; exit 1; fi

# Provjera za app_utils (posto si rekao da je tu)
if [ ! -d "$APP_UTILS_PATH" ]; then
    echo "UPOZORENJE: Ne vidim folder $APP_UTILS_PATH"
    echo "Provjeri da li se folder zove 'app_utils' u root-u SDK-a."
fi

BUILD_DIR=${PROJECT_ROOT}/build

# ==============================================================================
# 2. FLAGOVI
# ==============================================================================

# -DSOC_J721E definise platformu
CFLAGS="-O3 --gen_opt_info=2 -k --src_interlist --silicon_version=7100 -DSOC_J721E -DTARGET_C71"

# INCLUDE PUTANJE
# -I ${APP_UTILS_PATH} : Ovo omogucava da #include "utils/ipc/..." radi
# -I ${VISION_APPS_PATH} : Ovo omogucava da stariji utils rade
INCLUDES="-I ${PROJECT_ROOT}/jpeg_compression/include \
          -I ${CGT7X_ROOT}/include \
          -I ${VISION_APPS_PATH} \
          -I ${APP_UTILS_PATH}/utils/ipc/include/ \
          -I ${APP_UTILS_PATH}/utils/remote_service/include/ \
          -I ${APP_UTILS_PATH}/utils/console_io/include/ \
          -I ${APP_UTILS_PATH}/utils/mem/include/ \
          -I ${TIOVX_PATH}/include \

# ==============================================================================
# 3. IZVRŠAVANJE
# ==============================================================================

echo "--- Pripremam build folder: ${BUILD_DIR} ---"
rm -rf ${BUILD_DIR}
mkdir -p ${BUILD_DIR}

SOURCE_DIR="${PROJECT_ROOT}/jpeg_compression/src"
SOURCES=$(find ${SOURCE_DIR} -name "*.c")

echo "--- Pocetak kompilacije ---"
echo "SDK: $SDK_PATH"
echo "Dodajem APP_UTILS: $APP_UTILS_PATH"

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

echo "Gotovo. Provjeri 'build/dct.asm'."