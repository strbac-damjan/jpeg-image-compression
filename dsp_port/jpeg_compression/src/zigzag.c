#ifdef __C7000__
#include <stdint.h>
#include "jpeg_compression.h"

/* ========================================================================== */
/* DEFINICIJA MASKI                                                           */
/* ========================================================================== */
#pragma DATA_ALIGN(perm_mask_lo, 64)
static uchar64 perm_mask_lo;

#pragma DATA_ALIGN(perm_mask_hi, 64)
static uchar64 perm_mask_hi;

static const uint8_t ZIGZAG_ORDER_REF[64] = {
    0, 1, 8, 16, 9, 2, 3, 10,
    17, 24, 32, 25, 18, 11, 4, 5,
    12, 19, 26, 33, 40, 48, 41, 34,
    27, 20, 13, 6, 7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36,
    29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46,
    53, 60, 61, 54, 47, 55, 62, 63
};

/* ========================================================================== */
/* HELPER FUNKCIJA ZA REŠAVANJE PROBLEMA SA 3 ARGUMENTA                       */
/* ========================================================================== */
/* Ova funkcija simulira "vperm" sa dva izvora.
   Logika: 
   1. Ako je indeks u maski < 64, uzimamo bajt iz src1.
   2. Ako je indeks u maski >= 64, uzimamo bajt iz src2.
*/
static inline uchar64 __vperm_wrapper(uchar64 mask, uchar64 src1, uchar64 src2)
{
    // Permutujemo oba vektora koristeći istu masku
    // (C7x vperm uzima samo donjih 6 bitova indeksa, pa 64 postaje 0, što je ok)
    uchar64 p1 = __vperm_vvv(mask, src1);
    uchar64 p2 = __vperm_vvv(mask, src2);

    // Kreiramo predikat: Gde god je maska >= 64 (bit 6 setovan), biramo src2
    // Koristimo shift da proverimo 6. bit (vrednost 64)
    // Ako je mask >> 6 != 0, to znači da je indeks >= 64
    __vpred pred = __cmp_gt_pred(convert_char64(mask), (char64)63); 

    // Selektujemo pravi rezultat na osnovu predikata
    return __select(pred, p2, p1);
}

/* ========================================================================== */
/* INICIJALIZACIJA                                                            */
/* ========================================================================== */
void init_ZigZag_Masks(void)
{
    uint8_t temp_lo[64];
    uint8_t temp_hi[64];
    int i;

    for (i = 0; i < 32; i++) {
        uint8_t src_idx = ZIGZAG_ORDER_REF[i];
        temp_lo[2 * i]     = (uint8_t)(src_idx * 2);
        temp_lo[2 * i + 1] = (uint8_t)(src_idx * 2 + 1);
    }

    for (i = 32; i < 64; i++) {
        uint8_t src_idx = ZIGZAG_ORDER_REF[i];
        int j = i - 32;
        temp_hi[2 * j]     = (uint8_t)(src_idx * 2);
        temp_hi[2 * j + 1] = (uint8_t)(src_idx * 2 + 1);
    }

    perm_mask_lo = *((uchar64 *)temp_lo);
    perm_mask_hi = *((uchar64 *)temp_hi);
}

/* ========================================================================== */
/* RUNTIME FUNKCIJA                                                           */
/* ========================================================================== */
void performZigZagBlock(const int16_t * __restrict quant_block, int16_t * __restrict zigzag_block)
{
    const short32 *input_vec_ptr = (const short32 *)quant_block;
    short32 *output_vec_ptr      = (short32 *)zigzag_block;

    // 1. Učitavanje
    short32 v_src0_s = input_vec_ptr[0]; 
    short32 v_src1_s = input_vec_ptr[1];

    // 2. Konverzija u uchar64
    uchar64 v_src0_u = as_uchar64(v_src0_s);
    uchar64 v_src1_u = as_uchar64(v_src1_s);

    // 3. Vektorska permutacija (koristimo naš wrapper)
    // Sada ovo radi jer wrapper prima 3 argumenta i spaja rezultate
    uchar64 v_res0_u = __vperm_wrapper(perm_mask_lo, v_src0_u, v_src1_u);
    uchar64 v_res1_u = __vperm_wrapper(perm_mask_hi, v_src0_u, v_src1_u);

    // 4. Upis
    output_vec_ptr[0] = as_short32(v_res0_u);
    output_vec_ptr[1] = as_short32(v_res1_u);
}
#endif
