#ifdef __C7000__
#include "jpeg_compression.h"
#include <c7x.h>

#pragma DATA_ALIGN(RECIP_LUMINANCE_QUANT_TBL, 64)
static const int16_t RECIP_LUMINANCE_QUANT_TBL[64] = {
    2048, 2978, 3276, 2048, 1365, 819, 642, 537,
    2730, 2730, 2340, 1724, 1260, 564, 546, 595,
    2340, 2520, 2048, 1365, 819, 574, 474, 585,
    2340, 1927, 1489, 1129, 642, 376, 409, 528,
    1820, 1489, 885, 585, 481, 300, 318, 425,
    1365, 936, 595, 512, 404, 315, 289, 356,
    668, 512, 420, 376, 318, 270, 273, 324,
    455, 356, 344, 334, 292, 327, 318, 330
};

void quantizeBlock(float *dct_block, int16_t *quant_block)
{
    float16 *v_dct_in = (float16 *)dct_block;
    short16 *v_quant_tbl = (short16 *)RECIP_LUMINANCE_QUANT_TBL;
    short16 *v_out = (short16 *)quant_block;

    // Koristimo shift koji odgovara Q15 formatu
    int16 v_shift = (int16)15;
    int i;
    #pragma MUST_ITERATE(4, 4, 4)
    #pragma UNROLL(4)
    for ( i = 0; i < 4; i++) 
    {
        float16 val_f = v_dct_in[i];
        
        // 1. Float -> Int32
        int16 val_i = __convert_int16(val_f); 
        
        // 2. Load Table
        int16 q_i = __convert_int16(v_quant_tbl[i]);

        // 3. Množenje
        int16 product = val_i * q_i;

        // 4. Shift BEZ sabiranja (Truncation)
        // Ovo spaja Mult i Shift bliže u pipeline-u jer nema VADDW između
        int16 res_32 = product >> v_shift;

        // 5. Pack
        v_out[i] = __convert_short16(res_32);
    }
}
#endif
