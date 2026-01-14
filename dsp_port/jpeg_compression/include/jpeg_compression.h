#ifdef __C7000__
#ifndef JPEG_COMPRESSION_H
#define JPEG_COMPRESSION_H

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <TI/tivx.h>
#include <utils/ipc/include/app_ipc.h>
#include <utils/remote_service/include/app_remote_service.h>
//#include <utils/app_init/include/app_init.h>
#include <utils/console_io/include/app_log.h>
#include <utils/mem/include/app_mem.h>
#include <c7x.h>

#define MACRO_BLOCK_WIDTH 32
#define BLOCK_SIZE 8
#define JPEG_COMPRESSION_REMOTE_SERVICE_NAME "com.etfbl.sdos.jpeg_compression"

// -------------------------------------------------------------------------------------
// ---------------------------STRUCTURE DEFINITIONS-------------------------------------
// -------------------------------------------------------------------------------------
typedef struct BMPImage {
    int32_t width;
    int32_t height;
    uint8_t* r;  // Red channel
    uint8_t* g;  // Green channel
    uint8_t* b;  // Blue channel
} BMPImage;

typedef struct {
    int width;           // Image width
    int height;          // Image height
    uint8_t *data;       // Pointer to the pixel array (length = width * height)
                         // Values 0-255 (where 0=black, 255=white)
} YImage;

typedef struct {
    int width;
    int height;
    float *coefficients; // Width * Height
} DCTImage;

typedef struct {
    int32_t width;
    int32_t height;
    int16_t *data; // 16-bit signed integers for quantized values
} QuantizedImage;

typedef struct {
    uint8_t symbol;    // (Run << 4) | Size
    uint8_t codeBits;  // Number of significant bits
    uint16_t code;     // The amplitude value (variable length bits)
} RLESymbol;


typedef struct JPEG_COMPRESSION_DTO
{
    int32_t width;
    int32_t height;
    
    // Inputs
    uint64_t r_phy_ptr;
    uint64_t g_phy_ptr;
    uint64_t b_phy_ptr;

    // Intermediate buffers
    uint64_t y_phy_ptr;
    uint64_t dct_phy_ptr;
    uint64_t quant_phy_ptr;
    uint64_t zigzag_phy_ptr;

    // Outputs
    uint64_t rle_phy_ptr;
    uint32_t rle_count;
    uint64_t huff_phy_ptr; 
    uint32_t huff_size;

    // --- PROFILING DATA (Cycles) ---
    uint64_t cycles_color_conversion;
    uint64_t cycles_dct;
    uint64_t cycles_quantization;
    uint64_t cycles_zigzag;
    uint64_t cycles_rle;
    uint64_t cycles_huffman;
    uint64_t cycles_total; // Ukupno vrijeme obrade na DSP-u

} JPEG_COMPRESSION_DTO;
// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------

// -------------------------------------------------------------------------------------
// --------------------------TI SERVICE FUNCTIONS---------------------------------------
// -------------------------------------------------------------------------------------

// Remote service handler
int32_t JpegCompression_RemoteServiceHandler(char *service_name, uint32_t cmd,
void *prm, uint32_t prm_size, uint32_t flags);

// Service initialization function
int32_t JpegCompression_Init();
// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------

// -------------------------------------------------------------------------------------
// ----------------------- JPEG COMPRESSION FUNCTIONS-----------------------------------
// -------------------------------------------------------------------------------------

/**
 * \brief Extracts the Luminance (Y) component from RGB data using SIMD vectors.
 * * This function utilizes the C7x vector engine to process 32 pixels simultaneously per iteration.
 * The formula used is standard JPEG conversion: Y = (0.299*R + 0.587*G + 0.114*B).
 * Fixed-point approximation: Y = (77*R + 150*G + 29*B) >> 8.
 *
 * \param img    Pointer to the source BMP image structure (host pointers converted to DSP pointers).
 * \param y_out  Pointer to the destination Y-component structure.
 */
void extractYComponent(BMPImage *img, YImage *y_out);


void computeDCT(YImage *y_img, DCTImage *dct_out);

void quantizeImage(DCTImage *dct_img, QuantizedImage *q_img);

void performZigZag(QuantizedImage *q_img, int16_t *zigzag_out);

int32_t performRLE(int16_t *zigzag_data, int32_t width, int32_t height, RLESymbol *rle_out, int32_t max_capacity);

int32_t performHuffman(RLESymbol *rleData, int32_t numSymbols, uint8_t *outBuffer, int32_t bufferCapacity);

/**
 * \brief Main entry point for the DSP processing task.
 * * Receives the Data Transfer Object (DTO) containing physical addresses from the A72 core,
 * maps them to the DSP's local virtual address space, and triggers the conversion.
 */
int32_t convertToJpeg(JPEG_COMPRESSION_DTO* dto);



void extractYComponentBlock4x8x8(const BMPImage * __restrict img, int32_t startX, int32_t startY, int8_t * __restrict outputBuffer);

// void computeDCTBlock(int8_t * __restrict src_data, float * __restrict dct_out, int32_t stride);

// void quantizeBlock(float * __restrict dct_block, int16_t * __restrict quant_block);

void init_ZigZag_Masks(void);

void computeDCTBlock4x8x8(const int8_t * __restrict src_data, float * __restrict dct_out, int32_t stride);

void quantizeBlock4x8x8(float * __restrict dct_macro_block, int16_t * __restrict quant_macro_block);

void performZigZagBlock4x8x8(const int16_t * __restrict src_macro, int16_t * __restrict dst_macro);

int32_t performRLEBlock4x8x8(const int16_t * __restrict macro_zigzag_buffer, 
                             RLESymbol * __restrict rle_out, 
                             int32_t max_capacity, 
                             int16_t *last_dc_ptr);

// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------
#endif

#endif
