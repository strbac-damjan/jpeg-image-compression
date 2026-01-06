#!/bin/sh

# ===== Configuration =====
TARGET_USER=root
TARGET_IP=192.168.1.200
TARGET_DIR=/lib/firmware/vision_apps_evm
APPS_TARGET_DIR=/opt/vision_apps

BINARIES="
$HOME/ti-processor-sdk-rtos-j721e-evm-09_02_00_05/vision_apps/out/J721E/R5F/FREERTOS/release/*.out
$HOME/ti-processor-sdk-rtos-j721e-evm-09_02_00_05/vision_apps/out/J721E/C71/FREERTOS/release/*.out
$HOME/ti-processor-sdk-rtos-j721e-evm-09_02_00_05/vision_apps/out/J721E/C66/FREERTOS/release/*.out
"

APPS="
$HOME/ti-processor-sdk-rtos-j721e-evm-09_02_00_05/vision_apps/out/J721E/A72/LINUX/release/vx_app_single_cam.out"

# ===== Copy binaries =====
echo "Copying RTOS binaries to target..."

for BIN in $BINARIES; do
    echo "  -> $BIN"
    scp "$BIN" ${TARGET_USER}@${TARGET_IP}:${TARGET_DIR}/ || {
        echo "ERROR: Failed to copy $BIN"
        exit 1
    }
done

echo "Copying apps binaries to target..."
scp $APPS ${TARGET_USER}@${TARGET_IP}:${APPS_TARGET_DIR}

# ===== Restart target =====
echo "Rebooting target..."
ssh ${TARGET_USER}@${TARGET_IP} "sync && reboot"

echo "Done."
