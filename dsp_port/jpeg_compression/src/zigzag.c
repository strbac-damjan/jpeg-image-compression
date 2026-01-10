#ifdef __C7000__
#include "jpeg_compression.h"
#include <c7x.h>

/* -------------------------------------------------------------------------------------
 * ZIG-ZAG LOOKUP TABLE
 * -------------------------------------------------------------------------------------
 * Maps the output index (0..63) to the input Raster index (0..63).
 * e.g. ZIGZAG_ORDER[1] = 1 (horizontal neighbor)
 * ZIGZAG_ORDER[2] = 8 (vertical neighbor)
 */
static const uint8_t ZIGZAG_ORDER[64] = {
    0, 1, 8, 16, 9, 2, 3, 10,
    17, 24, 32, 25, 18, 11, 4, 5,
    12, 19, 26, 33, 40, 48, 41, 34,
    27, 20, 13, 6, 7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36,
    29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46,
    53, 60, 61, 54, 47, 55, 62, 63
};

/**
 * \brief Performs Zig-Zag reordering on a single 8x8 block.
 * * Transforms data from Raster Order (Row-by-Row) to ZigZag Order.
 * * Since inputs are in L1 memory, this is effectively a "Gather" operation.
 *
 * \param quant_block   Input: Linear array of 64 int16_t (Raster Order)
 * \param zigzag_block  Output: Linear array of 64 int16_t (ZigZag Order)
 */
void performZigZagBlock(int16_t *quant_block, int16_t *zigzag_block)
{
    int i;
    // Inform the compiler about the loop structure
    // #pragma MUST_ITERATE(64, 64, 64)
    for (i = 0; i < 64; i++)
    {
        // 1. Get the source index from the Lookup Table
        // This eliminates all complex 2D math (/, %, *)
        uint8_t src_idx = ZIGZAG_ORDER[i];
        
        // 2. Copy the value
        // The compiler will likely generate a "Gather" instruction or optimized loads
        zigzag_block[i] = quant_block[src_idx];
    }
}
#endif
