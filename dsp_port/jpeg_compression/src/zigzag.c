#ifdef __C7000__
#include <stdint.h>
#include <c7x.h>
#include "jpeg_compression.h"

/* ========================================================================== */
/* DEFINICIJA MASKI I TABELA                                                  */
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
/* HELPER FUNKCIJA (INLINE)                                                   */
/* ========================================================================== */
/* Simulira vperm sa 2 izvora. Selektuje bajtove iz src1 ili src2 zavisno od maske */
static inline uchar64 __vperm_wrapper(uchar64 mask, uchar64 src1, uchar64 src2)
{
    // C7x __vperm_vvv koristi samo donjih 6 bita indeksa (modulo 64)
    uchar64 p1 = __vperm_vvv(mask, src1);
    uchar64 p2 = __vperm_vvv(mask, src2);

    // Kreiramo predikat: Ako je bajt u maski > 63 (bit 6 setovan), trebamo src2.
    // Inace trebamo src1.
    __vpred pred = __cmp_gt_pred(convert_char64(mask), (char64)63); 

    // Selektujemo pravi rezultat
    return __select(pred, p2, p1);
}

/* ========================================================================== */
/* INICIJALIZACIJA (POZVATI JEDNOM U MAIN-u)                                  */
/* ========================================================================== */
void init_ZigZag_Masks(void)
{
    uint8_t temp_lo[64];
    uint8_t temp_hi[64];
    int i;

    // Generisanje maske za donji deo (prva 32 shorta / 64 bajta izlaza)
    for (i = 0; i < 32; i++) {
        uint8_t src_idx = ZIGZAG_ORDER_REF[i];
        // Short se sastoji od 2 bajta, pa moramo mapirati oba
        temp_lo[2 * i]     = (uint8_t)(src_idx * 2);     // Lo byte
        temp_lo[2 * i + 1] = (uint8_t)(src_idx * 2 + 1); // Hi byte
    }

    // Generisanje maske za gornji deo (druga 32 shorta izlaza)
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
/* 4x8x8 ZIGZAG IMPLEMENTACIJA                                                */
/* ========================================================================== */
/**
 * \brief Performs Zig-Zag on 4 blocks (Macro Block) using Vector Permute.
 * \param src_macro Input: 256 int16_t (Linear Raster Order)
 * \param dst_macro Output: 256 int16_t (ZigZag Order)
 */
void performZigZagBlock4x8x8(const int16_t * __restrict src_macro, int16_t * __restrict dst_macro)
{
    int k;

    /* * Pointeri se kastuju u short32 (vektor od 64 bajta / 32 shorta).
     * Svaki 8x8 blok (64 shorta) zauzima TAČNO 2 vektora (short32).
     */
    const short32 *input_vec_base = (const short32 *)src_macro;
    short32 *output_vec_base      = (short32 *)dst_macro;

    /* * UNROLL(4): Kompajler će generisati kod koji paralelno obrađuje 4 bloka.
     * Maske (perm_mask_lo/hi) su već u registrima i koriste se za sve blokove.
     */
    #pragma MUST_ITERATE(4, 4, 4)
    #pragma UNROLL(4)
    for (k = 0; k < 4; k++)
    {
        // Svaki blok zauzima 2 vektora (k*2)
        int vec_offset = k * 2;

        // 1. UČITAVANJE (2 vektora po bloku = 64 shorta)
        short32 v_src0_s = input_vec_base[vec_offset + 0]; 
        short32 v_src1_s = input_vec_base[vec_offset + 1];

        // 2. REINTERPRETACIJA (u bajtove za vperm)
        uchar64 v_src0_u = as_uchar64(v_src0_s);
        uchar64 v_src1_u = as_uchar64(v_src1_s);

        // 3. PERMUTACIJA (Srce optimizacije)
        // Koristimo iste maske za svaki blok! Ovo je ogromna ušteda.
        uchar64 v_res0_u = __vperm_wrapper(perm_mask_lo, v_src0_u, v_src1_u);
        uchar64 v_res1_u = __vperm_wrapper(perm_mask_hi, v_src0_u, v_src1_u);

        // 4. UPIS
        output_vec_base[vec_offset + 0] = as_short32(v_res0_u);
        output_vec_base[vec_offset + 1] = as_short32(v_res1_u);
    }
}
#endif
