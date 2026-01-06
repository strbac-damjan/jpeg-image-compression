#ifndef JPEG_TABLES_H
#define JPEG_TABLES_H

#include <stdint.h> 

// Standard JPEG Luminance Quantization Table
extern const unsigned char std_luminance_quant_tbl[64];

// Standard DC Luminance Data
extern const unsigned char std_dc_luminance_nrcodes[16];
extern const unsigned char std_dc_luminance_values[12];

// Standard AC Luminance Data
extern const unsigned char std_ac_luminance_nrcodes[16];
extern const unsigned char std_ac_luminance_values[162];

#endif