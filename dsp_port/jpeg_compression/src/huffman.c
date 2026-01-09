#ifdef __C7000__
#include "jpeg_compression.h"
#include <string.h>

#include <stdint.h>

/* Standard JPEG Luminance DC Huffman Tables */
static const uint8_t std_dc_luminance_nrcodes[17] = {
    0, 0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0
};
static const uint8_t std_dc_luminance_values[12] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11
};

/* Standard JPEG Luminance AC Huffman Tables */
static const uint8_t std_ac_luminance_nrcodes[17] = {
    0, 0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7d 
};
static const uint8_t std_ac_luminance_values[162] = {
    0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12,
    0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
    0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08,
    0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0,
    0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16,
    0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
    0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
    0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
    0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
    0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
    0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
    0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
    0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
    0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
    0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5,
    0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4,
    0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
    0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea,
    0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
    0xf9, 0xfa
};

/* --- Structures --- */
typedef struct {
    uint16_t code;
    uint8_t len;
} HuffmanCode;

/* --- BitWriter State --- */
typedef struct {
    uint8_t *buffer;
    int32_t capacity;
    int32_t size;
    uint32_t accumulator;
    int32_t bitCount;
} BitWriter;

/* --- Tables --- */
static HuffmanCode dcTable[16]; // Size maps to 0..15
static HuffmanCode acTable[256]; // Symbol maps to 0..255
static int tablesInitialized = 0;

/* --- Helper Functions --- */

// Canonical Huffman Code Generation
static void generateCodes(const uint8_t* nrcodes, const uint8_t* values, HuffmanCode* table) {
    uint16_t code = 0;
    int tableIdx = 0;
    int length, i;
    
    for (length = 1; length <= 16; length++) {
        int count = nrcodes[length]; // std tables are 1-based index in arrays often, but here logic: nrcodes[0] is count of len 1?
        // Wait, standard definition: nrcodes[0] = count of length 1, nrcodes[15] = count of length 16.
        // My header (jpeg_tables.h) has 17 elements, index 0 is usually ignored or length 0.
        // Let's adjust: In standard JPEG headers, index 0 is length 1. 
        // In the array provided above: {0, 0, 1...} usually implies index L is count of length L.
        // Let's assume input arrays are Size 17, where index 1..16 are valid lengths.
        
        count = nrcodes[length]; 
        
        for (i = 0; i < count; i++) {
            uint8_t symbol = values[tableIdx++];
            table[symbol].code = code;
            table[symbol].len = (uint8_t)length;
            code++;
        }
        code <<= 1;
    }
}

static void initTables() {
    if (tablesInitialized) return;
    
    memset(dcTable, 0, sizeof(dcTable));
    memset(acTable, 0, sizeof(acTable));
    
    // Arrays in jpeg_tables.h are defined as [17] where index is length.
    generateCodes(std_dc_luminance_nrcodes, std_dc_luminance_values, dcTable);
    generateCodes(std_ac_luminance_nrcodes, std_ac_luminance_values, acTable);
    
    tablesInitialized = 1;
}

static void writeByte(BitWriter* bw, uint8_t byte) {
    if (bw->size < bw->capacity) {
        bw->buffer[bw->size++] = byte;
        // Byte stuffing: if byte is 0xFF, write 0x00
        if (byte == 0xFF) {
            if (bw->size < bw->capacity) {
                bw->buffer[bw->size++] = 0x00;
            }
        }
    }
}

static void putBits(BitWriter* bw, uint16_t data, uint8_t numBits) {
    if (numBits == 0) return;
    
    // Mask to ensure only valid bits
    data &= (1 << numBits) - 1;
    
    // Accumulator logic (Align MSB)
    bw->accumulator |= ((uint32_t)data << (32 - bw->bitCount - numBits));
    bw->bitCount += numBits;
    
    while (bw->bitCount >= 8) {
        uint8_t byte = (bw->accumulator >> 24) & 0xFF;
        writeByte(bw, byte);
        bw->accumulator <<= 8;
        bw->bitCount -= 8;
    }
}

static void flushBits(BitWriter* bw) {
    if (bw->bitCount > 0) {
        uint8_t byte = (bw->accumulator >> 24) & 0xFF;
        // Pad the remaining bits with 1s is standard, but 0s works if last byte.
        // The mask (1s padding) logic: byte |= (1 << (8 - bitCount)) - 1; 
        // The user code used 0-padding implicit in shift. Sticking to user logic.
        writeByte(bw, byte);
    }
}

/* --- Main DSP Function --- */
int32_t performHuffman(RLESymbol *rleData, int32_t numSymbols, uint8_t *outBuffer, int32_t bufferCapacity) {
    
    initTables();
    
    BitWriter bw;
    bw.buffer = outBuffer;
    bw.capacity = bufferCapacity;
    bw.size = 0;
    bw.accumulator = 0;
    bw.bitCount = 0;
    
    int symbolIdx = 0;
    
    // We loop until we exhaust all symbols
    while (symbolIdx < numSymbols) {
        
        // 1. DC Coefficient (First symbol of block)
        RLESymbol dcSym = rleData[symbolIdx++];
        
        HuffmanCode huff = dcTable[dcSym.symbol]; // symbol is Size
        putBits(&bw, huff.code, huff.len);
        putBits(&bw, dcSym.code, dcSym.codeBits);
        
        // 2. AC Coefficients
        int coeffsEncoded = 1; // DC is coeff 0
        
        while (coeffsEncoded < 64) {
            if (symbolIdx >= numSymbols) break;
            
            // Peek at next symbol to see if it belongs to this block? 
            // Actually, RLE guarantees sequence.
            RLESymbol acSym = rleData[symbolIdx++];
            
            huff = acTable[acSym.symbol]; // symbol is (Run<<4)|Size
            putBits(&bw, huff.code, huff.len);
            
            if (acSym.codeBits > 0) {
                putBits(&bw, acSym.code, acSym.codeBits);
            }
            
            // Check for EOB or ZRL
            if (acSym.symbol == 0x00) {
                // EOB
                break; // End of this block
            } else if (acSym.symbol == 0xF0) {
                // ZRL
                coeffsEncoded += 16;
            } else {
                int run = (acSym.symbol >> 4) & 0x0F;
                coeffsEncoded += run + 1;
            }
        }
    }
    
    flushBits(&bw);
    
    return bw.size;
}
#endif
