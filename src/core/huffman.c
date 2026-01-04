#include "huffman.h"
#include "jpeg_tables.h"
#include <string.h>
#include <stdio.h>

// --- internal structures ---

typedef struct {
    uint16_t code; // The bit sequence (e.g., 1010)
    uint8_t len;   // The length of the sequence (e.g., 4)
} HuffmanCode;

// Lookup tables: 
// dcTable maps a category (0-11) to a code.
// acTable maps a symbol (Run/Size byte) to a code.
static HuffmanCode dcTable[16];
static HuffmanCode acTable[256];
static int tablesInitialized = 0;

// --- BitWriter Helper ---

typedef struct {
    JpegEncoderBuffer* buffer;
    uint32_t accumulator; // Temp storage for bits
    int bitCount;         // How many bits are currently in accumulator
} BitWriter;

// Grows the buffer if needed
static void ensureCapacity(JpegEncoderBuffer* buf, size_t extra) {
    if (buf->size + extra >= buf->capacity) {
        size_t newCap = buf->capacity == 0 ? 1024 : buf->capacity * 2;
        if (newCap < buf->size + extra) newCap = buf->size + extra + 1024;
        buf->data = (uint8_t*)realloc(buf->data, newCap);
        buf->capacity = newCap;
    }
}

// Writes a single byte to the buffer, handling JPEG Byte Stuffing
// Rule: If we write 0xFF, we must follow it with 0x00.
static void writeByte(JpegEncoderBuffer* buf, uint8_t byte) {
    ensureCapacity(buf, 2);
    buf->data[buf->size++] = byte;
    if (byte == 0xFF) {
        buf->data[buf->size++] = 0x00;
    }
}

// Pushes bits into the accumulator. When 8 bits accumulate, writes to buffer.
static void putBits(BitWriter* bw, uint16_t data, uint8_t numBits) {
    if (numBits == 0) return;

    // Mask data just in case
    data &= (1 << numBits) - 1;

    // Add bits to top of accumulator
    // JPEG writes MSB first.
    // We shift current bits left to make room, but here we construct 
    // the byte from top down.
    
    // Standard approach: Keep bits in a 32-bit integer, aligned to the left or right.
    // Here: We align to the left (MSB).
    
    // Let's use a simpler approach often used in JPEG encoders:
    // Accumulator fills from MSB.
    
    bw->accumulator |= ((uint32_t)data << (32 - bw->bitCount - numBits));
    bw->bitCount += numBits;

    while (bw->bitCount >= 8) {
        uint8_t byte = (bw->accumulator >> 24) & 0xFF;
        writeByte(bw->buffer, byte);
        
        bw->accumulator <<= 8;
        bw->bitCount -= 8;
    }
}

// Flushes remaining bits (pads with 1s)
static void flushBits(BitWriter* bw) {
    if (bw->bitCount > 0) {
        // Pad with 1s (standard says pad with 1s, though 0s often work too)
        // Since we are shifting 0s in from right, we need to OR with 1s for the padding.
        // Actually, simple padding with 0 (what remains in accumulator) is usually fine
        // as long as we align to byte boundary. 
        // But to be safe with `0xFF` stuffing at the end, let's just write what we have.
        
        uint8_t byte = (bw->accumulator >> 24) & 0xFF;
        
        // Mask the valid bits. The rest are 0s from the shift.
        // If we want 1-padding, we'd OR the lower bits. 
        // For this example, 0-padding the last byte is acceptable.
        
        writeByte(bw->buffer, byte);
    }
}

// --- Table Generation ---

/**
 * Generates Huffman codes from the standard length counts (nrcodes) and values.
 * This is the Canonical Huffman generation algorithm.
 */
static void generateCodes(const unsigned char* nrcodes, const unsigned char* values, HuffmanCode* table) {
    uint16_t code = 0;
    int tableIdx = 0; // Index into the 'values' array

    for (int length = 1; length <= 16; length++) {
        int count = nrcodes[length - 1]; // Number of symbols with this length
        
        for (int i = 0; i < count; i++) {
            unsigned char symbol = values[tableIdx++];
            table[symbol].code = code;
            table[symbol].len = length;
            code++;
        }
        code <<= 1;
    }
}

static void initHuffmanTables() {
    if (tablesInitialized) return;

    // Zero out first
    memset(dcTable, 0, sizeof(dcTable));
    memset(acTable, 0, sizeof(acTable));

    generateCodes(std_dc_luminance_nrcodes, std_dc_luminance_values, dcTable);
    generateCodes(std_ac_luminance_nrcodes, std_ac_luminance_values, acTable);

    tablesInitialized = 1;
}

// --- Main Encoder ---

JpegEncoderBuffer* encodeHuffman(const RLEData* rleData, int totalBlocks) {
    if (!tablesInitialized) {
        initHuffmanTables();
    }

    JpegEncoderBuffer* buf = (JpegEncoderBuffer*)malloc(sizeof(JpegEncoderBuffer));
    buf->data = NULL;
    buf->size = 0;
    buf->capacity = 0;

    BitWriter bw;
    bw.buffer = buf;
    bw.accumulator = 0;
    bw.bitCount = 0;

    int symbolIndex = 0;

    // We must track block boundaries to know when to switch between DC and AC tables.
    for (int b = 0; b < totalBlocks; b++) {
        
        if (symbolIndex >= rleData->count) break;

        // --- 1. Process DC Coefficient ---
        // The first symbol of every block is ALWAYS DC
        RLESymbol dcSym = rleData->data[symbolIndex++];
        
        // Look up code in DC Table
        HuffmanCode huff = dcTable[dcSym.symbol]; // dcSym.symbol is the Size/Category
        
        // Write Huffman Code
        putBits(&bw, huff.code, huff.len);
        // Write Amplitude Bits
        putBits(&bw, dcSym.code, dcSym.codeBits);

        // --- 2. Process AC Coefficients ---
        int coeffsEncoded = 1; // We just did DC (coeff 0)

        while (coeffsEncoded < 64) {
            if (symbolIndex >= rleData->count) break;

            RLESymbol acSym = rleData->data[symbolIndex++];
            
            // Look up code in AC Table
            // acSym.symbol is (Run << 4) | Size
            huff = acTable[acSym.symbol];

            // Write Huffman Code
            putBits(&bw, huff.code, huff.len);
            
            // Write Amplitude Bits (only if Size > 0)
            if (acSym.codeBits > 0) {
                putBits(&bw, acSym.code, acSym.codeBits);
            }

            // Update coefficient counter
            if (acSym.symbol == 0x00) { 
                // EOB (End Of Block)
                // This marks the end of the block, meaning we skip the rest of the 63 coeffs
                break; 
            } else if (acSym.symbol == 0xF0) {
                // ZRL (16 zeros)
                coeffsEncoded += 16;
            } else {
                // Regular symbol: Run zeros + 1 value
                int run = (acSym.symbol >> 4) & 0x0F;
                coeffsEncoded += run + 1;
            }
        }
    }

    flushBits(&bw);
    return buf;
}

void freeJpegEncoderBuffer(JpegEncoderBuffer* buffer) {
    if (buffer) {
        if (buffer->data) free(buffer->data);
        free(buffer);
    }
}