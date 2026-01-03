#ifndef CONVERTER_H
#define CONVERTER_H
#include "bmp_handler.h"
#include "jpeg_handler.h"

#include <stdlib.h>
#include <stdint.h> 

typedef struct {
    int width;
    int height;
    int8_t *data; // Pointer to signed 8-bit pixel array. 
                  // Values range from -128 to 127.
} CenteredYImage;

YImage* convertBMPToJPEGGrayscale(const BMPImage* image);

CenteredYImage *centerYImage(const YImage *source);
void freeCenteredYImage(CenteredYImage* img);

#endif