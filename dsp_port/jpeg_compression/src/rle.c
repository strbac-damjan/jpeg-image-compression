#ifdef __C7000__
#include "jpeg_compression.h"
#include <c7x.h>
#include <stdint.h>

// --- FAST MATH HELPERS ---

// Computes the magnitude category
// Returns the number of bits required to represent the absolute value
static inline uint8_t getBitLength(int16_t val)
{
    if (val == 0) return 0;

    // Convert to 32-bit and take absolute value
    int32_t v32 = (int32_t)__abs(val);

    // Use the norm intrinsic to find the position of the highest set bit
    return (uint8_t)(32 - __norm(v32) - 1);
}

// Converts a coefficient value into the JPEG amplitude code
// Positive values are returned unchanged
// Negative values are converted using one's complement logic
static inline uint16_t getAmplitudeCode(int16_t val)
{
    // Arithmetic shift propagates the sign bit
    int16_t mask = val >> 15;
    return (uint16_t)(val + mask);
}

// Portable count trailing zeros implementation for 64-bit values
// Returns the index of the least significant set bit
static inline int ctz_64(uint64_t x)
{
    if (x == 0) return 64;

    int n = 0;

    // Binary search for the first set bit
    if ((x & 0xFFFFFFFF) == 0) { n += 32; x >>= 32; }
    if ((x & 0x0000FFFF) == 0) { n += 16; x >>= 16; }
    if ((x & 0x000000FF) == 0) { n += 8;  x >>= 8;  }
    if ((x & 0x0000000F) == 0) { n += 4;  x >>= 4;  }
    if ((x & 0x00000003) == 0) { n += 2;  x >>= 2;  }
    if ((x & 0x00000001) == 0) { n += 1; }

    return n;
}

int32_t performRLEBlock4x8x8(const int16_t * __restrict macro_zigzag_buffer,
                             RLESymbol * __restrict rle_out,
                             int32_t max_capacity,
                             int16_t *last_dc_ptr)
{
    int32_t total_symbols = 0;
    int blk, i;

    // Ensure that the output buffer is large enough
    if (max_capacity < 256) return -1;

    // Process four 8x8 blocks
    for (blk = 0; blk < 4; blk++)
    {
        // Pointer to the current block in ZigZag order
        const int16_t *current_block_ptr = macro_zigzag_buffer + (blk * 64);

        // --- DC COEFFICIENT PROCESSING ---

        // Compute DC difference relative to the previous block
        int16_t currentDC = current_block_ptr[0];
        int16_t diff      = currentDC - *last_dc_ptr;
        *last_dc_ptr      = currentDC;

        // Encode DC symbol
        uint8_t dcSize = getBitLength(diff);
        RLESymbol *sym = &rle_out[total_symbols++];
        sym->symbol   = dcSize;
        sym->code     = getAmplitudeCode(diff);
        sym->codeBits = dcSize;

        // --- VECTORIZED ZERO DETECTION USING C7X ---

        // Load the block as two vectors of 32 short elements
        short32 v_lo = *((short32 *)&current_block_ptr[0]);
        short32 v_hi = *((short32 *)&current_block_ptr[32]);

        // Generate predicates indicating zero elements
        __vpred pred_lo_zeros = __cmp_eq_pred(v_lo, (short32)0);
        __vpred pred_hi_zeros = __cmp_eq_pred(v_hi, (short32)0);

        // Convert predicates to scalar bitmasks
        // Each bit corresponds to one byte in the vector
        uint64_t raw_lo = (uint64_t)__create_scalar(pred_lo_zeros);
        uint64_t raw_hi = (uint64_t)__create_scalar(pred_hi_zeros);

        // Reduce byte-level mask to short-level mask
        // A short is zero only if both of its bytes are zero
        uint64_t mask_const = 0x5555555555555555ULL;
        uint64_t is_zero_lo = raw_lo & (raw_lo >> 1) & mask_const;
        uint64_t is_zero_hi = raw_hi & (raw_hi >> 1) & mask_const;

        // Invert masks to identify non-zero elements
        uint64_t nz_mask_lo = (~is_zero_lo) & mask_const;
        uint64_t nz_mask_hi = (~is_zero_hi) & mask_const;

        // Exclude the DC coefficient which was already processed
        nz_mask_lo &= ~1ULL;

        // --- AC COEFFICIENT PROCESSING ---

        int last_k = 0;

        // Process coefficients with indices 0 to 31
        while (nz_mask_lo != 0)
        {
            // Find next non-zero coefficient
            int bit_idx = ctz_64(nz_mask_lo);
            nz_mask_lo &= (nz_mask_lo - 1);

            // Convert bit index to coefficient index
            int curr_k = bit_idx >> 1;
            int16_t val = current_block_ptr[curr_k];

            // Compute number of preceding zeros
            int zero_run = curr_k - last_k - 1;

            // Emit Zero Run Length symbols if required
            if (zero_run >= 16)
            {
                int num_zrl = zero_run >> 4;
                zero_run    = zero_run & 0xF;
                for (i = 0; i < num_zrl; i++)
                {
                    RLESymbol *z = &rle_out[total_symbols++];
                    z->symbol = 0xF0;
                    z->code = 0;
                    z->codeBits = 0;
                }
            }

            // Emit AC symbol
            uint8_t size = getBitLength(val);
            RLESymbol *ac = &rle_out[total_symbols++];
            ac->symbol   = (uint8_t)((zero_run << 4) | size);
            ac->code     = getAmplitudeCode(val);
            ac->codeBits = size;

            last_k = curr_k;
        }

        // Process coefficients with indices 32 to 63
        while (nz_mask_hi != 0)
        {
            int bit_idx = ctz_64(nz_mask_hi);
            nz_mask_hi &= (nz_mask_hi - 1);

            // Add offset for the second half of the block
            int curr_k = 32 + (bit_idx >> 1);
            int16_t val = current_block_ptr[curr_k];

            int zero_run = curr_k - last_k - 1;

            // Emit Zero Run Length symbols if needed
            if (zero_run >= 16)
            {
                int num_zrl = zero_run >> 4;
                zero_run    = zero_run & 0xF;
                for (i = 0; i < num_zrl; i++)
                {
                    RLESymbol *z = &rle_out[total_symbols++];
                    z->symbol = 0xF0;
                    z->code = 0;
                    z->codeBits = 0;
                }
            }

            // Emit AC symbol
            uint8_t size = getBitLength(val);
            RLESymbol *ac = &rle_out[total_symbols++];
            ac->symbol   = (uint8_t)((zero_run << 4) | size);
            ac->code     = getAmplitudeCode(val);
            ac->codeBits = size;

            last_k = curr_k;
        }

        // Emit End Of Block symbol if trailing zeros remain
        if (last_k < 63)
        {
            RLESymbol *eob = &rle_out[total_symbols++];
            eob->symbol = 0x00;
            eob->code = 0;
            eob->codeBits = 0;
        }
    }

    return total_symbols;
}
#endif
