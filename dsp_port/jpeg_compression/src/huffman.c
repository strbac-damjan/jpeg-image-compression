#ifdef __C7000__
#include "jpeg_compression.h"
#include <string.h>
#include <stdint.h>
#include <c7x.h>

// --- HUFFMAN TABLE DEFINITIONS ---

// Standard JPEG luminance DC table
// Defines how many codes exist for each bit length
static const uint8_t std_dc_luminance_nrcodes[17] =
{
    0, 0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0
};

// Values associated with DC luminance Huffman codes
static const uint8_t std_dc_luminance_values[12] =
{
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11
};

// Standard JPEG luminance AC table
// Number of codes per bit length
static const uint8_t std_ac_luminance_nrcodes[17] =
{
    0, 0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7d
};

// Values associated with AC luminance Huffman codes
static const uint8_t std_ac_luminance_values[162] =
{
    0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
    0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08, 0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0,
    0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
    0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
    0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
    0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
    0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5,
    0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
    0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
    0xf9, 0xfa
};

// Structure holding a single Huffman code and its length
typedef struct {
    uint16_t code;
    uint8_t len;
} HuffmanCode;

// Structure used to manage bit-level output
typedef struct {
    uint8_t * __restrict buffer;   // Output buffer pointer
    int32_t capacity;              // Total buffer capacity
    int32_t size;                  // Current number of bytes written
    uint64_t accumulator;          // Bit accumulator
    int32_t bitCount;              // Number of valid bits in accumulator
} BitWriter;

// Huffman lookup tables
static HuffmanCode dcTable[16];
static HuffmanCode acTable[256];
static int tablesInitialized = 0;

// Generates Huffman codes from JPEG specification tables
static void generateCodes(const uint8_t* nrcodes,
                          const uint8_t* values,
                          HuffmanCode* table)
{
    uint16_t code = 0;
    int tableIdx = 0;
    int length, i;

    // Iterate over all possible code lengths
    for (length = 1; length <= 16; length++)
    {
        int count = nrcodes[length];

        // Assign codes of the current length
        for (i = 0; i < count; i++)
        {
            uint8_t symbol = values[tableIdx++];
            table[symbol].code = code;
            table[symbol].len  = (uint8_t)length;
            code++;
        }

        // Prepare code prefix for the next length
        code <<= 1;
    }
}

// Initializes DC and AC Huffman tables once
static void initTables()
{
    if (tablesInitialized) return;

    memset(dcTable, 0, sizeof(dcTable));
    memset(acTable, 0, sizeof(acTable));

    generateCodes(std_dc_luminance_nrcodes,
                  std_dc_luminance_values,
                  dcTable);

    generateCodes(std_ac_luminance_nrcodes,
                  std_ac_luminance_values,
                  acTable);

    tablesInitialized = 1;
}

// Flushes 32 bits from the accumulator into the output buffer
static inline void flushBlock(BitWriter* bw)
{
    // Extract the upper 32 bits
    uint32_t chunk = (uint32_t)(bw->accumulator >> 32);

    // Shift accumulator and update bit count
    bw->accumulator <<= 32;
    bw->bitCount -= 32;

    // Pointer to the current write position
    uint8_t * __restrict p = &bw->buffer[bw->size];

    // Split the chunk into bytes
    uint8_t b3 = (chunk >> 24) & 0xFF;
    uint8_t b2 = (chunk >> 16) & 0xFF;
    uint8_t b1 = (chunk >> 8)  & 0xFF;
    uint8_t b0 =  chunk        & 0xFF;

    // Write bytes and insert stuffing after 0xFF
    *p++ = b3; if (b3 == 0xFF) *p++ = 0x00;
    *p++ = b2; if (b2 == 0xFF) *p++ = 0x00;
    *p++ = b1; if (b1 == 0xFF) *p++ = 0x00;
    *p++ = b0; if (b0 == 0xFF) *p++ = 0x00;

    // Update total output size
    bw->size = (int32_t)(p - bw->buffer);
}

// Writes bits into the accumulator and flushes when needed
static inline void putBits(BitWriter* bw, uint16_t data, uint8_t numBits)
{
    // Mask unused upper bits
    data &= (0xFFFF >> (16 - numBits));

    // Insert bits into the accumulator
    bw->accumulator |= ((uint64_t)data << (64 - bw->bitCount - numBits));
    bw->bitCount += numBits;

    // Flush when enough bits are accumulated
    if (bw->bitCount >= 32)
    {
        flushBlock(bw);
    }
}

// Flushes remaining bits at the end of encoding
static void flushBits(BitWriter* bw)
{
    // Write all full bytes
    while (bw->bitCount >= 8)
    {
        uint8_t byte = (uint8_t)(bw->accumulator >> 56);
        bw->accumulator <<= 8;
        bw->bitCount -= 8;
        bw->buffer[bw->size++] = byte;

        // Insert byte stuffing if needed
        if (byte == 0xFF)
        {
            bw->buffer[bw->size++] = 0x00;
        }
    }

    // Handle remaining partial byte
    if (bw->bitCount > 0)
    {
        uint8_t byte = (uint8_t)(bw->accumulator >> 56);
        byte |= (uint8_t)((1 << (8 - bw->bitCount)) - 1);
        bw->buffer[bw->size++] = byte;

        if (byte == 0xFF)
        {
            bw->buffer[bw->size++] = 0x00;
        }
    }
}

int32_t performHuffman(RLESymbol * __restrict rleData,
                       int32_t numSymbols,
                       uint8_t * __restrict outBuffer,
                       int32_t bufferCapacity)
{
    // Initialize Huffman tables if needed
    initTables();

    // Initialize bit writer
    BitWriter bw;
    bw.buffer = outBuffer;
    bw.capacity = bufferCapacity;
    bw.size = 0;
    bw.accumulator = 0;
    bw.bitCount = 0;

    int symbolIdx = 0;

    // Process all RLE symbols
    while (symbolIdx < numSymbols)
    {
        // Process DC symbol
        RLESymbol dcSym = rleData[symbolIdx++];
        HuffmanCode huff = dcTable[dcSym.symbol];

        // Write DC Huffman code and amplitude bits
        putBits(&bw, huff.code, huff.len);
        putBits(&bw, dcSym.code, dcSym.codeBits);

        // Process AC symbols for the block
        int coeffsEncoded = 1;

        while (coeffsEncoded < 64)
        {
            if (symbolIdx >= numSymbols) break;

            RLESymbol acSym = rleData[symbolIdx++];
            huff = acTable[acSym.symbol];

            // Write AC Huffman code
            putBits(&bw, huff.code, huff.len);

            // Write amplitude bits if present
            if (acSym.codeBits > 0)
            {
                putBits(&bw, acSym.code, acSym.codeBits);
            }

            // Handle special symbols
            if (acSym.symbol == 0x00)
            {
                break;
            }
            else if (acSym.symbol == 0xF0)
            {
                coeffsEncoded += 16;
            }
            else
            {
                int run = (acSym.symbol >> 4) & 0x0F;
                coeffsEncoded += run + 1;
            }
        }
    }

    // Flush remaining bits to output
    flushBits(&bw);

    // Return number of bytes written
    return bw.size;
}
#endif
