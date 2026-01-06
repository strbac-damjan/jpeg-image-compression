#ifndef DCT_H
#define DCT_H

#ifndef PI
#define PI 3.14159265358979323846264338327950288 
#endif

#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include "converter.h"


typedef struct {
    int width;           // Image width (should be multiple of 8)
    int height;          // Image height (should be multiple of 8)
    float *coefficients; // Pointer to DCT coefficients array.
                         // Size = width * height.
} DCTImage;

//void initDCTTables();
void computeDCTBlock(const int8_t inputBlock[8][8], float outputBlock[8][8]);
DCTImage *performDCT(const CenteredYImage *image);
void freeDCTImage(DCTImage* img);


#endif
