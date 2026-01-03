#ifndef JPEG_HANDLER_H
#define JPEG_HANDLER_H

// Helper macro to swap bytes for Big Endian (JPEG requirement)
#define SWAP16(x) ((uint16_t)((((x) & 0xFF) << 8) | (((x) >> 8) & 0xFF)))

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "bmp_handler.h"

// Internal image structure (does not necessarily need packing as it stays in memory)
typedef struct {
    int width;           // Image width
    int height;          // Image height
    uint8_t *data;       // Pointer to the pixel array (length = width * height)
                         // Values 0-255 (where 0=black, 255=white)
} YImage;

// Header indicating this is a standard JFIF JPEG
#pragma pack(push, 1) // Disable padding bytes
typedef struct {
    unsigned short soi_marker;  // Always 0xFFD8 (Start of Image)
    unsigned short app0_marker; // Always 0xFFE0 (APP0 marker)
    unsigned short length;      // Length of this segment (16 bytes)
    char identifier[5];         // "JFIF\0"
    unsigned short version;     // 0x0101
    unsigned char units;        // 0 = no units, 1 = dpi
    unsigned short x_density;   // e.g., 96 dpi
    unsigned short y_density;   // e.g., 96 dpi
    unsigned char thumb_width;  // 0 (we don't have a thumbnail)
    unsigned char thumb_height; // 0
} JPEG_Header_APP0;
#pragma pack(pop) // Restore normal alignment

// Header defining the image (Start of Frame - SOF0)
#pragma pack(push, 1) // Disable padding bytes
typedef struct {
    unsigned short marker;        // 0xFFC0 (Start of Frame Baseline)
    unsigned short length;        // Segment length (8 + 3 * number_of_components)
    unsigned char precision;      // 8 (bits per pixel)
    unsigned short height;        // Image height
    unsigned short width;         // Image width
    unsigned char num_components; // 1 if grayscale
    
    // Since we have only 1 component (Y), the structure ends here.
    // If it were color, we would have an array of 3 such sub-structures.
    unsigned char comp_id;        // 1 (ID for Y channel)
    unsigned char samp_factor;    // 0x11 (1x1 subsampling, i.e., no subsampling)
    unsigned char quant_table_id; // 0 (Use the first quantization table)
} JPEG_Header_SOF0;
#pragma pack(pop) // Restore normal alignment

// Start of Scan (SOS)
#pragma pack(push, 1) // Disable padding bytes
typedef struct {
    unsigned short marker;        // 0xFFDA
    unsigned short length;        // 12 (for 1 component)
    unsigned char num_components; // 1 (only Y)
    
    unsigned char comp_id;        // 1 (must match SOF0)
    unsigned char huff_table_id;  // 0x00 (DC table 0, AC table 0)
    
    unsigned char start_spectral; // 0
    unsigned char end_spectral;   // 63
    unsigned char approx_high;    // 0
    // AND IMMEDIATELY AFTER THIS THE BITSTREAM BEGINS (compressed data)
} JPEG_Header_SOS;
#pragma pack(pop) // Restore normal alignment

// Define Quantization Table (DQT)
#pragma pack(push, 1)
typedef struct {
    unsigned short marker;      // 0xFFDB
    unsigned short length;      // Always 67 for 1 table (2 bytes length + 1 info + 64 data)
    unsigned char qt_info;      // Bits 0-3: Table ID (0 for Y), Bits 4-7: Precision (0 = 8-bit)
    unsigned char table[64];    // The 64 quantization coefficients (in Zig-Zag order!)
} JPEG_DQT;
#pragma pack(pop)

// Huffman Table - DC Component (Standard Luminance)
#pragma pack(push, 1)
typedef struct {
    unsigned short marker;       // 0xFFC4 (DHT Marker)
    unsigned short length;       // 0x001F (31 bytes: 2 len + 1 info + 16 counts + 12 symbols)
    unsigned char ht_info;       // 0x00 (Bit 4=0 for DC, Bits 0-3=0 for ID 0)
    unsigned char num_k[16];     // Count of codes of length 1..16
    unsigned char val[12];       // The symbols sorted by frequency
} JPEG_DHT_DC;
#pragma pack(pop)

// Huffman Table - AC Component (Standard Luminance)
#pragma pack(push, 1)
typedef struct {
    unsigned short marker;       // 0xFFC4 (DHT Marker)
    unsigned short length;       // 0x00B5 (181 bytes: 2 len + 1 info + 16 counts + 162 symbols)
    unsigned char ht_info;       // 0x10 (Bit 4=1 for AC, Bits 0-3=0 for ID 0)
    unsigned char num_k[16];     // Count of codes of length 1..16
    unsigned char val[162];      // The symbols sorted by frequency
} JPEG_DHT_AC;
#pragma pack(pop)


bool write_app0(FILE *file);
bool write_dqt(FILE *file);
bool write_sof0(FILE *file, int width, int height);
bool write_dht_dc(FILE *file);
bool write_dht_ac(FILE *file);
bool write_sos(FILE *file);
bool write_eoi(FILE *file);
bool saveJPEGGrayscale(const char* filename, YImage img);

void freeYImage(YImage* img);

#endif
