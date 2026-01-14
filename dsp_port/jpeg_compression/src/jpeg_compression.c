#ifdef __C7000__
#include <jpeg_compression.h>

// Remote service handler for JPEG compression
// Casts input parameters to DTO and triggers JPEG conversion
int32_t JpegCompression_RemoteServiceHandler(char *service_name,
                                             uint32_t cmd,
                                             void *prm,
                                             uint32_t prm_size,
                                             uint32_t flags)
{
    JPEG_COMPRESSION_DTO *dto = (JPEG_COMPRESSION_DTO *)prm;
    return convertToJpeg(dto);
}

// Initializes the JPEG compression remote service
// Registers the service handler with the system
int32_t JpegCompression_Init()
{
    int32_t status = -1;

    appLogPrintf("JPEG Compression: Init ... !!!");
    status = appRemoteServiceRegister(JPEG_COMPRESSION_REMOTE_SERVICE_NAME,
                                      JpegCompression_RemoteServiceHandler);

    if (status != 0)
    {
        appLogPrintf("JPEG Compression: ERROR: Unable to register remote service handler\n");
    }

    appLogPrintf("JPEG Compression: Init ... DONE!!!\n");
    return status;
}

int32_t convertToJpeg(JPEG_COMPRESSION_DTO *dto)
{
    
    uint64_t t_start, t_step;

    RLESymbol *rleBase;
    RLESymbol *rleCurrent;

    int32_t max_rle_capacity;
    int32_t total_rle_symbols;

    uint8_t *huffData;
    int32_t huff_capacity;
    int32_t bytesWritten;

    // Debug output pointers
    uint8_t  *debug_y_ptr;
    float    *debug_dct_ptr;
    int16_t  *debug_quant_ptr;
    int16_t  *debug_zigzag_ptr;

    // Local L1 buffers

    // Buffer for one macro block of Y samples
    // Represents a 32x8 block of int8 pixels
    __attribute__((aligned(64))) int8_t macro_y_buffer[MACRO_BLOCK_WIDTH * BLOCK_SIZE];

    // Buffer for DCT output and quantization input
    // Contains four 32x8 blocks stored as floats
    __attribute__((aligned(64))) float macro_dct_buffer[MACRO_BLOCK_WIDTH * BLOCK_SIZE];

    // Buffer for quantization output and ZigZag input
    // Contains four 32x8 blocks stored as int16
    __attribute__((aligned(64))) int16_t macro_quant_buffer[MACRO_BLOCK_WIDTH * BLOCK_SIZE];

    // Buffer for ZigZag output
    // Stores reordered coefficients for the macro block
    __attribute__((aligned(64))) int16_t macro_zigzag_buffer[MACRO_BLOCK_WIDTH * BLOCK_SIZE];

    // Profiling counters for different pipeline stages
    uint64_t sum_color   = 0;
    uint64_t sum_dct     = 0;
    uint64_t sum_quant   = 0;
    uint64_t sum_zigzag  = 0;
    uint64_t sum_rle     = 0;

    // DC predictor for differential coding
    int16_t global_last_dc = 0;

    // Loop variables
    int y, x, i, row;
    int32_t syms;

    t_start = __TSC;

    // Map shared memory pointers for RGB input components
    uint8_t *r_component = (uint8_t *)(uintptr_t)appMemShared2TargetPtr(dto->r_phy_ptr);
    uint8_t *g_component = (uint8_t *)(uintptr_t)appMemShared2TargetPtr(dto->g_phy_ptr);
    uint8_t *b_component = (uint8_t *)(uintptr_t)appMemShared2TargetPtr(dto->b_phy_ptr);

    // Initialize RLE output buffer and counters
    rleBase    = (RLESymbol *)(uintptr_t)appMemShared2TargetPtr(dto->rle_phy_ptr);
    rleCurrent = rleBase;
    max_rle_capacity   = dto->width * dto->height * 2;
    total_rle_symbols  = 0;

    // Initialize Huffman output buffer
    huffData      = (uint8_t *)(uintptr_t)appMemShared2TargetPtr(dto->huff_phy_ptr);
    huff_capacity = dto->width * dto->height;

    // Map debug output buffers
    debug_y_ptr      = (uint8_t *)(uintptr_t)appMemShared2TargetPtr(dto->y_phy_ptr);
    debug_dct_ptr    = (float *)(uintptr_t)appMemShared2TargetPtr(dto->dct_phy_ptr);
    debug_quant_ptr  = (int16_t *)(uintptr_t)appMemShared2TargetPtr(dto->quant_phy_ptr);
    debug_zigzag_ptr = (int16_t *)(uintptr_t)appMemShared2TargetPtr(dto->zigzag_phy_ptr);

    // Initialize ZigZag permutation masks once
    t_step = __TSC;
    init_ZigZag_Masks();
    sum_zigzag += (__TSC - t_step);

    // Iterate over the image in blocks of 8 rows and 32 width
    for (y = 0; y < dto->height; y += BLOCK_SIZE)
    {
        for (x = 0; x < dto->width; x += MACRO_BLOCK_WIDTH)
        {
            // Compute pointers to the current macro block in each color channel
            uint8_t *current_r = r_component + (y * dto->width + x);
            uint8_t *current_g = g_component + (y * dto->width + x);
            uint8_t *current_b = b_component + (y * dto->width + x);

            // Extract Y component for the current macro block
            t_step = __TSC;
            extractYComponentBlock4x8x8(current_r,
                                       current_g,
                                       current_b,
                                       x,
                                       y,
                                       dto->width,
                                       macro_y_buffer);
            sum_color += (__TSC - t_step);

            // Perform DCT on the macro block
            t_step = __TSC;
            computeDCTBlock4x8x8(macro_y_buffer,
                                 macro_dct_buffer,
                                 MACRO_BLOCK_WIDTH);
            sum_dct += (__TSC - t_step);

            // Quantize DCT coefficients
            t_step = __TSC;
            quantizeBlock4x8x8(macro_dct_buffer,
                               macro_quant_buffer);
            sum_quant += (__TSC - t_step);

            // Perform ZigZag reordering
            t_step = __TSC;
            performZigZagBlock4x8x8(macro_quant_buffer,
                                    macro_zigzag_buffer);
            sum_zigzag += (__TSC - t_step);

            // Save debug data only for the first block in the image
            if (y == 0 && x == 0)
            {
                // Save Y samples from the first 8x8 block
                for (row = 0; row < 8; row++)
                {
                    for (i = 0; i < 8; i++)
                    {
                        debug_y_ptr[row * 8 + i] =
                            (uint8_t)(macro_y_buffer[row * 32 + i] + 128);
                    }
                }

                // Save DCT coefficients of the first block
                for (i = 0; i < 64; i++)
                    debug_dct_ptr[i] = macro_dct_buffer[i];

                // Save quantized coefficients of the first block
                for (i = 0; i < 64; i++)
                    debug_quant_ptr[i] = macro_quant_buffer[i];

                // Save ZigZag-ordered coefficients of the first block
                for (i = 0; i < 64; i++)
                    debug_zigzag_ptr[i] = macro_zigzag_buffer[i];
            }

            // Perform run-length encoding on the macro block
            t_step = __TSC;
            syms = performRLEBlock4x8x8(macro_zigzag_buffer,
                                        rleCurrent,
                                        max_rle_capacity - total_rle_symbols,
                                        &global_last_dc);
            sum_rle += (__TSC - t_step);

            // Handle RLE error
            if (syms < 0)
                return -6;

            // Advance RLE output pointer and update symbol count
            rleCurrent += syms;
            total_rle_symbols += syms;
        }
    }

    // Store profiling results in DTO
    dto->rle_count             = total_rle_symbols;
    dto->cycles_color_conversion = sum_color;
    dto->cycles_dct              = sum_dct;
    dto->cycles_quantization     = sum_quant;
    dto->cycles_zigzag           = sum_zigzag;
    dto->cycles_rle              = sum_rle;

    // Perform Huffman encoding on the RLE symbols
    t_step = __TSC;
    bytesWritten = performHuffman(rleBase,
                                  total_rle_symbols,
                                  huffData,
                                  huff_capacity);
    dto->cycles_huffman = __TSC - t_step;

    // Handle Huffman error
    if (bytesWritten < 0)
        return -8;

    // Store Huffman output size and total cycle count
    dto->huff_size    = bytesWritten;
    dto->cycles_total = __TSC - t_start;

    return 0;
}
#endif
