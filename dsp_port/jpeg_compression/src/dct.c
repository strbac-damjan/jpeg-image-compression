#ifdef __C7000__
#include <c7x.h>
#include "jpeg_compression.h"
#include <math.h>

/* --- DCT CONSTANTS (Global in .c file) --- */
/* T Matrix */
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

/* T Transpose */
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

/**
 * \brief Standard Matrix Multiplication 8x8 (C = A * B)
 */
static void matrixMul8x8(const float A[8][8], const float B[8][8], float C[8][8])
{
    int i, j, k;
    
    for ( i = 0; i < 8; i++) 
    {
        for ( j = 0; j < 8; j++) 
        {
            float sum = 0.0f;
            /* Hint: Inner loop usually unrolled by compiler */
            for ( k = 0; k < 8; k++) 
            {
                sum += A[i][k] * B[k][j];
            }
            C[i][j] = sum;
        }
    }
}

/**
 * \brief Computes DCT for a single 8x8 block.
 */
void computeDCTBlock(int8_t *src_data, float *dct_out, int32_t stride)
{
    /* C89 Declarations at the top */
    float block[8][8];
    float temp[8][8];
    float result[8][8];
    float *linear_out;
    int i, j;

    /* 1. Load Data (Strided Load) */
    for( i = 0; i < 8; i++)
    {
        for( j = 0; j < 8; j++)
        {
            /* Access: [row * stride + col] */
            block[i][j] = (float)src_data[i * stride + j];
        }
    }

    /* * DCT Calculation: Result = T * Block * T'
     * Where T is DCT_T and T' is DCT_T_TRANSPOSED
     */

    /* Step A: Temp = Block * T' */
    /* FIX: Cast 'block' to (const float (*)[8]) to satisfy compiler type check */
    matrixMul8x8((const float (*)[8])block, (const float (*)[8])DCT_T_TRANSPOSED, temp);

    /* Step B: Result = T * Temp */
    /* FIX: Cast 'temp' to (const float (*)[8]) */
    matrixMul8x8((const float (*)[8])DCT_T, (const float (*)[8])temp, result);

    /* 2. Store Result (Linear Store) */
    linear_out = dct_out;
    
    for( i = 0; i < 8; i++)
    {
        for( j = 0; j < 8; j++)
        {
             *linear_out++ = result[i][j];
        }
    }
}
#endif
