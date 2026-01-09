#ifdef __C7000__
#include "jpeg_compression.h"
#include <math.h>

/* -------------------------------------------------------------------------------------
 * DCT CONSTANTS
 * -------------------------------------------------------------------------------------
 */

/* * Orthonormal DCT Matrix (T)
 * This matrix includes the necessary scaling factors (sqrt(2/N)).
 * The transformation is performed as Result = T * Block * T'.
 */
static const float DCT_T[8][8] = {
    { 0.353553f,  0.353553f,  0.353553f,  0.353553f,  0.353553f,  0.353553f,  0.353553f,  0.353553f },
    { 0.490393f,  0.415735f,  0.277785f,  0.097545f, -0.097545f, -0.277785f, -0.415735f, -0.490393f },
    { 0.461940f,  0.191342f, -0.191342f, -0.461940f, -0.461940f, -0.191342f,  0.191342f,  0.461940f },
    { 0.415735f, -0.097545f, -0.490393f, -0.277785f,  0.277785f,  0.490393f,  0.097545f, -0.415735f },
    { 0.353553f, -0.353553f, -0.353553f,  0.353553f,  0.353553f, -0.353553f, -0.353553f,  0.353553f },
    { 0.277785f, -0.490393f,  0.097545f,  0.415735f, -0.415735f, -0.097545f,  0.490393f, -0.277785f },
    { 0.191342f, -0.461940f,  0.461940f, -0.191342f, -0.191342f,  0.461940f, -0.461940f,  0.191342f },
    { 0.097545f, -0.277785f,  0.415735f, -0.490393f,  0.490393f, -0.415735f,  0.277785f, -0.097545f }
};

/* * Transposed DCT Matrix (T')
 * Pre-calculated transpose of the DCT matrix to optimize the first pass 
 * of matrix multiplication.
 */
static const float DCT_T_TRANSPOSED[8][8] = {
    { 0.353553f,  0.490393f,  0.461940f,  0.415735f,  0.353553f,  0.277785f,  0.191342f,  0.097545f },
    { 0.353553f,  0.415735f,  0.191342f, -0.097545f, -0.353553f, -0.490393f, -0.461940f, -0.277785f },
    { 0.353553f,  0.277785f, -0.191342f, -0.490393f, -0.353553f,  0.097545f,  0.461940f,  0.415735f },
    { 0.353553f,  0.097545f, -0.461940f, -0.277785f,  0.353553f,  0.415735f, -0.191342f, -0.490393f },
    { 0.353553f, -0.097545f, -0.461940f,  0.277785f,  0.353553f, -0.415735f, -0.191342f,  0.490393f },
    { 0.353553f, -0.277785f, -0.191342f,  0.490393f, -0.353553f, -0.097545f,  0.461940f, -0.415735f },
    { 0.353553f, -0.415735f,  0.191342f,  0.097545f, -0.353553f,  0.490393f, -0.461940f,  0.277785f },
    { 0.353553f, -0.490393f,  0.461940f, -0.415735f,  0.353553f, -0.277785f,  0.191342f, -0.097545f }
};


/* -------------------------------------------------------------------------------------
 * HELPER FUNCTIONS
 * -------------------------------------------------------------------------------------
 */

/**
 * \brief Multiplies two 8x8 matrices: C = A * B
 * * Note: Arguments are not const to ensure compatibility with C89/C90 strict 
 * pointer types when passing non-const buffers.
 */
static inline void matrixMul8x8(float A[8][8], float B[8][8], float C[8][8])
{
    int i, j, k; 
    for (i = 0; i < 8; i++) 
    {
        for (j = 0; j < 8; j++) 
        {
            float sum = 0.0f;
            
            for (k = 0; k < 8; k++) 
            {
                sum += A[i][k] * B[k][j];
            }
            C[i][j] = sum;
        }
    }
}

/**
 * \brief Computes the 2D Discrete Cosine Transform (DCT) for the image.
 * * The function iterates over the image in 8x8 blocks, converting the data
 * to the frequency domain using matrix multiplication.
 */
void computeDCT(YImage *y_img, DCTImage *dct_out)
{
    int width = y_img->width;
    int height = y_img->height;
    int8_t *srcData = (int8_t *)y_img->data; 
    float *dstData = dct_out->coefficients;
    
    // Loop variables declared at the top of the block for strict C compliance
    int y, x;
    int i, j;

    // Process the image in 8x8 non-overlapping blocks
    for (y = 0; y <= height - 8; y += 8)
    {
        for (x = 0; x <= width - 8; x += 8)
        {
            float block[8][8];
            float temp[8][8];  
            float result[8][8];
            float *dstPtr;

            // Load 8x8 block from linear memory into local buffer
            for (i = 0; i < 8; i++)
            {
                for (j = 0; j < 8; j++)
                {
                    block[i][j] = (float)srcData[(y + i) * width + (x + j)];
                }
            }

            // First pass: Temp = Block * T_Transposed
            // This effectively performs the DCT on the rows of the block.
            // Casting to (float (*)[8]) is required to match the function signature
            // with the const global arrays.
            matrixMul8x8(block, (float (*)[8])DCT_T_TRANSPOSED, temp);

            // Second pass: Result = T * Temp
            // This performs the DCT on the columns, completing the 2D transform.
            matrixMul8x8((float (*)[8])DCT_T, temp, result);

            // Store the result back into the linear output buffer
            for (i = 0; i < 8; i++)
            {
                dstPtr = &dstData[(y + i) * width + x];
                
                // Unroll pragma helps the compiler generate efficient vector stores
                #pragma UNROLL(8)
                for (j = 0; j < 8; j++)
                {
                    dstPtr[j] = result[i][j];
                }
            }
        }
    }
}

#endif
