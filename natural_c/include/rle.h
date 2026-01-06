#ifndef RLE_H
#define RLE_H

#include <stdint.h>
#include "zigzag.h" 

// Structure representing a single encoding symbol for Huffman
typedef struct {
    uint8_t symbol;    // The constructed byte:
                       // For DC: Size (0..11)
                       // For AC: (Run << 4) | Size
    uint16_t code;     // The variable length bit string (amplitude)
    uint8_t codeBits;  // Number of bits in 'code' (Size)
} RLESymbol;

// Container for all symbols in the image
typedef struct {
    RLESymbol *data;   // Array of symbols
    size_t count;      // Current number of symbols
    size_t capacity;   // Allocated capacity
} RLEData;

RLEData* performRLE(const ZigZagData* zigZagData);
void freeRLEData(RLEData* rleData);

#endif