#ifndef CONVERTER_H
#define CONVERTER_H

#include "bmp_handler.h"

#include <stdlib.h>
#include <stdint.h> 

typedef struct {
    int width;
    int height;
    int8_t *data; // Pointer to signed 8-bit pixel array. 
                  // Values range from -128 to 127.
} CenteredYImage;

// Internal image structure (does not necessarily need packing as it stays in memory)
typedef struct {
    int width;           // Image width
    int height;          // Image height
    uint8_t *data;       // Pointer to the pixel array (length = width * height)
                         // Values 0-255 (where 0=black, 255=white)
} YImage;

YImage* convertBMPToJPEGGrayscale(const BMPImage* image);

CenteredYImage *centerYImage(const YImage *source);
void freeCenteredYImage(CenteredYImage* img);

#endif