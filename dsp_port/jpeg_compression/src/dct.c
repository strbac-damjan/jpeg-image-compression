#ifdef __C7000__
#include "jpeg_compression.h"
#include <math.h>

/* AAN konstante */
#define AAN_C1  0.707106781f
#define AAN_C2  0.382683433f
#define AAN_C3  0.541196100f
#define AAN_C4  1.306562965f

/**
 * AAN algoritam (In-place).
 * Ovo je "Fast Integer DCT" portovan na float.
 * Rezultat NIJE normalizovan (vrijednosti su 8x vece od standardnog DCT).
 */
static inline void aanDCT8x8(float *data)
{
    float tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
    float tmp10, tmp11, tmp12, tmp13;
    float z1, z2, z3, z4, z5, z11, z13;
    float *dataptr;
    int ctr;

    /* Pass 1: Rows */
    dataptr = data;
    #pragma MUST_ITERATE(8, 8, 8)
    for (ctr = 0; ctr < 8; ctr++) {
        tmp0 = dataptr[0] + dataptr[7];
        tmp7 = dataptr[0] - dataptr[7];
        tmp1 = dataptr[1] + dataptr[6];
        tmp6 = dataptr[1] - dataptr[6];
        tmp2 = dataptr[2] + dataptr[5];
        tmp5 = dataptr[2] - dataptr[5];
        tmp3 = dataptr[3] + dataptr[4];
        tmp4 = dataptr[3] - dataptr[4];

        tmp10 = tmp0 + tmp3;
        tmp13 = tmp0 - tmp3;
        tmp11 = tmp1 + tmp2;
        tmp12 = tmp1 - tmp2;

        dataptr[0] = tmp10 + tmp11;
        dataptr[4] = tmp10 - tmp11;

        z1 = (tmp12 + tmp13) * AAN_C1;
        dataptr[2] = tmp13 + z1;
        dataptr[6] = tmp13 - z1;

        tmp10 = tmp4 + tmp5;
        tmp11 = tmp5 + tmp6;
        tmp12 = tmp6 + tmp7;

        z5 = (tmp10 - tmp12) * AAN_C2;
        z2 = (tmp10 * AAN_C3) + z5;
        z4 = (tmp12 * AAN_C4) + z5;
        z3 = tmp11 * AAN_C1;

        z11 = tmp7 + z3;
        z13 = tmp7 - z3;

        dataptr[5] = z13 + z2;
        dataptr[3] = z13 - z2;
        dataptr[1] = z11 + z4;
        dataptr[7] = z11 - z4;

        dataptr += 8;
    }

    /* Pass 2: Columns */
    dataptr = data;
    #pragma MUST_ITERATE(8, 8, 8)
    for (ctr = 0; ctr < 8; ctr++) {
        tmp0 = dataptr[8*0] + dataptr[8*7];
        tmp7 = dataptr[8*0] - dataptr[8*7];
        tmp1 = dataptr[8*1] + dataptr[8*6];
        tmp6 = dataptr[8*1] - dataptr[8*6];
        tmp2 = dataptr[8*2] + dataptr[8*5];
        tmp5 = dataptr[8*2] - dataptr[8*5];
        tmp3 = dataptr[8*3] + dataptr[8*4];
        tmp4 = dataptr[8*3] - dataptr[8*4];

        tmp10 = tmp0 + tmp3;
        tmp13 = tmp0 - tmp3;
        tmp11 = tmp1 + tmp2;
        tmp12 = tmp1 - tmp2;

        dataptr[8*0] = tmp10 + tmp11;
        dataptr[8*4] = tmp10 - tmp11;

        z1 = (tmp12 + tmp13) * AAN_C1;
        dataptr[8*2] = tmp13 + z1;
        dataptr[8*6] = tmp13 - z1;

        tmp10 = tmp4 + tmp5;
        tmp11 = tmp5 + tmp6;
        tmp12 = tmp6 + tmp7;

        z5 = (tmp10 - tmp12) * AAN_C2;
        z2 = (tmp10 * AAN_C3) + z5;
        z4 = (tmp12 * AAN_C4) + z5;
        z3 = tmp11 * AAN_C1;

        z11 = tmp7 + z3;
        z13 = tmp7 - z3;

        dataptr[8*5] = z13 + z2;
        dataptr[8*3] = z13 - z2;
        dataptr[8*1] = z11 + z4;
        dataptr[8*7] = z11 - z4;

        dataptr++;
    }
}

void computeDCTBlock4x8x8(const int8_t * __restrict src_data, float * __restrict dct_out, int32_t stride) 
{
    int k, r, c, i;

    // Process 4 blocks
    #pragma MUST_ITERATE(4, 4, 4)
    for (k = 0; k < 4; k++)
    {
        const int8_t *block_src = src_data + (k * 8);
        float *block_dst        = dct_out + (k * 64);

        /* 1. LOAD & CONVERT (NO LEVEL SHIFT)
         * Posto su podaci vec centralizovani (-128..127), samo ih kastujemo u float.
         * Nema oduzimanja 128.0f.
         */
        for (r = 0; r < 8; r++) {
            const int8_t *row_ptr = block_src + (r * stride);
            for (c = 0; c < 8; c++) {
                block_dst[r*8 + c] = (float)row_ptr[c];
            }
        }

        /* 2. COMPUTE AAN DCT */
        aanDCT8x8(block_dst);

        /* 3. NORMALIZE (SCALING) - KLJUČNO!
         * AAN algoritam vraca sume koje su 8x vece od standardnog DCT-a.
         * MatrixMul metoda je imala "ugradjenu" normalizaciju (1/sqrt(8)).
         * Ovdje moramo rucno podijeliti sa 8 da bi rezultat bio kompatibilan
         * sa tvojim postojecim Huffman/Quantization koracima.
         */
        #pragma MUST_ITERATE(64, 64, 64)
        for( i = 0; i < 64; i++) {
            block_dst[i] *= 0.125f; // Mnozenje sa 1/8
        }
    }
}
#endif
