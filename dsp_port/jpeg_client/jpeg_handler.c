#include "jpeg_handler.h"
#include <stdio.h>
#include <string.h>

const unsigned char std_luminance_quant_tbl[64] = {
    16, 11, 10, 16, 24, 40, 51, 61,
    12, 12, 14, 19, 26, 58, 60, 55,
    14, 13, 16, 24, 40, 57, 69, 56,
    14, 17, 22, 29, 51, 87, 80, 62,
    18, 22, 37, 56, 68, 109, 103, 77,
    24, 35, 55, 64, 81, 104, 113, 92,
    49, 64, 78, 87, 103, 121, 120, 101,
    72, 92, 95, 98, 112, 100, 103, 99
};

const unsigned char std_dc_luminance_nrcodes[16] = { 
    0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0 
};

const unsigned char std_dc_luminance_values[12]  = { 
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 
};

const unsigned char std_ac_luminance_nrcodes[16] = { 
    0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7D 
};

const unsigned char std_ac_luminance_values[162] = {
    0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12,
    0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
    0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xA1, 0x08,
    0x23, 0x42, 0xB1, 0xC1, 0x15, 0x52, 0xD1, 0xF0,
    0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0A, 0x16,
    0x17, 0x18, 0x19, 0x1A, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2A, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
    0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
    0x4A, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
    0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
    0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
    0x7A, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
    0x8A, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
    0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
    0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6,
    0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3, 0xC4, 0xC5,
    0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2, 0xD3, 0xD4,
    0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xE1, 0xE2,
    0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA,
    0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8,
    0xF9, 0xFA
};


// Standard Zigzag mapping table
const unsigned char zigzag_map[64] = {
     0,  1,  8, 16,  9,  2,  3, 10,
    17, 24, 32, 25, 18, 11,  4,  5,
    12, 19, 26, 33, 40, 48, 41, 34,
    27, 20, 13,  6,  7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36,
    29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46,
    53, 60, 61, 54, 47, 55, 62, 63
};

// Write APP0 (JFIF Header)
bool write_app0(FILE *file)
{
    JPEG_Header_APP0 app0;
    memset(&app0, 0, sizeof(app0));

    app0.soi_marker = SWAP16(0xFFD8);  // SOI
    app0.app0_marker = SWAP16(0xFFE0); // APP0
    app0.length = SWAP16(16);
    strcpy(app0.identifier, "JFIF");
    app0.version = SWAP16(0x0101);
    app0.units = 1; // DPI
    app0.x_density = SWAP16(96);
    app0.y_density = SWAP16(96);

    return fwrite(&app0, sizeof(app0), 1, file) == 1;
}


bool write_dqt(FILE *file)
{
    JPEG_DQT dqt;
    dqt.marker = SWAP16(0xFFDB);
    dqt.length = SWAP16(67);
    dqt.qt_info = 0x00;

    // Apply Zigzag reordering
    for (int i = 0; i < 64; i++) {
        dqt.table[i] = std_luminance_quant_tbl[zigzag_map[i]];
    }

    return fwrite(&dqt, sizeof(dqt), 1, file) == 1;
}

// Write SOF0 (Image Dimensions)
bool write_sof0(FILE *file, int width, int height)
{
    JPEG_Header_SOF0 sof0;
    sof0.marker = SWAP16(0xFFC0);
    sof0.length = SWAP16(11); // 8 header + 3 bytes for 1 component
    sof0.precision = 8;
    sof0.height = SWAP16((uint16_t)height);
    sof0.width = SWAP16((uint16_t)width);
    sof0.num_components = 1;

    sof0.comp_id = 1;        // Y Channel ID
    sof0.samp_factor = 0x11; // 1x1 Subsampling
    sof0.quant_table_id = 0; // Use DQT 0

    return fwrite(&sof0, sizeof(sof0), 1, file) == 1;
}

// Write DHT (DC Component)
bool write_dht_dc(FILE *file)
{
    JPEG_DHT_DC dht;
    dht.marker = SWAP16(0xFFC4);
    dht.length = SWAP16(31); // 2 + 1 + 16 + 12
    dht.ht_info = 0x00;      // DC (0), ID (0) -> 0x00
    memcpy(dht.num_k, std_dc_luminance_nrcodes, 16);
    memcpy(dht.val, std_dc_luminance_values, 12);

    return fwrite(&dht, sizeof(dht), 1, file) == 1;
}

// Write DHT (AC Component)
bool write_dht_ac(FILE *file)
{
    JPEG_DHT_AC dht;
    dht.marker = SWAP16(0xFFC4);
    dht.length = SWAP16(181); // 2 + 1 + 16 + 162
    dht.ht_info = 0x10;       // AC (1), ID (0) -> 0x10
    memcpy(dht.num_k, std_ac_luminance_nrcodes, 16);
    memcpy(dht.val, std_ac_luminance_values, 162);

    return fwrite(&dht, sizeof(dht), 1, file) == 1;
}

// Write SOS (Start of Scan)
bool write_sos(FILE *file)
{
    JPEG_Header_SOS sos;
    sos.marker = SWAP16(0xFFDA);
    sos.length = SWAP16(8); // 6 header + 2 bytes for 1 component
    sos.num_components = 1;

    sos.comp_id = 1;          // Match SOF0 ID
    sos.huff_table_id = 0x00; // DC 0 / AC 0
    sos.start_spectral = 0;
    sos.end_spectral = 63;
    sos.approx_high = 0;

    return fwrite(&sos, sizeof(sos), 1, file) == 1;
}

// Write EOI (End of Image)
bool write_eoi(FILE *file)
{
    unsigned short eoi = SWAP16(0xFFD9);
    return fwrite(&eoi, sizeof(eoi), 1, file) == 1;
}


bool saveJPEG(const char* filename, int width, int height, const uint8_t* huffmanStream, uint32_t streamSize) {
    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("Error opening output file");
        return false;
    }

    bool ok = true;

    // 1. Headers
    ok &= write_app0(file);
    ok &= write_dqt(file);
    ok &= write_sof0(file, width, height);
    
    // DHT Tables
    ok &= write_dht_dc(file);
    ok &= write_dht_ac(file);
    

    ok &= write_sos(file);

    if (!ok) {
        printf("JPEG: Error writing headers.\n");
        fclose(file);
        return false;
    }

    // 2. Bitstream (Calculated on DSP)
    size_t written = fwrite(huffmanStream, 1, streamSize, file);
    if (written != streamSize) {
        printf("JPEG: Error writing bitstream.\n");
        fclose(file);
        return false;
    }

    // 3. Footer
    write_eoi(file);

    fclose(file);
    printf("JPEG: File '%s' saved successfully (%d bytes)!\n", filename, (int)(ftell(file))); 
    return true;
}
