#!/bin/bash

# ==========================================
# CONFIGURATION (PATHS & NETWORK)
# ==========================================

# Network settings
TARGET_USER=root
TARGET_IP=192.168.1.200

# Host Paths (PC)
# Adjust this path to the folder where you usually run 'make vision_apps'
SDK_ROOT="$HOME/ti-processor-sdk-rtos-j721e-evm-09_02_00_05"
BUILD_DIR="$SDK_ROOT" 

# Target Paths (TDA4)
TARGET_DIR_FIRMWARE=/lib/firmware/vision_apps_evm
TARGET_DIR_APPS=/opt/vision_apps

# Files to copy (using SDK_ROOT variable for readability)
BINARIES="
$SDK_ROOT/vision_apps/out/J721E/R5F/FREERTOS/release/*.out
$SDK_ROOT/vision_apps/out/J721E/C71/FREERTOS/release/*.out
$SDK_ROOT/vision_apps/out/J721E/C66/FREERTOS/release/*.out
"

APPS="
$SDK_ROOT/vision_apps/out/J721E/A72/LINUX/release/jpeg_client_app.out
$SDK_ROOT/vision_apps/out/J721E/A72/LINUX/release/jpeg_client_app.out.map"

# ==========================================
# BUILD PROCESS
# ==========================================

echo "========================================"
echo "STARTING BUILD PROCESS"
echo "========================================"

# Change to build directory
cd "$BUILD_DIR/sdk_builder" || { echo "ERROR: Could not find build directory $BUILD_DIR"; exit 1; }

# Run make command
# Added 'time' to see how long the build takes
echo "Running: make vision_apps -j16 in $(pwd)"
time make vision_apps -j16

# ERROR CHECK!
# $? contains the exit code of the last command. 0 means success.
if [ $? -ne 0 ]; then
    echo "########################################"
    echo "       BUILD FAILED! STOPPING."
    echo "########################################"
    exit 1
fi

echo "Build successful! "