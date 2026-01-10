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
     * 1. DECLARATIONS
     * ---------------------------------------------------------------------
     */
    uint64_t t_start, t_step;
    BMPImage inputImg;
    RLESymbol *rleBase, *rleCurrent;
    int32_t max_rle_capacity, total_rle_symbols;
    uint8_t *huffData;
    int32_t huff_capacity, bytesWritten;

    /* Debug pointers (mapped only if needed) */
    uint8_t *debug_y_ptr;
    float   *debug_dct_ptr;
    int16_t *debug_quant_ptr;
    int16_t *debug_zigzag_ptr;

    /* Local L1 Buffers */
    __attribute__((aligned(64))) int8_t macro_y_buffer[MACRO_BLOCK_WIDTH * BLOCK_SIZE]; 
    __attribute__((aligned(64))) float   dct_block[64];    
    __attribute__((aligned(64))) int16_t quant_block[64];
    __attribute__((aligned(64))) int16_t zigzag_block[64];

    /* Profiling vars */
    uint64_t sum_color = 0, sum_dct = 0, sum_quant = 0, sum_zigzag = 0, sum_rle = 0;
    int16_t global_last_dc = 0;
    
    /* Loop vars */
    int y, x, k, i, row;
    int32_t syms;
    int8_t *block_ptr;

    /* ---------------------------------------------------------------------
     * 2. INITIALIZATION
     * ---------------------------------------------------------------------
     */
    t_start = __TSC;

    inputImg.width  = dto->width;
    inputImg.height = dto->height;
    inputImg.r = (uint8_t *)(uintptr_t)appMemShared2TargetPtr(dto->r_phy_ptr);
    inputImg.g = (uint8_t *)(uintptr_t)appMemShared2TargetPtr(dto->g_phy_ptr);
    inputImg.b = (uint8_t *)(uintptr_t)appMemShared2TargetPtr(dto->b_phy_ptr);

    rleBase = (RLESymbol *)(uintptr_t)appMemShared2TargetPtr(dto->rle_phy_ptr);
    rleCurrent = rleBase;
    max_rle_capacity = dto->width * dto->height * 2; 
    total_rle_symbols = 0;

    huffData = (uint8_t *)(uintptr_t)appMemShared2TargetPtr(dto->huff_phy_ptr);
    huff_capacity = dto->width * dto->height;

    /* Mapiranje debug pointera (da mozemo pisati u njih za prvi blok) */
    debug_y_ptr      = (uint8_t *)(uintptr_t)appMemShared2TargetPtr(dto->y_phy_ptr);
    debug_dct_ptr    = (float   *)(uintptr_t)appMemShared2TargetPtr(dto->dct_phy_ptr);
    debug_quant_ptr  = (int16_t *)(uintptr_t)appMemShared2TargetPtr(dto->quant_phy_ptr);
    debug_zigzag_ptr = (int16_t *)(uintptr_t)appMemShared2TargetPtr(dto->zigzag_phy_ptr);

    /* ---------------------------------------------------------------------
     * 3. PIPELINE LOOP
     * ---------------------------------------------------------------------
     */
    for (y = 0; y < dto->height; y += BLOCK_SIZE) 
    {
        for (x = 0; x < dto->width; x += MACRO_BLOCK_WIDTH) 
        {
            /* A. Color Space Extraction */
            t_step = __TSC;
            extractYComponentBlock32x8(&inputImg, x, y, macro_y_buffer);
            sum_color += (__TSC - t_step);

            /* B. Process 4 blocks */
            for (k = 0; k < 4; k++) 
            {
                block_ptr = macro_y_buffer + (k * 8);

                /* --- DCT --- */
                t_step = __TSC;
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

                /* * DEBUG: SAVE ONLY THE FIRST BLOCK (0,0) TO DDR 
                 * Ovo omogucava Main funkciji da ispise sta se desava.
                 */
                if (y == 0 && x == 0 && k == 0)
                {
                    /* Save Y (Input to DCT). Note: macro buffer is strided 32! */
                    for(row=0; row<8; row++) {
                        for(i=0; i<8; i++) {
                            /* Pretvaramo int8 (-128..127) nazad u uint8 (0..255) za pregled */
                            debug_y_ptr[row*8 + i] = (uint8_t)(block_ptr[row*32 + i] + 128);
                        }
                    }
                    /* Save DCT (Linear copy) */
                    for(i=0; i<64; i++) debug_dct_ptr[i] = dct_block[i];
                    
                    /* Save Quant (Linear copy) */
                    for(i=0; i<64; i++) debug_quant_ptr[i] = quant_block[i];

                    /* Save ZigZag (Linear copy) */
                    for(i=0; i<64; i++) debug_zigzag_ptr[i] = zigzag_block[i];
                }

                /* --- RLE --- */
                t_step = __TSC;
                syms = performRLEBlock(zigzag_block, rleCurrent, 
                                   max_rle_capacity - total_rle_symbols, 
                                   &global_last_dc);
                sum_rle += (__TSC - t_step);

                if (syms < 0) return -6;
                rleCurrent += syms;
                total_rle_symbols += syms;
            }
        }
    }

    /* 4. FINALIZATION */
    dto->rle_count = total_rle_symbols;
    dto->cycles_color_conversion = sum_color;
    dto->cycles_dct = sum_dct;
    dto->cycles_quantization = sum_quant;
    dto->cycles_zigzag = sum_zigzag;
    dto->cycles_rle = sum_rle;

    t_step = __TSC;
    bytesWritten = performHuffman(rleBase, total_rle_symbols, huffData, huff_capacity);
    dto->cycles_huffman = __TSC - t_step;
    
    if (bytesWritten < 0) return -8;
    dto->huff_size = bytesWritten;
    dto->cycles_total = __TSC - t_start;

    return 0;
}
#endif
