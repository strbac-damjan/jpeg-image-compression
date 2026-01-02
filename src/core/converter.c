#include "converter.h"


YImage* convertBMPToJPEGGrayscale(const BMPImage* image) {
    
    if (image == NULL || image->data == NULL) {
        return NULL;
    }

    YImage* yImg = (YImage*)malloc(sizeof(YImage));
    if (yImg == NULL) {
        return NULL; // Greška pri alokaciji
    }

    yImg->width = image->width;
    yImg->height = image->height;
    
    int totalPixels = image->width * image->height;
    yImg->data = (unsigned char*)malloc(totalPixels * sizeof(unsigned char));

    if (yImg->data == NULL) {
        free(yImg);
        return NULL;
    }

    for (int i = 0; i < totalPixels; i++) {
        int rgbIndex = i * 3;

        uint8_t r = image->data[rgbIndex];     
        uint8_t g = image->data[rgbIndex + 1];
        uint8_t b = image->data[rgbIndex + 2];

        // Original:
        // Y = 0.299*R + 0.587*G + 0.114*B
        // Optimized whole number approximation(multiplied by 256):
        // Y = (77*R + 150*G + 29*B) >> 8
        
        uint32_t yVal = (77 * r + 150 * g + 29 * b) >> 8;

        yImg->data[i] = (uint8_t) yVal;
    }

    return yImg;
}