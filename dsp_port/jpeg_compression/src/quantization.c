#ifdef __C7000__
#include "jpeg_compression.h"
#include <c7x.h>

// Poravnanje je ključno za VLD (Vector Load) instrukcije
#pragma DATA_ALIGN(RECIP_LUMINANCE_QUANT_TBL, 64)
static const float RECIP_LUMINANCE_QUANT_TBL[64] = {
    0.062500f, 0.090909f, 0.100000f, 0.062500f, 0.041667f, 0.025000f, 0.019608f, 0.016393f,
    0.083333f, 0.083333f, 0.071429f, 0.052632f, 0.038462f, 0.017241f, 0.016667f, 0.018182f,
    0.071429f, 0.076923f, 0.062500f, 0.041667f, 0.025000f, 0.017544f, 0.014493f, 0.017857f,
    0.071429f, 0.058824f, 0.045455f, 0.034483f, 0.019608f, 0.011494f, 0.012500f, 0.016129f,
    0.055556f, 0.045455f, 0.027027f, 0.017857f, 0.014706f, 0.009174f, 0.009709f, 0.012987f,
    0.041667f, 0.028571f, 0.018182f, 0.015625f, 0.012346f, 0.009174f, 0.008850f, 0.010870f,
    0.020408f, 0.015625f, 0.012821f, 0.011494f, 0.009709f, 0.008264f, 0.008333f, 0.009901f,
    0.013889f, 0.010870f, 0.010526f, 0.010204f, 0.008929f, 0.010000f, 0.009709f, 0.010101f
};

/**
 * \brief Kvantizacija za 4 bloka odjednom (4x8x8).
 * \param dct_macro_block Ulaz: 256 floata (4 bloka x 64 koeficijenta), linearno složeni.
 * \param quant_macro_block Izlaz: 256 shortova.
 */
void quantizeBlock4x8x8(float * __restrict dct_macro_block, int16_t * __restrict quant_macro_block)
{
    /* * STRATEGIJA:
     * Imamo 4 bloka (0,1,2,3). Svaki blok ima 64 elementa.
     * Tabela kvantizacije ima 64 elementa.
     * C7000 vektor je 16 floatova. Znači tabela stane u 4 vektora (4x16=64).
     * * Optimizacija: Učitaćemo tabelu u registre JEDNOM (4 vektora) 
     * i koristiti je ponovo za svaki od 4 bloka podataka.
     */

    float16 *v_dct_ptr   = (float16 *)dct_macro_block;
    short16 *v_quant_out = (short16 *)quant_macro_block;
    
    // Pomoćni pointer za učitavanje tabele
    float16 *v_tbl_ptr   = (float16 *)RECIP_LUMINANCE_QUANT_TBL;

    // 1. Preload Quantization Table (4 vektora po 16 floatova)
    // Ovo smanjuje pritisak na load unit jer se tabela ne učitava iz memorije 4 puta.
    float16 q_vec0 = v_tbl_ptr[0];
    float16 q_vec1 = v_tbl_ptr[1];
    float16 q_vec2 = v_tbl_ptr[2];
    float16 q_vec3 = v_tbl_ptr[3];

    int k;

    // Procesiramo 4 bloka (Macro Block)
    // #pragma UNROLL(4) ovdje govori kompajleru da kopira tijelo petlje 4 puta
    // i zamijeni 'k' konstantama, što omogućava savršen pipelining.
    #pragma MUST_ITERATE(4, 4, 4)
    #pragma UNROLL(4)
    for (k = 0; k < 4; k++) 
    {
        // Računamo bazni offset za trenutni blok (u vektorima)
        // Svaki blok ima 4 vektora (64 floata / 16 = 4)
        int offset = k * 4;

        // Vektor 0 u bloku
        float16 val0 = v_dct_ptr[offset + 0];
        float16 res0 = val0 * q_vec0;
        v_quant_out[offset + 0] = __convert_short16(res0); // Koristi _rtn verziju ako je dostupna

        // Vektor 1 u bloku
        float16 val1 = v_dct_ptr[offset + 1];
        float16 res1 = val1 * q_vec1;
        v_quant_out[offset + 1] = __convert_short16(res1);

        // Vektor 2 u bloku
        float16 val2 = v_dct_ptr[offset + 2];
        float16 res2 = val2 * q_vec2;
        v_quant_out[offset + 2] = __convert_short16(res2);

        // Vektor 3 u bloku
        float16 val3 = v_dct_ptr[offset + 3];
        float16 res3 = val3 * q_vec3;
        v_quant_out[offset + 3] = __convert_short16(res3);
    }
}
#endif
