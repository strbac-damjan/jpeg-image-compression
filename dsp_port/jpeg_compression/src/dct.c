#ifdef __C7000__
#include <c7x.h>
#include "jpeg_compression.h"
#include <math.h>

/* --- DCT CONSTANTS --- */
__attribute__((aligned(64))) static const float DCT_T[64] = {
     0.353553f,  0.353553f,  0.353553f,  0.353553f,  0.353553f,  0.353553f,  0.353553f,  0.353553f,
     0.490393f,  0.415735f,  0.277785f,  0.097545f, -0.097545f, -0.277785f, -0.415735f, -0.490393f,
     0.461940f,  0.191342f, -0.191342f, -0.461940f, -0.461940f, -0.191342f,  0.191342f,  0.461940f,
     0.415735f, -0.097545f, -0.490393f, -0.277785f,  0.277785f,  0.490393f,  0.097545f, -0.415735f,
     0.353553f, -0.353553f, -0.353553f,  0.353553f,  0.353553f, -0.353553f, -0.353553f,  0.353553f,
     0.277785f, -0.490393f,  0.097545f,  0.415735f, -0.415735f, -0.097545f,  0.490393f, -0.277785f,
     0.191342f, -0.461940f,  0.461940f, -0.191342f, -0.191342f,  0.461940f, -0.461940f,  0.191342f,
     0.097545f, -0.277785f,  0.415735f, -0.490393f,  0.490393f, -0.415735f,  0.277785f, -0.097545f
};

__attribute__((aligned(64))) static const float DCT_T_TRANSPOSED[64] = {
     0.353553f,  0.490393f,  0.461940f,  0.415735f,  0.353553f,  0.277785f,  0.191342f,  0.097545f,
     0.353553f,  0.415735f,  0.191342f, -0.097545f, -0.353553f, -0.490393f, -0.461940f, -0.277785f,
     0.353553f,  0.277785f, -0.191342f, -0.490393f, -0.353553f,  0.097545f,  0.461940f,  0.415735f,
     0.353553f,  0.097545f, -0.461940f, -0.277785f,  0.353553f,  0.415735f, -0.191342f, -0.490393f,
     0.353553f, -0.097545f, -0.461940f,  0.277785f,  0.353553f, -0.415735f, -0.191342f,  0.490393f,
     0.353553f, -0.277785f, -0.191342f,  0.490393f, -0.353553f, -0.097545f,  0.461940f, -0.415735f,
     0.353553f, -0.415735f,  0.191342f,  0.097545f, -0.353553f,  0.490393f, -0.461940f,  0.277785f,
     0.353553f, -0.490393f,  0.461940f, -0.415735f,  0.353553f, -0.277785f,  0.191342f, -0.097545f
};

/**
 * \brief Standard Matrix Multiplication 8x8 (C = A * B)
 */
static inline void matrixMul8x8(const float * __restrict A, const float * __restrict B, float * __restrict C)
{
    int i, j, k;
    
    #pragma MUST_ITERATE(8, 8, 8)
    for ( i = 0; i < 8; i++) 
    {
        #pragma MUST_ITERATE(8, 8, 8)
        for ( j = 0; j < 8; j++) 
        {
            float sum = 0.0f; 

            #pragma MUST_ITERATE(8, 8, 8)
            #pragma UNROLL(8)
            for ( k = 0; k < 8; k++) 
            {
                sum += A[i*8 + k] * B[k*8 + j];
            }
            C[i*8 + j] = sum;
        }
    }
}

/**
 * \brief Computes DCT for a single 8x8 block (Float Input).
 */
static void computeDCTBlock(const float * __restrict src_block, float * __restrict dct_out)
{
    __attribute__((aligned(64))) float temp[64];
    __attribute__((aligned(64))) float result[64];
    int i;

    // Step A: Temp = Block * T' 
    matrixMul8x8(src_block, DCT_T_TRANSPOSED, temp);

    // Step B: Result = T * Temp 
    matrixMul8x8(DCT_T, temp, result);

    // Store Result 
    #pragma MUST_ITERATE(64, 64, 64)
    for( i = 0; i < 64; i++)
    {
        dct_out[i] = result[i];
    }
}

void computeDCTBlock4x8x8(const int8_t * __restrict src_data, float * __restrict dct_out) 
{
    // Temporary float buffer for a single 8x8 block 
    __attribute__((aligned(64))) float work_block[64];
    
    int k, i;

    // Process exactly 4 blocks 
    #pragma MUST_ITERATE(4, 4, 4)
    for (k = 0; k < 4; k++)
    {
        // Pointer to current input and output block 
        const int8_t *current_src = src_data + (k * 64);
        float *current_dst = dct_out + (k * 64);

        // Convert int8 input samples to float 
        #pragma MUST_ITERATE(64, 64, 64)
        for(i = 0; i < 64; i++)
        {
            work_block[i] = (float)current_src[i];
        }

        // Compute DCT for the current block 
        computeDCTBlock(work_block, current_dst);
    }
}
#endif
