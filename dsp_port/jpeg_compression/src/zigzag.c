#ifdef __C7000__
#include <stdint.h>
#include <c7x.h>
#include "jpeg_compression.h"

// ==========================================================================
// MASK AND TABLE DEFINITIONS
// ==========================================================================

// Permutation mask for the lower half of the ZigZag output
#pragma DATA_ALIGN(perm_mask_lo, 64)
static uchar64 perm_mask_lo;

// Permutation mask for the upper half of the ZigZag output
#pragma DATA_ALIGN(perm_mask_hi, 64)
static uchar64 perm_mask_hi;

// Reference ZigZag order for an 8x8 block
// Values represent indices in raster order
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

// Wrapper that emulates a two-source vector permute operation
// Bytes are selected from src1 or src2 depending on the mask value
static inline uchar64 __vperm_wrapper(uchar64 mask, uchar64 src1, uchar64 src2)
{
    // Perform permutation assuming src1 as the source
    uchar64 p1 = __vperm_vvv(mask, src1);

    // Perform permutation assuming src2 as the source
    uchar64 p2 = __vperm_vvv(mask, src2);

    // Create a predicate that selects src2 when mask value exceeds 63
    __vpred pred = __cmp_gt_pred(convert_char64(mask), (char64)63);

    // Select between src1 and src2 permutation results
    return __select(pred, p2, p1);
}

// Initializes permutation masks for ZigZag operation
// This function should be called once during program initialization
void init_ZigZag_Masks(void)
{
    uint8_t temp_lo[64];
    uint8_t temp_hi[64];
    int i;

    // Generate mask for the lower half of the ZigZag output
    // Each short consists of two bytes, so both bytes are mapped
    for (i = 0; i < 32; i++) {
        uint8_t src_idx = ZIGZAG_ORDER_REF[i];
        temp_lo[2 * i]     = (uint8_t)(src_idx * 2);
        temp_lo[2 * i + 1] = (uint8_t)(src_idx * 2 + 1);
    }

    // Generate mask for the upper half of the ZigZag output
    for (i = 32; i < 64; i++) {
        uint8_t src_idx = ZIGZAG_ORDER_REF[i];
        int j = i - 32;
        temp_hi[2 * j]     = (uint8_t)(src_idx * 2);
        temp_hi[2 * j + 1] = (uint8_t)(src_idx * 2 + 1);
    }

    // Load masks into vector registers
    perm_mask_lo = *((uchar64 *)temp_lo);
    perm_mask_hi = *((uchar64 *)temp_hi);
}


void performZigZagBlock4x8x8(const int16_t * __restrict src_macro,
                             int16_t * __restrict dst_macro)
{
    int k;

    // Cast input and output pointers to vector types
    // Each short32 vector represents 32 int16 values
    // One 8x8 block occupies exactly two short32 vectors
    const short32 *input_vec_base = (const short32 *)src_macro;
    short32 *output_vec_base      = (short32 *)dst_macro;

    // Process four blocks using loop unrolling
    // Permutation masks are reused for all blocks
    #pragma MUST_ITERATE(4, 4, 4)
    #pragma UNROLL(4)
    for (k = 0; k < 4; k++)
    {
        // Compute vector offset for the current block
        int vec_offset = k * 2;

        // Load two vectors representing one 8x8 block
        short32 v_src0_s = input_vec_base[vec_offset + 0];
        short32 v_src1_s = input_vec_base[vec_offset + 1];

        // Reinterpret short vectors as byte vectors for permutation
        uchar64 v_src0_u = as_uchar64(v_src0_s);
        uchar64 v_src1_u = as_uchar64(v_src1_s);

        // Apply ZigZag permutation using precomputed masks
        uchar64 v_res0_u = __vperm_wrapper(perm_mask_lo, v_src0_u, v_src1_u);
        uchar64 v_res1_u = __vperm_wrapper(perm_mask_hi, v_src0_u, v_src1_u);

        // Store permuted result back as short vectors
        output_vec_base[vec_offset + 0] = as_short32(v_res0_u);
        output_vec_base[vec_offset + 1] = as_short32(v_res1_u);
    }
}
#endif
