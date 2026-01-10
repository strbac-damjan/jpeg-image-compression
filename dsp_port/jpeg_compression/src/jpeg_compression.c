#ifdef __C7000__
#include <jpeg_compression.h>

int32_t JpegCompression_RemoteServiceHandler(char *service_name, uint32_t cmd, void *prm, uint32_t prm_size, uint32_t flags)
{
    JPEG_COMPRESSION_DTO *dto = (JPEG_COMPRESSION_DTO *)prm;

    return convertToJpeg(dto);
}

int32_t JpegCompression_Init()
{
    int32_t status = -1;
    appLogPrintf("JPEG Compression: Init ... !!!");
    status = appRemoteServiceRegister(JPEG_COMPRESSION_REMOTE_SERVICE_NAME, JpegCompression_RemoteServiceHandler);
    if(status != 0)
    {
        appLogPrintf("JPEG Compression: ERROR: Unable to register remote service handler\n");
    }
    appLogPrintf("JPEG Compression: Init ... DONE!!!\n");
    return status;
}
int32_t convertToJpeg(JPEG_COMPRESSION_DTO* dto) 
{
    /* ---------------------------------------------------------------------
     * 1. DECLARATIONS (C89 requires all vars at the top)
     * ---------------------------------------------------------------------
     */
    uint64_t t_start, t_step;
    
    BMPImage inputImg;
    
    RLESymbol *rleBase;
    RLESymbol *rleCurrent;
    int32_t max_rle_capacity;
    int32_t total_rle_symbols;

    uint8_t *huffData;
    int32_t huff_capacity;
    int32_t bytesWritten;

    /* Local L1/Stack Buffers (HOT MEMORY) */
    /* align(64) is critical for vector instructions on C7x */
    __attribute__((aligned(64))) int8_t macro_y_buffer[MACRO_BLOCK_WIDTH * BLOCK_SIZE]; 
    __attribute__((aligned(64))) float   dct_block[64];    
    __attribute__((aligned(64))) int16_t quant_block[64];
    __attribute__((aligned(64))) int16_t zigzag_block[64];

    /* Profiling Accumulators */
    uint64_t sum_color = 0;
    uint64_t sum_dct = 0;
    uint64_t sum_quant = 0;
    uint64_t sum_zigzag = 0;
    uint64_t sum_rle = 0;
    
    int16_t global_last_dc = 0;
    
    /* Loop counters */
    int y, x, k;
    int32_t syms;
    int8_t *block_ptr;

    /* ---------------------------------------------------------------------
     * 2. INITIALIZATION & ADDRESS MAPPING
     * ---------------------------------------------------------------------
     */
    t_start = __TSC;

    inputImg.width  = dto->width;
    inputImg.height = dto->height;
    /* Mapiranje postojecih polja iz DTO: r_phy_ptr, g_phy_ptr, b_phy_ptr */
    inputImg.r = (uint8_t *)(uintptr_t)appMemShared2TargetPtr(dto->r_phy_ptr);
    inputImg.g = (uint8_t *)(uintptr_t)appMemShared2TargetPtr(dto->g_phy_ptr);
    inputImg.b = (uint8_t *)(uintptr_t)appMemShared2TargetPtr(dto->b_phy_ptr);

    /* RLE Output Pointer (mapped from rle_phy_ptr) */
    rleBase = (RLESymbol *)(uintptr_t)appMemShared2TargetPtr(dto->rle_phy_ptr);
    rleCurrent = rleBase;
    max_rle_capacity = dto->width * dto->height * 2; 
    total_rle_symbols = 0;

    /* Huffman Output Pointer (mapped from huff_phy_ptr) */
    huffData = (uint8_t *)(uintptr_t)appMemShared2TargetPtr(dto->huff_phy_ptr);
    /* Kapacitet racunamo rucno jer ga nema u DTO */
    huff_capacity = dto->width * dto->height;

    /* ---------------------------------------------------------------------
     * 3. THE PIPELINE LOOP
     * ---------------------------------------------------------------------
     */
    for (y = 0; y < dto->height; y += BLOCK_SIZE) 
    {
        for (x = 0; x < dto->width; x += MACRO_BLOCK_WIDTH) 
        {
            /* A. Color Space Extraction (32x8 pixels at once) */
            t_step = __TSC;
            extractYComponentBlock32x8(&inputImg, x, y, macro_y_buffer);
            sum_color += (__TSC - t_step);

            /* B. Process 4 individual blocks (8x8) from the macro buffer */
            for (k = 0; k < 4; k++) 
            {
                /* Calculate pointer to the k-th 8x8 block inside the 32x8 macro buffer */
                block_ptr = macro_y_buffer + (k * 8);

                /* --- DCT --- */
                t_step = __TSC;
                /* Pass stride=32 because block is part of wider buffer */
                computeDCTBlock(block_ptr, dct_block, MACRO_BLOCK_WIDTH); 
                sum_dct += (__TSC - t_step);

                /* --- Quantization --- */
                t_step = __TSC;
                quantizeBlock(dct_block, quant_block);
                sum_quant += (__TSC - t_step);

                /* --- ZigZag --- */
                t_step = __TSC;
                performZigZagBlock(quant_block, zigzag_block);
                sum_zigzag += (__TSC - t_step);

                /* --- RLE --- */
                t_step = __TSC;
                syms = performRLEBlock(zigzag_block, rleCurrent, 
                                   max_rle_capacity - total_rle_symbols, 
                                   &global_last_dc);
                sum_rle += (__TSC - t_step);

                if (syms < 0) return -6; /* Overflow error */
                
                /* Advance pointers */
                rleCurrent += syms;
                total_rle_symbols += syms;
            }
        }
    }

    /* ---------------------------------------------------------------------
     * 4. FINALIZATION & PROFILING
     * ---------------------------------------------------------------------
     */
    
    /* Update DTO with counts */
    dto->rle_count = total_rle_symbols;

    /* Upisujemo cikluse u DTO (koristeci originalna imena polja) */
    dto->cycles_color_conversion = sum_color;
    dto->cycles_dct              = sum_dct;
    dto->cycles_quantization     = sum_quant;
    dto->cycles_zigzag           = sum_zigzag;
    dto->cycles_rle              = sum_rle;

    /* --- Huffman Encoding (Global Step) --- */
    t_step = __TSC;
    bytesWritten = performHuffman(rleBase, total_rle_symbols, huffData, huff_capacity);
    dto->cycles_huffman = __TSC - t_step;
    
    if (bytesWritten < 0) return -8;
    dto->huff_size = bytesWritten;

    dto->cycles_total = __TSC - t_start;

    return 0; /* Success */
}
#endif
