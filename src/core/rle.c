#include <stdlib.h>
#include <math.h>
#include "rle.h"

/**
 * Calculates the category (Size) for a given value.
 * e.g., -3 needs 2 bits, 5 needs 3 bits.
 */
static uint8_t getBitLength(int16_t val) {
    if (val == 0) return 0;
    
    // Absolute value
    int16_t absVal = (val < 0) ? -val : val;
    
    // Simple log2 implementation for integers
    uint8_t bits = 0;
    while (absVal > 0) {
        bits++;
        absVal >>= 1;
    }
    return bits;
}

static uint16_t getAmplitudeCode(int16_t val) {
    if (val > 0) {
        return (uint16_t)val;
    } else {
        // Example: val = -3 (binary ...11111101)
        // bitLength = 2
        // We want result 0 (binary 00) for -3
        // Formula: val + (2^bitLength) - 1
        // Simplified C approach: val - 1 (masks handled by bit length later)
        return (uint16_t)(val - 1);
    }
}

/**
 * Adds a symbol to the dynamic array.
 */
static void addSymbol(RLEData* rle, uint8_t symbol, uint16_t code, uint8_t bits) {
    if (rle->count >= rle->capacity) {
        rle->capacity = (rle->capacity == 0) ? 1024 : rle->capacity * 2;
        rle->data = (RLESymbol*)realloc(rle->data, rle->capacity * sizeof(RLESymbol));
    }
    rle->data[rle->count].symbol = symbol;
    rle->data[rle->count].code = code;
    rle->data[rle->count].codeBits = bits;
    rle->count++;
}

RLEData* performRLE(const ZigZagData* zzData) {
    if (zzData == NULL || zzData->data == NULL) return NULL;

    RLEData* rle = (RLEData*)malloc(sizeof(RLEData));
    rle->count = 0;
    rle->capacity = 4096; // Initial guess
    rle->data = (RLESymbol*)malloc(rle->capacity * sizeof(RLESymbol));

    int16_t lastDC = 0;

    // Iterate through all blocks
    for (int i = 0; i < zzData->totalBlocks; i++) {
        
        // Pointer to the start of the current block (64 coeffs)
        int16_t* block = &zzData->data[i * 64];

        // Process DC Coefficient (Index 0)
        int16_t currentDC = block[0];
        int16_t diff = currentDC - lastDC;
        lastDC = currentDC;

        uint8_t dcSize = getBitLength(diff);
        uint16_t dcCode = getAmplitudeCode(diff);

        // Store DC Symbol (Run is always 0 for DC, so symbol == dcSize)
        addSymbol(rle, dcSize, dcCode, dcSize);

        // Process AC Coefficients (Indices 1..63) 
        int zeroCount = 0;
        
        // Find the index of the last non-zero coefficient
        // This helps us know when to place EOB
        int lastNonZeroIndex = 0;
        for (int k = 63; k > 0; k--) {
            if (block[k] != 0) {
                lastNonZeroIndex = k;
                break;
            }
        }

        // Loop only up to the last non-zero value
        for (int k = 1; k <= lastNonZeroIndex; k++) {
            int16_t val = block[k];

            if (val == 0) {
                zeroCount++;
            } else {
                // If we have 16 or more zeros, emit ZRL(s)
                while (zeroCount >= 16) {
                    // Symbol 0xF0: Run=15, Size=0
                    addSymbol(rle, 0xF0, 0, 0); 
                    zeroCount -= 16;
                }

                // Emit current non-zero value
                uint8_t size = getBitLength(val);
                uint16_t code = getAmplitudeCode(val);
                
                // Construct Byte: (Run << 4) | Size
                uint8_t symbolByte = (zeroCount << 4) | size;
                
                addSymbol(rle, symbolByte, code, size);
                
                zeroCount = 0; // Reset run
            }
        }

        // End of Block (EOB) 
        // If the last non-zero index is less than 63, it means the rest are zeros.
        // We must emit EOB (0x00).
        if (lastNonZeroIndex < 63) {
            addSymbol(rle, 0x00, 0, 0);
        }
    }

    return rle;
}

void freeRLEData(RLEData* rleData) {
    if (rleData) {
        if (rleData->data) free(rleData->data);
        free(rleData);
    }
}
