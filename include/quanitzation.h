#ifndef QUANITZATION_H
#define QUANITZATION_H

#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include "dct.h"
#include "jpeg_tables.h"

typedef struct {
    int width;
    int height;
    int16_t *data; 
} QuantizedImage;

QuantizedImage* quantizeImage(const DCTImage* dctImg);
void freeQuantizedImage(QuantizedImage* img);


#endif