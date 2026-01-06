#include "converter.h"


YImage* convertBMPToJPEGGrayscale(const BMPImage* image) {
    
    if (image == NULL || image->data == NULL) {
        return NULL;
    }

    YImage* yImg = (YImage*)malloc(sizeof(YImage));
    if (yImg == NULL) {
        return NULL; 
    }

    int paddedWidth = (image->width + 7) & (~7);
    int paddedHeight = (image->height + 7) & (~7);

    yImg->width = paddedWidth;
    yImg->height = paddedHeight;
    
    // Allocate memory for the PADDED size
    yImg->data = (uint8_t*)malloc(paddedWidth * paddedHeight * sizeof(uint8_t));

    if (yImg->data == NULL) {
        free(yImg);
        return NULL;
    }
    for (int y = 0; y < paddedHeight; y++) {
        // Clamp the Y coordinate to the original image height.
        // If y >= original height, we repeat the last row.
        int srcY = MIN(y, image->height - 1);

        for (int x = 0; x < paddedWidth; x++) {
            // Clamp the X coordinate to the original image width.
            // If x >= original width, we repeat the last pixel of the row.
            int srcX = MIN(x, image->width - 1);

            // Calculate index in the source (RGB) buffer
            int srcIndex = (srcY * image->width + srcX) * 3;

            // Calculate index in the destination (Y) buffer
            int dstIndex = y * paddedWidth + x;

            uint8_t r = image->data[srcIndex];     
            uint8_t g = image->data[srcIndex + 1];
            uint8_t b = image->data[srcIndex + 2];

            // Original: Y = 0.299*R + 0.587*G + 0.114*B
            // Optimized whole number approximation (multiplied by 256):
            // Y = (77*R + 150*G + 29*B) >> 8
            uint32_t yVal = (77 * r + 150 * g + 29 * b) >> 8;

            yImg->data[dstIndex] = (uint8_t) yVal;
        }
    }

    return yImg;
}

CenteredYImage* centerYImage(const YImage* source) {
    
    if (source == NULL || source->data == NULL) {
        return NULL;
    }

    CenteredYImage* centeredImg = (CenteredYImage*)malloc(sizeof(CenteredYImage));
    if (centeredImg == NULL) {
        return NULL;
    }

    centeredImg->width = source->width;
    centeredImg->height = source->height;

    int totalPixels = source->width * source->height;
    centeredImg->data = (int8_t*)malloc(totalPixels * sizeof(int8_t));

    if (centeredImg->data == NULL) {
        free(centeredImg); // Cleanup struct allocation
        return NULL;
    }

    // Perform Level Shifting
    for (int i = 0; i < totalPixels; i++) {
        int shiftedValue = (int)source->data[i] - 128;
        
        centeredImg->data[i] = (int8_t)shiftedValue;
    }

    return centeredImg;
}

void freeCenteredYImage(CenteredYImage *img)
{
    if(img) 
    {
        if(img->data)
        {
            free(img->data);
        }
        free(img);
    }
}
