#ifdef __C7000__

#include "jpeg_compression.h"

// Standard JPEG luminance conversion coefficients scaled by 256
// Y = 0.299*R + 0.587*G + 0.114*B
// Scaled integer form:
// Y = (77*R + 150*G + 29*B) >> 8
#define COEFF_R 77
#define COEFF_G 150
#define COEFF_B 29

void extractYComponentBlock4x8x8(
    uint8_t * __restrict rComponent,
    uint8_t * __restrict gComponent,
    uint8_t * __restrict bComponent,
    int32_t startX,
    int32_t startY,
    int width,
    int8_t * __restrict outputBuffer)
{
    int i;

    // Destination pointer for the output Y block
    int8_t * __restrict pDst = outputBuffer;

    // Vector registers kept local to enable VSPLAT usage and avoid memory access
    uchar32 vR, vG, vB;
    short32 vR_s, vG_s, vB_s;
    short32 vY_s;
    char32  vY_out;

    // Coefficient vectors generated in a single cycle using VSPLAT
    short32 vCoeffR = (short32)COEFF_R;
    short32 vCoeffG = (short32)COEFF_G;
    short32 vCoeffB = (short32)COEFF_B;

    // Offset used to convert unsigned range to signed range
    short32 vOffset = (short32)128;

    // The loop always iterates exactly 8 times for an 8-row block
    #pragma MUST_ITERATE(8, 8, 8)
    for (i = 0; i < 8; i++)
    {
        // Load 32 pixels of R, G, and B components
        vR = *((uchar32 *)rComponent);
        vG = *((uchar32 *)gComponent);
        vB = *((uchar32 *)bComponent);

        // Convert unsigned 8-bit values to signed 16-bit for computation
        vR_s = __convert_short32(vR);
        vG_s = __convert_short32(vG);
        vB_s = __convert_short32(vB);

        // Compute luminance using scaled integer arithmetic
        // Y = R*77 + G*150 + B*29
        vY_s = (vR_s * vCoeffR) + (vG_s * vCoeffG) + (vB_s * vCoeffB);

        // Normalize by shifting right and apply offset
        vY_s = __shift_right(vY_s, (short32)8);
        vY_s = vY_s - vOffset;

        // Pack the result back to signed 8-bit values and store
        vY_out = __convert_char32(vY_s);
        *((char32 *)pDst) = vY_out;

        // Advance input pointers by one image row
        rComponent += width;
        gComponent += width;
        bComponent += width;

        // Advance output pointer by 32 bytes for the next vector
        pDst += 32;
    }
}
#endif
