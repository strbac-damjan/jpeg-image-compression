#ifndef BMP_HANDLER_H
#define BMP_HANDLER_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// BMP file format structures
#pragma pack(push, 1)
typedef struct BMPFileHeader{
    uint16_t bfType;      // "BM" (0x4D42)
    uint32_t bfSize;      // Total file size in bytes
    uint16_t bfReserved1; // Reserved (must be 0)
    uint16_t bfReserved2; // Reserved (must be 0)
    uint32_t bfOffBits;   // Offset to start of pixel data
} BMPFileHeader;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct BMPInfoHeader{
    uint32_t biSize;          // Size of this header (40 bytes)
    int32_t  biWidth;         // Width of the image in pixels
    int32_t  biHeight;        // Height of the image in pixels (positive = top-down, negative = bottom-up)
    uint16_t biPlanes;        // Number of color planes (must be 1)
    uint16_t biBitCount;      // Bits per pixel (1, 4, 8, or 24)
    uint32_t biCompression;   // Compression type (0 = none, 1 = RLE 8-bit, 2 = RLE 4-bit)
    uint32_t biSizeImage;     // Size of the pixel data (can be 0 for uncompressed images)
    int32_t  biXPelsPerMeter; // Horizontal resolution (pixels per meter)
    int32_t  biYPelsPerMeter; // Vertical resolution (pixels per meter)
    uint32_t biClrUsed;       // Number of colors in the color palette (0 = default 2^n)
    uint32_t biClrImportant;  // Number of important colors (0 = all colors are important)
} BMPInfoHeader;
#pragma pack(pop)

typedef struct BMPImage {
    int32_t width;
    int32_t height;
    uint8_t* r;  // Red channel
    uint8_t* g;  // Green channel
    uint8_t* b;  // Blue channel
} BMPImage;

void freeBMPImage(BMPImage* image);

BMPImage* loadBMPImage(const char* filename);

bool saveBMPImage(const char* filename, const BMPImage* image);
#endif
