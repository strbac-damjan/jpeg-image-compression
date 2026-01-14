#ifdef __C7000__
#include "jpeg_compression.h"
#include <c7x.h> 

/* -------------------------------------------------------------------------------------
 * HELPER FUNCTIONS 
 * -------------------------------------------------------------------------------------
 */

static inline uint8_t getBitLength(int16_t val) 
{
    if (val == 0) return 0;
    
    /* * __abs(x) -> Absolute value
     * __norm(x) -> Count Leading Zeros/Ones).
     */
    int32_t v32 = (int32_t)__abs(val);
    return (uint8_t)(32 - __norm(v32) - 1);
}

static inline uint16_t getAmplitudeCode(int16_t val) 
{
    /*
     * If val < 0, (val >> 15) then -1 (sve jedinice: 0xFFFF).
     * If val > 0, (val >> 15) then 0  (0x0000).
     * result = val + (val >> 15).
     */
    int16_t mask = val >> 15; 
    return (uint16_t)(val + mask);
}

/**
 * \brief Encodes a MACROBLOCK (4 blocks of 8x8) using RLE.
 * * \param macro_zigzag_buffer  Pointer to 256 int16_t elements (4 blocks contiguous).
 * \param rle_out              Pointer to output buffer.
 * \param max_capacity         Max remaining space in output buffer.
 * \param last_dc_ptr          Pointer to global Last DC value (updated sequentially).
 * \return                     Total symbols written for all 4 blocks (or -1 on error).
 */
int32_t performRLEBlock4x8x8(const int16_t * __restrict macro_zigzag_buffer, 
                             RLESymbol * __restrict rle_out, 
                             int32_t max_capacity, 
                             int16_t *last_dc_ptr)
{
    int32_t total_symbols = 0;
    int blk, k;
    
    int16_t currentDC, prevDC, diff;
    uint8_t dcSize;
    uint16_t dcCode;
    
    /* Cache pointers & vars in registers */
    const int16_t *current_block_ptr;
    int lastNonZeroIndex;
    int zeroCount;
    int16_t val;
    uint8_t size;
    uint16_t code;
    
    #pragma MUST_ITERATE(4, 4, 4)
    for (blk = 0; blk < 4; blk++)
    {
        current_block_ptr = macro_zigzag_buffer + (blk * 64);
        
        /* -----------------------------------------------------------
         * 1. DC COEFF (Always Index 0)
         * -----------------------------------------------------------
         */
        currentDC = current_block_ptr[0];
        prevDC    = *last_dc_ptr;
        diff      = currentDC - prevDC;
        *last_dc_ptr = currentDC;

        /* Koristimo brze funkcije */
        dcSize = getBitLength(diff);
        dcCode = getAmplitudeCode(diff);

        /* Check capacity carefully - branching here is unavoidable but predictable */
        if (total_symbols >= max_capacity) return -1;
        
        /* Direct write */
        RLESymbol *sym = &rle_out[total_symbols++];
        sym->symbol   = dcSize;
        sym->code     = dcCode;
        sym->codeBits = dcSize;

        /* -----------------------------------------------------------
         * 2. FIND LAST NON-ZERO (Optimization)
         * -----------------------------------------------------------
         */
        lastNonZeroIndex = 0;
        
        /* Vector-friendly search: Scan backwards */
        for (k = 63; k > 0; k--) {
            if (current_block_ptr[k] != 0) {
                lastNonZeroIndex = k;
                break;
            }
        }

        /* -----------------------------------------------------------
         * 3. AC COEFFS (Index 1 .. lastNonZeroIndex)
         * -----------------------------------------------------------
         */
        zeroCount = 0;

        /* MUST_ITERATE hints: min=0 (ako je last=0), max=63 */
        #pragma MUST_ITERATE(0, 63)
        for (k = 1; k <= lastNonZeroIndex; k++) 
        {
            val = current_block_ptr[k];

            if (val == 0) {
                zeroCount++;
                continue; /* Preskoci ostatak, sto brze na sledecu iteraciju */
            }
            
            /* -- NON-ZERO FOUND -- */
            
            /* Handle ZRL (Runs > 15) 
             */
            while (zeroCount >= 16) {
                if (total_symbols >= max_capacity) return -1;
                
                RLESymbol *zrl = &rle_out[total_symbols++];
                zrl->symbol   = 0xF0;
                zrl->code     = 0;
                zrl->codeBits = 0;
                zeroCount -= 16;
            }

            /* Compute Size/Code using FAST intrinsics */
            size = getBitLength(val);
            code = getAmplitudeCode(val);
            
            /* Combine ZeroRun and Size */
            // symbolByte = (zeroCount << 4) | size; 
            
            if (total_symbols >= max_capacity) return -1;
            
            RLESymbol *ac = &rle_out[total_symbols++];
            ac->symbol   = (uint8_t)((zeroCount << 4) | size);
            ac->code     = code;
            ac->codeBits = size;
            
            zeroCount = 0;
        }

        /* -----------------------------------------------------------
         * 4. EOB (End of Block)
         * -----------------------------------------------------------
         */
        if (lastNonZeroIndex < 63) {
            if (total_symbols >= max_capacity) return -1;
            
            RLESymbol *eob = &rle_out[total_symbols++];
            eob->symbol   = 0x00;
            eob->code     = 0;
            eob->codeBits = 0;
        }
    }

    return total_symbols;
}
#endif
