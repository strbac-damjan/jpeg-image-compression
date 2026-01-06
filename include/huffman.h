#ifndef JPEG_HUFFMAN_H
#define JPEG_HUFFMAN_H

#include <stdint.h>
#include <stdlib.h>
#include "rle.h"

// Output structure containing the final compressed bytes
typedef struct {
    uint8_t *data;
    size_t size;
    size_t capacity;
} JpegEncoderBuffer;

// --- internal structures ---

typedef struct {
    uint16_t code; // The bit sequence (e.g., 1010)
    uint8_t len;   // The length of the sequence (e.g., 4)
} HuffmanCode;

// --- BitWriter Helper ---

typedef struct {
    JpegEncoderBuffer* buffer;
    uint32_t accumulator; // Temp storage for bits
    int bitCount;         // How many bits are currently in accumulator
} BitWriter;

/**
 * Encodes the RLE data into a JPEG Huffman bitstream.
 * * @param rleData Input RLE symbols.
 * @param totalBlocks Total number of 8x8 blocks (needed for state tracking).
 * @return Pointer to JpegEncoderBuffer containing the bytestream.
 */
JpegEncoderBuffer* encodeHuffman(const RLEData* rleData, int totalBlocks);

void freeJpegEncoderBuffer(JpegEncoderBuffer* buffer);

#endif