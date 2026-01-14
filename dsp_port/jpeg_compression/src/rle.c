#ifdef __C7000__
#include "jpeg_compression.h"
#include <c7x.h>
#include <stdint.h> 

// --- FAST MATH HELPERS ---

// Calculates the magnitude category (number of bits required) for a coefficient
static inline uint8_t getBitLength(int16_t val) {
    if (val == 0) return 0;
    int32_t v32 = (int32_t)__abs(val);
    // Use the norm intrinsic to find the position of the highest bit
    return (uint8_t)(32 - __norm(v32) - 1);
}

// Converts the value to the standard JPEG amplitude code
// For positive numbers, it is the number itself
// For negative numbers, it is the 1's complement
static inline uint16_t getAmplitudeCode(int16_t val) {
    int16_t mask = val >> 15; 
    return (uint16_t)(val + mask);
}

// Portable Count Trailing Zeros (64-bit) implementation
// Returns the index of the least significant set bit
static inline int ctz_64(uint64_t x)
{
    if (x == 0) return 64;
    int n = 0;
    // Binary search for the first set bit
    if ((x & 0xFFFFFFFF) == 0) { n += 32; x >>= 32; }
    if ((x & 0x0000FFFF) == 0) { n += 16; x >>= 16; }
    if ((x & 0x000000FF) == 0) { n += 8;  x >>= 8; }
    if ((x & 0x0000000F) == 0) { n += 4;  x >>= 4; }
    if ((x & 0x00000003) == 0) { n += 2;  x >>= 2; }
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
    
    // Ensure sufficient buffer space
    if (max_capacity < 256) return -1;

    // Process 4 blocks of 8x8 coefficients
    for (blk = 0; blk < 4; blk++)
    {
        const int16_t *current_block_ptr = macro_zigzag_buffer + (blk * 64);
        
        // --- DC Coefficient Processing ---
        
        // Calculate the difference between the current DC and the previous block's DC
        int16_t currentDC = current_block_ptr[0];
        int16_t diff      = currentDC - *last_dc_ptr;
        *last_dc_ptr      = currentDC;

        uint8_t dcSize = getBitLength(diff);
        RLESymbol *sym = &rle_out[total_symbols++];
        sym->symbol   = dcSize;
        sym->code     = getAmplitudeCode(diff);
        sym->codeBits = dcSize;

        // --- C7x Vectorized Bit-Scan ---
        
        // Load the 8x8 block as two vectors of 32 short integers each
        short32 v_lo = *((short32 *)&current_block_ptr[0]);
        short32 v_hi = *((short32 *)&current_block_ptr[32]);

        // Generate predicates indicating which elements are equal to zero
        __vpred pred_lo_zeros = __cmp_eq_pred(v_lo, (short32)0);
        __vpred pred_hi_zeros = __cmp_eq_pred(v_hi, (short32)0);

        // Convert the vector predicates into 64-bit scalar integers
        // Note: The C7x intrinsic creates a bitmask where 1 bit corresponds to 1 BYTE
        // Since we are using shorts (2 bytes), index 0 covers bits 0 and 1 in the raw mask
        uint64_t raw_lo = (uint64_t)__create_scalar(pred_lo_zeros);
        uint64_t raw_hi = (uint64_t)__create_scalar(pred_hi_zeros);

        // Consolidate bits from Byte-granularity to Short-granularity
        // If a short is zero, both of its constituent bytes are zero
        // This means we look for pairs of set bits (e.g., bits 0 and 1 are both 1)
        // We shift right by 1 and AND with the original to detect the pair
        uint64_t mask_const = 0x5555555555555555ULL;
        
        // Create a mask where '1' on an even bit position indicates the element is ZERO
        uint64_t is_zero_lo = raw_lo & (raw_lo >> 1) & mask_const;
        uint64_t is_zero_hi = raw_hi & (raw_hi >> 1) & mask_const;

        // Invert the mask to find NON-ZERO elements
        // We are still only interested in the even bit positions (0, 2, 4...)
        uint64_t nz_mask_lo = (~is_zero_lo) & mask_const;
        uint64_t nz_mask_hi = (~is_zero_hi) & mask_const;

        // Mask out the DC coefficient (index 0 corresponds to bit 0 in the low mask)
        // We have already processed DC above
        nz_mask_lo &= ~1ULL; 

        // --- Pass 2: Process Non-Zero AC Coefficients ---
        
        int last_k = 0;

        // Process the first 32 elements (Indices 0-31)
        while (nz_mask_lo != 0)
        {
            // Find the position of the next non-zero element
            int bit_idx = ctz_64(nz_mask_lo); // Returns even indices like 0, 2, 4...
            nz_mask_lo &= (nz_mask_lo - 1);   // Clear the found bit

            // Convert bit index to array index (divide by 2 because of the short/byte gap)
            int curr_k = bit_idx >> 1; 

            int16_t val = current_block_ptr[curr_k];
            
            // Calculate run-length of zeros preceding this coefficient
            int zero_run = curr_k - last_k - 1;

            // Handle run lengths exceeding 15 (ZRL - Zero Run Length)
            if (zero_run >= 16) {
                int num_zrl = zero_run >> 4;
                zero_run    = zero_run & 0xF;
                for(i=0; i<num_zrl; i++) {
                   RLESymbol *z = &rle_out[total_symbols++];
                   z->symbol = 0xF0; z->code = 0; z->codeBits = 0;
                }
            }
            
            // Emit the AC symbol
            uint8_t size = getBitLength(val);
            RLESymbol *ac = &rle_out[total_symbols++];
            ac->symbol   = (uint8_t)((zero_run << 4) | size);
            ac->code     = getAmplitudeCode(val);
            ac->codeBits = size;

            last_k = curr_k;
        }

        // Process the second 32 elements (Indices 32-63)
        while (nz_mask_hi != 0)
        {
            int bit_idx = ctz_64(nz_mask_hi);
            nz_mask_hi &= (nz_mask_hi - 1);

            // Calculate array index: 32 offset + relative index
            int curr_k = 32 + (bit_idx >> 1); 

            int16_t val = current_block_ptr[curr_k];
            int zero_run = curr_k - last_k - 1;

            // Handle ZRL markers for long runs of zeros
            if (zero_run >= 16) {
                int num_zrl = zero_run >> 4;
                zero_run    = zero_run & 0xF;
                for(i=0; i<num_zrl; i++) {
                   RLESymbol *z = &rle_out[total_symbols++];
                   z->symbol = 0xF0; z->code = 0; z->codeBits = 0;
                }
            }
            
            // Emit the AC symbol
            uint8_t size = getBitLength(val);
            RLESymbol *ac = &rle_out[total_symbols++];
            ac->symbol   = (uint8_t)((zero_run << 4) | size);
            ac->code     = getAmplitudeCode(val);
            ac->codeBits = size;

            last_k = curr_k;
        }

        // --- End of Block (EOB) ---
        // If the last non-zero coefficient was not the final element (63), write EOB
        if (last_k < 63) {
            RLESymbol *eob = &rle_out[total_symbols++];
            eob->symbol = 0x00; eob->code = 0; eob->codeBits = 0;
        }
    }

    return total_symbols;
}
#endif
