#include "jpeg_handler.h"
#include "jpeg_tables.h"


YImage* convertBMPToJPEGGrayscale(const BMPImage* image) {
    
    if (image == NULL || image->data == NULL) {
        return NULL;
    }

    YImage* yImg = (YImage*)malloc(sizeof(YImage));
    if (yImg == NULL) {
        return NULL; // Greška pri alokaciji
    }

    yImg->width = image->width;
    yImg->height = image->height;
    
    int totalPixels = image->width * image->height;
    yImg->data = (unsigned char*)malloc(totalPixels * sizeof(unsigned char));

    if (yImg->data == NULL) {
        free(yImg);
        return NULL;
    }

    for (int i = 0; i < totalPixels; i++) {
        int rgbIndex = i * 3;

        uint8_t r = image->data[rgbIndex];     
        uint8_t g = image->data[rgbIndex + 1];
        uint8_t b = image->data[rgbIndex + 2];

        // Original:
        // Y = 0.299*R + 0.587*G + 0.114*B
        // Optimized whole number approximation(multiplied by 256):
        // Y = (77*R + 150*G + 29*B) >> 8
        
        uint32_t yVal = (77 * r + 150 * g + 29 * b) >> 8;

        yImg->data[i] = (uint8_t) yVal;
    }

    return yImg;
}


// 1. Write APP0 (JFIF Header)
bool write_app0(FILE *file) {
    JPEG_Header_APP0 app0;
    memset(&app0, 0, sizeof(app0));
    
    app0.soi_marker  = SWAP16(0xFFD8); // SOI
    app0.app0_marker = SWAP16(0xFFE0); // APP0
    app0.length      = SWAP16(16);     
    strcpy(app0.identifier, "JFIF");   
    app0.version     = SWAP16(0x0101); 
    app0.units       = 1;              // DPI
    app0.x_density   = SWAP16(96);     
    app0.y_density   = SWAP16(96);     

    return fwrite(&app0, sizeof(app0), 1, file) == 1;
}

// Write DQT (Quantization Table)
bool write_dqt(FILE *file) {
    JPEG_DQT dqt;
    dqt.marker  = SWAP16(0xFFDB);
    dqt.length  = SWAP16(67); // 2 + 1 + 64
    dqt.qt_info = 0x00;       // ID 0, 8-bit
    memcpy(dqt.table, std_luminance_quant_tbl, 64);

    return fwrite(&dqt, sizeof(dqt), 1, file) == 1;
}

// Write SOF0 (Image Dimensions)
bool write_sof0(FILE *file, int width, int height) {
    JPEG_Header_SOF0 sof0;
    sof0.marker         = SWAP16(0xFFC0);
    sof0.length         = SWAP16(11); // 8 header + 3 bytes for 1 component
    sof0.precision      = 8;
    sof0.height         = SWAP16((uint16_t)height);
    sof0.width          = SWAP16((uint16_t)width);
    sof0.num_components = 1; 

    sof0.comp_id        = 1;    // Y Channel ID
    sof0.samp_factor    = 0x11; // 1x1 Subsampling
    sof0.quant_table_id = 0;    // Use DQT 0

    return fwrite(&sof0, sizeof(sof0), 1, file) == 1;
}

// Write DHT (DC Component)
bool write_dht_dc(FILE *file) {
    JPEG_DHT_DC dht;
    dht.marker  = SWAP16(0xFFC4);
    dht.length  = SWAP16(31); // 2 + 1 + 16 + 12
    dht.ht_info = 0x00;       // DC (0), ID (0) -> 0x00
    memcpy(dht.num_k, std_dc_luminance_nrcodes, 16);
    memcpy(dht.val, std_dc_luminance_values, 12);

    return fwrite(&dht, sizeof(dht), 1, file) == 1;
}

// Write DHT (AC Component)
bool write_dht_ac(FILE *file) {
    JPEG_DHT_AC dht;
    dht.marker  = SWAP16(0xFFC4);
    dht.length  = SWAP16(181); // 2 + 1 + 16 + 162
    dht.ht_info = 0x10;        // AC (1), ID (0) -> 0x10
    memcpy(dht.num_k, std_ac_luminance_nrcodes, 16);
    memcpy(dht.val, std_ac_luminance_values, 162);

    return fwrite(&dht, sizeof(dht), 1, file) == 1;
}

// Write SOS (Start of Scan)
bool write_sos(FILE *file) {
    JPEG_Header_SOS sos;
    sos.marker         = SWAP16(0xFFDA);
    sos.length         = SWAP16(8); // 6 header + 2 bytes for 1 component
    sos.num_components = 1;

    sos.comp_id        = 1;    // Match SOF0 ID
    sos.huff_table_id  = 0x00; // DC 0 / AC 0
    sos.start_spectral = 0;
    sos.end_spectral   = 63;
    sos.approx_high    = 0;

    return fwrite(&sos, sizeof(sos), 1, file) == 1;
}

// Write EOI (End of Image)
bool write_eoi(FILE *file) {
    unsigned short eoi = SWAP16(0xFFD9);
    return fwrite(&eoi, sizeof(eoi), 1, file) == 1;
}
