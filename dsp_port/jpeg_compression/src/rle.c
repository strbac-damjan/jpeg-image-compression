#ifdef __C7000__
#include "jpeg_compression.h"
#include <c7x.h> 

/* -------------------------------------------------------------------------------------
 * HELPER FUNCTIONS (Inlined for speed)
 * -------------------------------------------------------------------------------------
 */

/**
 * \brief Calculates the number of bits needed to represent a value.
 */
static inline uint8_t getBitLength(int16_t val) 
{
    int16_t absVal;
    uint8_t bits = 0;

    if (val == 0) return 0;
    
    /* Efficient Absolute Value */
    absVal = (val < 0) ? -val : val;
    
    while (absVal > 0) {
        bits++;
        absVal >>= 1;
    }
    return bits;
}

/**
 * \brief Calculates the VLI (Variable Length Integer) code.
 */
static inline uint16_t getAmplitudeCode(int16_t val) 
{
    if (val > 0) {
        return (uint16_t)val;
    } else {
        /* JPEG Spec: Negative numbers are stored as (val - 1) */
        return (uint16_t)(val - 1);
    }
}

/**
 * \brief Encodes a SINGLE 8x8 Block using RLE.
 */
int32_t performRLEBlock(int16_t *block, RLESymbol *rle_out, int32_t max_capacity, int16_t *last_dc_ptr)
{
    /* 1. DECLARATIONS (Must be at the top for C89) */
    int32_t symbol_count = 0;
    int16_t currentDC, prevDC, diff;
    uint8_t dcSize;
    uint16_t dcCode;
    int lastNonZeroIndex = 0;
    int k; /* Loop variable declared here */
    int zeroCount = 0;
    
    /* Temp vars for AC loop */
    int16_t val;
    uint8_t size;
    uint16_t code;
    uint8_t symbolByte;

    /* -----------------------------------------------------------
     * 2. LOGIC START
     * -----------------------------------------------------------
     */

    /* --- Process DC Coefficient (Index 0) --- */
    currentDC = block[0];
    prevDC = *last_dc_ptr;
    
    /* Differential Encoding: Diff = Current - Previous */
    diff = currentDC - prevDC;
    
    /* Update state for the NEXT block */
    *last_dc_ptr = currentDC;

    /* Encode DC */
    dcSize = getBitLength(diff);
    dcCode = getAmplitudeCode(diff);

    /* Write DC Symbol */
    if (symbol_count >= max_capacity) return -1;
    rle_out[symbol_count].symbol   = dcSize; /* Run is always 0 for DC */
    rle_out[symbol_count].code     = dcCode;
    rle_out[symbol_count].codeBits = dcSize;
    symbol_count++;

    /* --- Find Last Non-Zero AC Coefficient (Index 1..63) --- */
    /* Optimization: Scanning backwards allows us to emit EOB early. */
    
    /* C89 Loop: k is already declared at top */
    for (k = 63; k > 0; k--) {
        if (block[k] != 0) {
            lastNonZeroIndex = k;
            break;
        }
    }

    /* --- Process AC Coefficients --- */
    /* Loop only up to the last non-zero element */
    for (k = 1; k <= lastNonZeroIndex; k++) 
    {
        val = block[k];

        if (val == 0) {
            zeroCount++;
        } else {
            /* Handle ZRL (Zero Run Length) for runs > 15 */
            while (zeroCount >= 16) {
                if (symbol_count >= max_capacity) return -1;
                
                /* Symbol 0xF0: Run=15 (0xF), Size=0 */
                rle_out[symbol_count].symbol   = 0xF0;
                rle_out[symbol_count].code     = 0;
                rle_out[symbol_count].codeBits = 0;
                symbol_count++;
                
                zeroCount -= 16;
            }

            /* Encode Non-Zero AC */
            size = getBitLength(val);
            code = getAmplitudeCode(val);
            
            /* Symbol Byte: High nibble = Run, Low nibble = Size */
            symbolByte = (uint8_t)((zeroCount << 4) | size);
            
            if (symbol_count >= max_capacity) return -1;
            rle_out[symbol_count].symbol   = symbolByte;
            rle_out[symbol_count].code     = code;
            rle_out[symbol_count].codeBits = size;
            symbol_count++;
            
            zeroCount = 0; /* Reset run counter */
        }
    }

    /* --- Emit EOB (End of Block) --- */
    /* If the last non-zero index is less than 63, the rest are zeros. */
    if (lastNonZeroIndex < 63) {
        if (symbol_count >= max_capacity) return -1;
        
        /* Symbol 0x00: Run=0, Size=0 */
        rle_out[symbol_count].symbol   = 0x00;
        rle_out[symbol_count].code     = 0;
        rle_out[symbol_count].codeBits = 0;
        symbol_count++;
    }

    return symbol_count;
}
#endif
