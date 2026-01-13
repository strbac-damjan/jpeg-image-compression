#ifdef __C7000__
#include "jpeg_compression.h"
#include <c7x.h>

// Obavezno: Poravnanje memorije za vektorski pristup (poboljšava load/store)
#pragma DATA_ALIGN(RECIP_LUMINANCE_QUANT_TBL, 64)
static const float RECIP_LUMINANCE_QUANT_TBL[64] = {
    // ... tvoji podaci ostaju isti ...
    0.062500f, 0.090909f, 0.100000f, 0.062500f, 0.041667f, 0.025000f, 0.019608f, 0.016393f,
    0.083333f, 0.083333f, 0.071429f, 0.052632f, 0.038462f, 0.017241f, 0.016667f, 0.018182f,
    0.071429f, 0.076923f, 0.062500f, 0.041667f, 0.025000f, 0.017544f, 0.014493f, 0.017857f,
    0.071429f, 0.058824f, 0.045455f, 0.034483f, 0.019608f, 0.011494f, 0.012500f, 0.016129f,
    0.055556f, 0.045455f, 0.027027f, 0.017857f, 0.014706f, 0.009174f, 0.009709f, 0.012987f,
    0.041667f, 0.028571f, 0.018182f, 0.015625f, 0.012346f, 0.009615f, 0.008850f, 0.010870f,
    0.020408f, 0.015625f, 0.012821f, 0.011494f, 0.009709f, 0.008264f, 0.008333f, 0.009901f,
    0.013889f, 0.010870f, 0.010526f, 0.010204f, 0.008929f, 0.010000f, 0.009709f, 0.010101f
};

void quantizeBlock(float * __restrict dct_block, int16_t * __restrict quant_block)
{
    int i;
    // 1. Kastovanje pointera u vektorske pointere
    // float16 označava vektor od 16 float vrednosti (512 bita)
    float16 *v_dct   = (float16 *)dct_block;
    float16 *v_quant = (float16 *)RECIP_LUMINANCE_QUANT_TBL;
    short16 *v_out   = (short16 *)quant_block;

    // 2. Petlja se sada izvršava samo 4 puta (64 elementa / 16 po vektoru)
    // Kompajler će ovo verovatno potpuno "odmotati" (loop unrolling)
    #pragma MUST_ITERATE(4, 4, 4)
    for ( i = 0; i < 4; i++) {
        // Učitavanje 16 float vrednosti odjednom
        float16 val = v_dct[i];
        float16 q   = v_quant[i];

        // Vektorsko množenje (16 operacija u 1 ciklusu)
        float16 scaled = val * q;

        // 3. KLJUČNA IZMENA: Zamena za roundf()
        // __convert_short16_rtn konvertuje float vektor u short vektor
        // koristeći "Round To Nearest" (isto što i roundf) ali hardverski.
        v_out[i] = __convert_short16(scaled);
    }
}
#endif
