#ifdef __C7000__
#include <c7x.h>
#include "jpeg_compression.h"
#include <math.h>

/* --- DCT CONSTANTS (Global in .c file) --- */
/* T Matrix */
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
/* T Transpose */
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
 */static inline void matrixMul8x8(const float * __restrict A, const float * __restrict B, float * __restrict C)
{
    int i, j, k;
    
    #pragma MUST_ITERATE(8, 8, 8)
    for ( i = 0; i < 8; i++) 
    {
        #pragma MUST_ITERATE(8, 8, 8)
        for ( j = 0; j < 8; j++) 
        {
            float sum = 0.0f; /* FIX: Deklaracija varijable PRIJE pragme za inner loop */

            #pragma MUST_ITERATE(8, 8, 8)
            #pragma UNROLL(8)
            for ( k = 0; k < 8; k++) 
            {
                /* Pristup linearnom nizu: row*8 + col */
                sum += A[i*8 + k] * B[k*8 + j];
            }
            C[i*8 + j] = sum;
        }
    }
}

/**
 * \brief Computes DCT for a single 8x8 block (Float Input).
 * \param src_block Pointer to 8x8 float block (linear, 64 elements).
 * \param dct_out   Pointer to output float buffer (linear, 64 elements).
 */
static void computeDCTBlock(const float * __restrict src_block, float * __restrict dct_out)
{
    /* Aligned buffers for intermediate results */
    __attribute__((aligned(64))) float temp[64];
    __attribute__((aligned(64))) float result[64];
    
    int i;

    /* * VIŠE NEMA KONVERZIJE OVDJE. 
     * Pretpostavljamo da je 'src_block' već float i linearan (0..63).
     */

    /* Step A: Temp = Block * T' */
    /* src_block je A, DCT_T_TRANSPOSED je B */
    matrixMul8x8(src_block, DCT_T_TRANSPOSED, temp);

    /* Step B: Result = T * Temp */
    /* DCT_T je A, temp je B */
    matrixMul8x8(DCT_T, temp, result);

    /* Store Result */
    #pragma MUST_ITERATE(64, 64, 64)
    for( i = 0; i < 64; i++)
    {
        dct_out[i] = result[i];
    }
}
/**
 * \brief Computes DCT for a 32x8 Macro Block.
 * Handles int8 -> float conversion here.
 */
void computeDCTBlock4x8x8(const int8_t * __restrict src_data, float * __restrict dct_out, int32_t stride) 
{
    /* * Buffer za jedan 8x8 blok u float formatu.
     * Alociramo ga na stacku s poravnanjem za vektorske instrukcije.
     */
    __attribute__((aligned(64))) float work_block[64];
    
    int k, r, c;

    /* Procesiramo 4 bloka horizontalno (0, 1, 2, 3) */
    #pragma MUST_ITERATE(4, 4, 4)
    #pragma UNROLL(4) // Pokušaj unrollati vanjsku petlju ako compiler dozvoli
    for (k = 0; k < 4; k++)
    {
        /* * 1. LOAD & CONVERT
         * Izvlačimo 8x8 blok iz 32x8 int8 buffera i prebacujemo u float buffer.
         */
        
        // Pointer na početak trenutnog 8x8 bloka u int8 bufferu
        const int8_t *block_src_start = src_data + (k * 8); 

        #pragma MUST_ITERATE(8, 8, 8)
        #pragma UNROLL(8)
        for(r = 0; r < 8; r++)
        {
            // Pointer na trenutni red unutar bloka
            const int8_t *row_ptr = block_src_start + (r * stride);
            
            #pragma MUST_ITERATE(8, 8, 8)
            for(c = 0; c < 8; c++)
            {
                // Linearni index za work_block (0..63)
                work_block[r*8 + c] = (float)row_ptr[c];
            }
        }

        /* * 2. COMPUTE DCT
         * Pozivamo ažuriranu funkciju koja prima float.
         * Output ide direktno u završni buffer.
         */
        computeDCTBlock(work_block, dct_out + (k * 64));
    }
}
#endif
