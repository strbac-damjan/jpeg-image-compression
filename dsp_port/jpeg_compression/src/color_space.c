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

void extractYComponentBlock4x8x8(const uint8_t * __restrict rComponent, 
                                const uint8_t * __restrict gComponent, 
                                const uint8_t * __restrict bComponent, int32_t startX, int32_t startY, int width, int8_t * __restrict outputBuffer)
{
    int i;
    
    int8_t * __restrict pDst = outputBuffer;

    /* Vektori - ostavljamo ih lokalno da bi se koristio VSPLAT (registri), a ne MEMORIJA */
    uchar32 vR, vG, vB;
    short32 vR_s, vG_s, vB_s;
    short32 vY_s;
    char32  vY_out;
    
    /* Ovi se generišu u 1 ciklusu koristeći VSPLAT instrukciju */
    short32 vCoeffR = (short32)COEFF_R;
    short32 vCoeffG = (short32)COEFF_G;
    short32 vCoeffB = (short32)COEFF_B;
    short32 vOffset = (short32)128;

    /* Opcionalno: Ako znaš da su pointeri poravnati na 64 bajta (512 bita) */
    /* _nassert(((uintptr_t)rComponent & 63) == 0); */
    
    #pragma MUST_ITERATE(8, 8, 8)
    for (i = 0; i < 8; i++)
    {
        /* Učitavanje */
        vR = *((uchar32 *)rComponent);
        vG = *((uchar32 *)gComponent);
        vB = *((uchar32 *)bComponent);

        /* Konverzija */
        vR_s = __convert_short32(vR);
        vG_s = __convert_short32(vG);
        vB_s = __convert_short32(vB);

        /* Obrada Y = R*77 + G*150 + B*29 */
        vY_s = (vR_s * vCoeffR) + (vG_s * vCoeffG) + (vB_s * vCoeffB);

        /* Shift i Offset */
        vY_s = __shift_right(vY_s, (short32)8);
        vY_s = vY_s - vOffset;

        /* Pakovanje i upis */
        vY_out = __convert_char32(vY_s);
        *((char32 *)pDst) = vY_out;

        /* Pointer aritmetika */
        rComponent += width;
        gComponent += width;
        bComponent += width;
        pDst  += 32;
    }
}
#endif
