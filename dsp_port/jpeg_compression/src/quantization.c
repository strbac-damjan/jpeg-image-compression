#ifdef __C7000__
#include "jpeg_compression.h"
#include <math.h>

/* -------------------------------------------------------------------------------------
 * QUANTIZATION CONSTANTS
 * -------------------------------------------------------------------------------------
 */

/* * Reciprocal Luminance Quantization Table
 * Values are pre-calculated as (1.0f / Standard_Table_Value).
 * This allows us to use fast multiplication instead of slow division on the DSP.
 */
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
 * \brief Quantizes DCT coefficients using the luminance table.
 * * Performs element-wise multiplication with the reciprocal quantization table
 * and rounds to the nearest integer.
 * * \param dct_img  Input image with DCT coefficients (float).
 * \param q_img    Output image with quantized coefficients (int16_t).
 */
void quantizeImage(DCTImage *dct_img, QuantizedImage *q_img)
{
    int width = dct_img->width;
    int height = dct_img->height;
    
    float *srcData = dct_img->coefficients;
    int16_t *dstData = q_img->data;

    int y, x, i, j;

    // Process the image in 8x8 blocks
    for (y = 0; y <= height - 8; y += 8)
    {
        for (x = 0; x <= width - 8; x += 8)
        {
            // Iterate inside the 8x8 block
            for (i = 0; i < 8; i++) 
            {
                // Calculate pointers to the start of the current row in the image
                float *blockRowSrc = &srcData[(y + i) * width + x];
                int16_t *blockRowDst = &dstData[(y + i) * width + x];
                
                // Pointer to the current row in the 8x8 quantization table
                const float *tblRow = &RECIP_LUMINANCE_QUANT_TBL[i * 8];

                // Unroll loop for SIMD optimization (8 pixels per vector)
                #pragma UNROLL(8)
                for (j = 0; j < 8; j++) 
                {
                    float dctVal = blockRowSrc[j];
                    float recipQ = tblRow[j];
                    
                    // Multiply by reciprocal (faster than division)
                    float scaled = dctVal * recipQ;
                    
                    // Round to nearest integer:
                    // roundf is typically mapped to an efficient intrinsic on C7x.
                    // Casting to (int16_t) handles the storage type.
                    blockRowDst[j] = (int16_t)roundf(scaled);
                }
            }
        }
    }
}
#endif
