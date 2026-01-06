# --- Variables ---
CC = gcc
# CFLAGS: -Iinclude ensures the compiler finds bmp_handler.h
CFLAGS = -Iinclude -Wall -Wextra -g

LDFLAGS = -lm

# --- Configuration ---
# Source directories
SRC_DIRS = src src/io src/core
# Output directory
BUILD_DIR = build
# Executable name
TARGET_EXEC = jpeg_compression_app

# --- Automatic File Discovery ---
# Find all .c files in SRC_DIRS
SRCS = $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.c))
# Create object file paths (src/%.c -> build/%.o)
OBJS = $(patsubst src/%.c, $(BUILD_DIR)/%.o, $(SRCS))
# Final executable path
TARGET = $(BUILD_DIR)/$(TARGET_EXEC)

# --- Targets ---

# Default target (runs when you type 'make')
all: $(TARGET)

# Link
$(TARGET): $(OBJS)
	@echo "Linking executable..."
	@mkdir -p $(dir $@)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)
	@echo "Build successful! Executable created at: $@"

# Compile
$(BUILD_DIR)/%.o: src/%.c
	@echo "Compiling: $<"
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean
clean:
	@echo "Cleaning build directory..."
	rm -rf $(BUILD_DIR)

.PHONY: all clean