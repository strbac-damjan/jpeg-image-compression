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
// #include <utils/app_init/include/app_init.h>
#include <utils/console_io/include/app_log.h>
#include <utils/mem/include/app_mem.h>
#include <c7x.h>

#define MACRO_BLOCK_WIDTH 32
#define BLOCK_SIZE 8
#define JPEG_COMPRESSION_REMOTE_SERVICE_NAME "com.etfbl.sdos.jpeg_compression"

// -------------------------------------------------------------------------------------
// ---------------------------STRUCTURE DEFINITIONS-------------------------------------
// -------------------------------------------------------------------------------------

typedef struct
{
    uint8_t symbol;   // (Run << 4) | Size
    uint8_t codeBits; // Number of significant bits
    uint16_t code;    // The amplitude value (variable length bits)
} RLESymbol;

typedef struct JPEG_COMPRESSION_DTO
{
    int32_t width;
    int32_t height;

    // Input RGB buffers (physical addresses)
    uint64_t r_phy_ptr;
    uint64_t gb_phy_ptr;

    // Intermediate buffers for debugging and profiling
    uint64_t y_phy_ptr;
    uint64_t dct_phy_ptr;
    uint64_t quant_phy_ptr;
    uint64_t zigzag_phy_ptr;

    // RLE output buffer
    uint64_t rle_phy_ptr;
    uint32_t rle_count;

    // Final Huffman bitstream output
    uint64_t huff_phy_ptr;
    uint32_t huff_size;

    // Profiling cycle counters
    uint64_t cycles_color_conversion;
    uint64_t cycles_dct;
    uint64_t cycles_quantization;
    uint64_t cycles_zigzag;
    uint64_t cycles_rle;
    uint64_t cycles_huffman;
    uint64_t cycles_total;

} JPEG_COMPRESSION_DTO;

// -------------------------------------------------------------------------------------
// --------------------------TI SERVICE FUNCTIONS---------------------------------------
// -------------------------------------------------------------------------------------

// Remote service handler for JPEG compression
// Casts input parameters to DTO and triggers JPEG conversion
int32_t JpegCompression_RemoteServiceHandler(char *service_name, uint32_t cmd,
                                             void *prm, uint32_t prm_size, uint32_t flags);

// Initializes the JPEG compression remote service
// Registers the service handler with the system
int32_t JpegCompression_Init();

// -------------------------------------------------------------------------------------
// ----------------------- JPEG COMPRESSION FUNCTIONS-----------------------------------
// -------------------------------------------------------------------------------------

#ifdef __cplusplus
extern "C"
{
#endif
    void setupStreamingEngine(uint8_t* r_vec, uint8_t* gb_vec, uint64_t image_length);
    void getNextHalfBlock(short32* r_output, short32* g_output, short32* b_output);
    void closeStreamingEngine();
    void fetch_next_block(int8_t* y_output);
    void fetch_setup(uint8_t* r_vec, uint8_t* gb_vec, uint64_t image_length);
    void fetch_close();
#ifdef __cplusplus
}
#endif

/**
 * \brief Main entry point for the DSP processing task.
 * Receives the Data Transfer Object (DTO) containing physical addresses from the A72 core,
 * maps them to the DSP's local virtual address space, and triggers the conversion.
 */
int32_t convertToJpeg(JPEG_COMPRESSION_DTO *dto);

/**
 * \brief Extracts the Y (luminance) component from RGB input for a 4x8x8 block of pixels
 */
void extractYComponentBlock4x8x8(int8_t *__restrict outputBuffer);

void init_ZigZag_Masks(void);

/**
 * \brief Computes DCT for a 32x8 Macro Block.
 * Handles int8 -> float conversion here.
 */
void computeDCTBlock4x8x8(const int8_t * __restrict src_data, float * __restrict dct_out);

/**
 * \brief Performs quantization for four 8x8 blocks at once
 * Input contains 256 floats representing four DCT blocks stored linearly
 * Output contains 256 int16 values after quantization
 */
void quantizeBlock4x8x8(float *__restrict dct_macro_block, int16_t *__restrict quant_macro_block);

/**
 * \brief Performs ZigZag reordering on four 8x8 blocks using vector permute
 * Input is in linear raster order and output is in ZigZag order
 */
void performZigZagBlock4x8x8(const int16_t *__restrict src_macro, int16_t *__restrict dst_macro);

/**
 * \brief Performs run-length encoding on four ZigZag-ordered 8x8 blocks
 * Produces JPEG-compliant RLE symbols
 */
int32_t performRLEBlock4x8x8(const int16_t *__restrict macro_zigzag_buffer,
                             RLESymbol *__restrict rle_out,
                             int32_t max_capacity,
                             int16_t *last_dc_ptr);

/**
 * \brief Performs Huffman encoding of RLE symbols into a byte stream
 */
int32_t performHuffman(RLESymbol *__restrict rleData, int32_t numSymbols, uint8_t *__restrict outBuffer, int32_t bufferCapacity);

// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------
#endif
#endif
