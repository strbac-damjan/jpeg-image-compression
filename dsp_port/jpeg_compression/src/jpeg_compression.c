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
    int8_t *debug_y_ptr;
    float *debug_dct_ptr;
    int16_t *debug_quant_ptr;
    int16_t *debug_zigzag_ptr;

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
    uint64_t sum_color = 0;
    uint64_t sum_dct = 0;
    uint64_t sum_quant = 0;
    uint64_t sum_zigzag = 0;
    uint64_t sum_rle = 0;

    // DC predictor for differential coding
    int16_t global_last_dc = 0;

    // Loop variables
    int i;
    int32_t syms;

    t_start = __TSC;

    // Map shared memory pointers for RGB input components
    uint8_t *r_component = (uint8_t *)(uintptr_t)appMemShared2TargetPtr(dto->r_phy_ptr);
    uint8_t *gb_component = (uint8_t *)(uintptr_t)appMemShared2TargetPtr(dto->gb_phy_ptr);

    appMemCacheInv(r_component, dto->width * dto->height);
    appMemCacheInv(gb_component, dto->width * dto->height);

    // Initialize RLE output buffer and counters
    rleBase = (RLESymbol *)(uintptr_t)appMemShared2TargetPtr(dto->rle_phy_ptr);
    rleCurrent = rleBase;
    max_rle_capacity = dto->width * dto->height * 2;
    total_rle_symbols = 0;

    // Initialize Huffman output buffer
    huffData = (uint8_t *)(uintptr_t)appMemShared2TargetPtr(dto->huff_phy_ptr);
    huff_capacity = dto->width * dto->height;

    // Map debug output buffers
    debug_y_ptr = (int8_t *)(uintptr_t)appMemShared2TargetPtr(dto->y_phy_ptr);
    debug_dct_ptr = (float *)(uintptr_t)appMemShared2TargetPtr(dto->dct_phy_ptr);
    debug_quant_ptr = (int16_t *)(uintptr_t)appMemShared2TargetPtr(dto->quant_phy_ptr);
    debug_zigzag_ptr = (int16_t *)(uintptr_t)appMemShared2TargetPtr(dto->zigzag_phy_ptr);

    setupStreamingEngine(r_component, gb_component, dto->height * dto->width);

    // Initialize ZigZag permutation masks once
    t_step = __TSC;
    init_ZigZag_Masks();
    sum_zigzag += (__TSC - t_step);

    uint32_t blocks_w = (dto->width + 7) / 8; // ceiling division
    uint32_t blocks_h = (dto->height + 7) / 8;
    uint32_t total_blocks = blocks_w * blocks_h;
    int block_index;
    // Iterate over the image in blocks of 8 rows and 32 width
    for (block_index = 0; block_index < total_blocks; block_index += 4)
    {
        // Extract Y component for the current macro block
        t_step = __TSC;
        extractYComponentBlock4x8x8(macro_y_buffer);
        sum_color += (__TSC - t_step);

        // Perform DCT on the macro block
        t_step = __TSC;
        computeDCTBlock4x8x8(macro_y_buffer, macro_dct_buffer);
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
        if (block_index == 0)
        {

            for (i = 0; i < 64; i++)
            {
                debug_y_ptr[i] = (macro_y_buffer[i]);
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

    // Store profiling results in DTO
    dto->rle_count = total_rle_symbols;
    dto->cycles_color_conversion = sum_color;
    dto->cycles_dct = sum_dct;
    dto->cycles_quantization = sum_quant;
    dto->cycles_zigzag = sum_zigzag;
    dto->cycles_rle = sum_rle;

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
    dto->huff_size = bytesWritten;
    dto->cycles_total = __TSC - t_start;

    closeStreamingEngine();

    return 0;
}
#endif
