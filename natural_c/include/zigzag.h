#ifndef JPEG_ZIGZAG_H
#define JPEG_ZIGZAG_H

#include <stdint.h>
#include "quantization.h"

typedef struct {
    int numBlocksW;
    int numBlocksH;
    int totalBlocks;
    int16_t *data;
} ZigZagData;

ZigZagData* performZigZag(const QuantizedImage* qImg);
void freeZigZagData(ZigZagData* zData);

#endif