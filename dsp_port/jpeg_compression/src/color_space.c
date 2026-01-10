#ifdef __C7000__
#include "jpeg_compression.h"

/* * Standard JPEG Conversion Coefficients (scaled by 256 for integer math)
 * Y = 0.299*R + 0.587*G + 0.114*B
 * * Scaled:
 * Y = (77*R + 150*G + 29*B) >> 8
 */
#define COEFF_R 77
#define COEFF_G 150
#define COEFF_B 29

void extractYComponentBlock32x8(BMPImage *img, int32_t startX, int32_t startY, int8_t *outputBuffer)
{
    /* 1. DECLARATIONS (C89 compliant) */
    int i;
    int32_t width = img->width;
    
    /* Input pointers (uint8_t) */
    uint8_t *rPtrBase = img->r + (startY * width + startX);
    uint8_t *gPtrBase = img->g + (startY * width + startX);
    uint8_t *bPtrBase = img->b + (startY * width + startX);

    /* Output pointer (int8_t) */
    int8_t *dstPtr = outputBuffer;

    /* Vector registers */
    uchar32 vR, vG, vB;      /* Input pixels (0..255) */
    short32 vR_s, vG_s, vB_s; /* Promoted to short for multiplication */
    short32 vY_s;            /* Calculated Y in short precision */
    char32  vY_out;          /* Final result (-128..127) */
    
    /* Constants vectors */
    short32 vCoeffR = (short32)COEFF_R;
    short32 vCoeffG = (short32)COEFF_G;
    short32 vCoeffB = (short32)COEFF_B;
    short32 vOffset = (short32)128; /* For level shifting (-128) */

    /* 2. LOOP over 8 rows */
    /* We process 32 pixels width at once per iteration */
    #pragma MUST_ITERATE(8, 8, 8)
    for (i = 0; i < 8; i++)
    {
        /* * LOAD (Vector Load) 
         * Casting pointer to (uchar32*) tells the compiler to load 32 bytes 
         * into a vector register. C7x handles unaligned loads automatically.
         */
        vR = *((uchar32 *)(rPtrBase + i * width));
        vG = *((uchar32 *)(gPtrBase + i * width));
        vB = *((uchar32 *)(bPtrBase + i * width));

        /* * CONVERT & CALCULATE
         * 1. Convert uchar (8-bit) to short (16-bit) to prevent overflow during multiply 
         */
        vR_s = __convert_short32(vR);
        vG_s = __convert_short32(vG);
        vB_s = __convert_short32(vB);

        /* * 2. Calculate Y = (77*R + 150*G + 29*B) 
         * Note: Operations are element-wise automatically on vector types
         */
        vY_s = (vR_s * vCoeffR) + (vG_s * vCoeffG) + (vB_s * vCoeffB);

        /* * 3. Shift right by 8 (divide by 256) 
         */
        vY_s = vY_s >> 8;

        /* * 4. Level Shift (JPEG requires -128 offset, so range becomes -128..127)
         * We subtract 128.
         */
        vY_s = vY_s - vOffset;

        /* * 5. Convert back to 8-bit (signed char)
         * __convert_char32 handles clamping/truncation if necessary
         */
        vY_out = __convert_char32(vY_s);

        /* * STORE
         * Write 32 bytes to the linear macro buffer.
         * The buffer is 32x8, so row 0 is at offset 0, row 1 at 32, etc.
         */
        *((char32 *)(dstPtr + i * 32)) = vY_out;
    }
}
#endif
