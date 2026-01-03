#include "zigzag.h"

#include <stdlib.h>
#include <stdint.h>

// Standard JPEG Zig-Zag order
static const uint8_t ZIGZAG_ORDER[64] = {
    0, 1, 8, 16, 9, 2, 3, 10,
    17, 24, 32, 25, 18, 11, 4, 5,
    12, 19, 26, 33, 40, 48, 41, 34,
    27, 20, 13, 6, 7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36,
    29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46,
    53, 60, 61, 54, 47, 55, 62, 63};

/**
 * Performs Zig-Zag scanning on quantized image blocks.
 * Converts 2D image blocks into linear arrays of 64 coefficients.
 */
ZigZagData *performZigZag(const QuantizedImage *qImg)
{
    if (qImg == NULL || qImg->data == NULL)
        return NULL;

    ZigZagData *zzData = (ZigZagData *)malloc(sizeof(ZigZagData));
    if (zzData == NULL)
        return NULL;

    // Računamo koliko imamo 8x8 blokova
    // Pretpostavljamo da su dimenzije deljive sa 8 (standard za ovaj korak)
    zzData->numBlocksW = qImg->width / 8;
    zzData->numBlocksH = qImg->height / 8;
    zzData->totalBlocks = zzData->numBlocksW * zzData->numBlocksH;

    // Alociramo memoriju: 64 koeficijenta (int16) po bloku
    int totalCoeffs = zzData->totalBlocks * 64;
    zzData->data = (int16_t *)malloc(totalCoeffs * sizeof(int16_t));

    if (zzData->data == NULL)
    {
        free(zzData);
        return NULL;
    }

    int blockIndex = 0;

    // Iteriramo kroz blokove slike (Raster Scan order of blocks)
    for (int blockY = 0; blockY < qImg->height; blockY += 8)
    {
        for (int blockX = 0; blockX < qImg->width; blockX += 8)
        {

            // Za svaki blok, popunjavamo 64 vrednosti u ZigZag nizu
            for (int i = 0; i < 64; i++)
            {

                // 1. Gde smo u ZigZag putanji unutar bloka? (0-63)
                int zigZagPos = ZIGZAG_ORDER[i];

                // 2. Mapiramo tu poziciju na 2D koordinate unutar bloka (0-7)
                int localRow = zigZagPos / 8; // Integer deljenje (red)
                int localCol = zigZagPos % 8; // Ostatak (kolona)

                // 3. Nalazimo pravi indeks u velikom nizu originalne slike
                // GlobalRow = blockY + localRow
                // GlobalCol = blockX + localCol
                int imageIndex = (blockY + localRow) * qImg->width + (blockX + localCol);

                // 4. Upisujemo u izlazni niz
                // Izlazni niz je organizovan blok po blok
                zzData->data[blockIndex * 64 + i] = qImg->data[imageIndex];
            }

            blockIndex++;
        }
    }

    return zzData;
}

void freeZigZagData(ZigZagData *zData)
{
    if (zData)
    {
        if(zData->data)
        {
            free(zData->data);
        }
        free(zData);
    }
}
