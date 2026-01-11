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
 * \brief Computes DCT for a single 8x8 block.
 */
void computeDCTBlock(int8_t * __restrict src_data, float * __restrict dct_out, int32_t stride)
{
    /* OPTIMIZACIJA: Koristimo linearne nizove od 64 elementa umjesto 2D [8][8].
     * Poravnanje na 64 bajta je kljucno za VLD/VST instrukcije.
     */
    __attribute__((aligned(64))) float block[64];
    __attribute__((aligned(64))) float temp[64];
    __attribute__((aligned(64))) float result[64];
    
    int i, j;

    /* 1. Load Data & Convert */
    #pragma MUST_ITERATE(8, 8, 8)
    for( i = 0; i < 8; i++)
    {
        /* Pokazivač na početak reda u ulaznom bufferu */
        int8_t *row_ptr = src_data + (i * stride);

        #pragma MUST_ITERATE(8, 8, 8)
        #pragma UNROLL(8)
        for( j = 0; j < 8; j++)
        {
            /* Linearni upis u block buffer */
            block[i*8 + j] = (float)row_ptr[j];
        }
    }

    /* * DCT Calculation: Result = T * Block * T' */

    /* Step A: Temp = Block * T' */
    matrixMul8x8(block, DCT_T_TRANSPOSED, temp);

    /* Step B: Result = T * Temp */
    matrixMul8x8(DCT_T, temp, result);

    /* 2. Store Result (Linear Store) */
    /* Sada su oba pointera (result i dct_out) linearni nizovi float-ova.
     * Nema vise type mismatch greske.
     */
    #pragma MUST_ITERATE(64, 64, 64)
    for( i = 0; i < 64; i++)
    {
        dct_out[i] = result[i];
    }
}
#endif
