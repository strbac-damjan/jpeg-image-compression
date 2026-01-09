#ifdef __C7000__
#include "jpeg_compression.h"
#include <math.h>

/* -------------------------------------------------------------------------------------
 * HELPER FUNCTIONS
 * -------------------------------------------------------------------------------------
 */

/**
 * Calculates the category (Size) for a given value.
 */
static inline uint8_t getBitLength(int16_t val) {
    if (val == 0) return 0;
    
    // Absolute value
    int16_t absVal = (val < 0) ? -val : val;
    
    // Simple log2 implementation
    uint8_t bits = 0;
    while (absVal > 0) {
        bits++;
        absVal >>= 1;
    }
    return bits;
}

/**
 * Calculates the amplitude code (VLI - Variable Length Integer).
 */
static inline uint16_t getAmplitudeCode(int16_t val) {
    if (val > 0) {
        return (uint16_t)val;
    } else {
        // For negative numbers: val - 1 (in 1's complement sense for JPEG)
        return (uint16_t)(val - 1);
    }
}

/**
 * \brief Performs Run-Length Encoding (RLE) on Zig-Zag scanned data.
 * * \param zigzag_data  Input buffer (int16_t) containing all blocks in ZigZag order.
 * \param width        Image width.
 * \param height       Image height.
 * \param rle_out      Output buffer for RLE symbols.
 * \param max_capacity Maximum number of symbols the output buffer can hold.
 * \return             Number of symbols written.
 */
int32_t performRLE(int16_t *zigzag_data, int32_t width, int32_t height, RLESymbol *rle_out, int32_t max_capacity)
{
    int32_t total_pixels = width * height;
    int32_t num_blocks = total_pixels / 64;
    
    int32_t symbol_count = 0;
    int16_t lastDC = 0;
    int i, k;

    // Loop through all 8x8 blocks
    for (i = 0; i < num_blocks; i++) {
        
        // Safety check
        if (symbol_count + 64 > max_capacity) {
            return -1; // Buffer overflow error
        }

        // Pointer to the start of the current block (64 coeffs)
        int16_t* block = &zigzag_data[i * 64];

        // --- Process DC Coefficient (Index 0) ---
        int16_t currentDC = block[0];
        int16_t diff = currentDC - lastDC;
        lastDC = currentDC;

        uint8_t dcSize = getBitLength(diff);
        uint16_t dcCode = getAmplitudeCode(diff);

        // Emit DC Symbol (Run is always 0)
        rle_out[symbol_count].symbol = dcSize; // Run=0, Size=dcSize
        rle_out[symbol_count].code = dcCode;
        rle_out[symbol_count].codeBits = dcSize;
        symbol_count++;

        // --- Process AC Coefficients (Indices 1..63) ---
        int zeroCount = 0;
        int lastNonZeroIndex = 0;

        // Find the index of the last non-zero coefficient to know where to put EOB
        for (k = 63; k > 0; k--) {
            if (block[k] != 0) {
                lastNonZeroIndex = k;
                break;
            }
        }

        // Iterate through AC coeffs
        for (k = 1; k <= lastNonZeroIndex; k++) {
            int16_t val = block[k];

            if (val == 0) {
                zeroCount++;
            } else {
                // Handle run lengths > 15 (ZRL - Zero Run Length)
                while (zeroCount >= 16) {
                    // Symbol 0xF0: Run=15 (F), Size=0
                    rle_out[symbol_count].symbol = 0xF0;
                    rle_out[symbol_count].code = 0;
                    rle_out[symbol_count].codeBits = 0;
                    symbol_count++;
                    
                    zeroCount -= 16;
                }

                // Emit current non-zero value
                uint8_t size = getBitLength(val);
                uint16_t code = getAmplitudeCode(val);
                
                // Construct Byte: (Run << 4) | Size
                uint8_t symbolByte = (uint8_t)((zeroCount << 4) | size);
                
                rle_out[symbol_count].symbol = symbolByte;
                rle_out[symbol_count].code = code;
                rle_out[symbol_count].codeBits = size;
                symbol_count++;
                
                zeroCount = 0; // Reset run
            }
        }

        // --- End of Block (EOB) ---
        // If we stopped before the 63rd index, the rest are zeros.
        if (lastNonZeroIndex < 63) {
            // Symbol 0x00: Run=0, Size=0
            rle_out[symbol_count].symbol = 0x00;
            rle_out[symbol_count].code = 0;
            rle_out[symbol_count].codeBits = 0;
            symbol_count++;
        }
    }

    return symbol_count;
}
#endif
