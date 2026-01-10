#ifdef __C7000__
#include "jpeg_compression.h"
#include <math.h>
#include <c7x.h>

/* -------------------------------------------------------------------------------------
 * QUANTIZATION CONSTANTS
 * -------------------------------------------------------------------------------------
 */

/* Reciprocal Luminance Quantization Table (Linear 64 elements) */
static const float RECIP_LUMINANCE_QUANT_TBL[64] = {
    0.062500f, 0.090909f, 0.100000f, 0.062500f, 0.041667f, 0.025000f, 0.019608f, 0.016393f,
    0.083333f, 0.083333f, 0.071429f, 0.052632f, 0.038462f, 0.017241f, 0.016667f, 0.018182f,
    0.071429f, 0.076923f, 0.062500f, 0.041667f, 0.025000f, 0.017544f, 0.014493f, 0.017857f,
    0.071429f, 0.058824f, 0.045455f, 0.034483f, 0.019608f, 0.011494f, 0.012500f, 0.016129f,
    0.055556f, 0.045455f, 0.027027f, 0.017857f, 0.014706f, 0.009174f, 0.009709f, 0.012987f,
    0.041667f, 0.028571f, 0.018182f, 0.015625f, 0.012346f, 0.009615f, 0.008850f, 0.010870f,
    0.020408f, 0.015625f, 0.012821f, 0.011494f, 0.009709f, 0.008264f, 0.008333f, 0.009901f,
    0.013889f, 0.010870f, 0.010526f, 0.010204f, 0.008929f, 0.010000f, 0.009709f, 0.010101f
};

/**
 * \brief Quantizes a single 8x8 DCT block.
 * * Takes a linear array of 64 floats (from DCT), multiplies by the reciprocal
 * quantization table, and rounds to int16.
 *
 * \param dct_block    Input: Linear array of 64 floats (L1 memory)
 * \param quant_block  Output: Linear array of 64 int16_t (L1 memory)
 */
void quantizeBlock(float *dct_block, int16_t *quant_block)
{   
    int i;
    // C7x Optimization Hint:
    // Inform compiler that we are processing exactly 64 elements.
    // This allows it to generate optimal SIMD instructions without loop overhead code.
    //#pragma MUST_ITERATE(64, 64, 64)
    for ( i = 0; i < 64; i++) 
    {
        float dctVal = dct_block[i];
        float recipQ = RECIP_LUMINANCE_QUANT_TBL[i];
        
        // Math: val * (1/Q)
        float scaled = dctVal * recipQ;
        
        // Rounding
        // roundf maps to efficient hardware instructions.
        quant_block[i] = (int16_t)roundf(scaled);
    }
}
#endif
