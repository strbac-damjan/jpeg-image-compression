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

    /* Debug pointers */
    uint8_t *debug_y_ptr;
    float   *debug_dct_ptr;
    int16_t *debug_quant_ptr;
    int16_t *debug_zigzag_ptr;

    /* Local L1 Buffers */
    /* Input: 32x8 int8 pixels */
    __attribute__((aligned(64))) int8_t macro_y_buffer[MACRO_BLOCK_WIDTH * BLOCK_SIZE]; 
    
    /* DCT Output / Quant Input: 4 blocks * 64 floats = 256 floats */
    __attribute__((aligned(64))) float macro_dct_buffer[MACRO_BLOCK_WIDTH * BLOCK_SIZE]; 
    
    /* Quant Output / ZigZag Input: 4 blocks * 64 shorts = 256 shorts (NOVO) */
    __attribute__((aligned(64))) int16_t macro_quant_buffer[MACRO_BLOCK_WIDTH * BLOCK_SIZE];

    /* ZigZag Output: 1 block * 64 shorts (ZigZag se i dalje radi sekvencijalno) */
    __attribute__((aligned(64))) int16_t zigzag_block[64];

    /* Profiling vars */
    uint64_t sum_color = 0, sum_dct = 0, sum_quant = 0, sum_zigzag = 0, sum_rle = 0;
    int16_t global_last_dc = 0;
    
    /* Loop vars */
    int y, x, k, i, row;
    int32_t syms;
    float *current_dct_ptr;
    int16_t *current_quant_ptr;

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

    /* Mapiranje debug pointera */
    debug_y_ptr      = (uint8_t *)(uintptr_t)appMemShared2TargetPtr(dto->y_phy_ptr);
    debug_dct_ptr    = (float   *)(uintptr_t)appMemShared2TargetPtr(dto->dct_phy_ptr);
    debug_quant_ptr  = (int16_t *)(uintptr_t)appMemShared2TargetPtr(dto->quant_phy_ptr);
    debug_zigzag_ptr = (int16_t *)(uintptr_t)appMemShared2TargetPtr(dto->zigzag_phy_ptr);
    
    /* ---------------------------------------------------------------------
     * 3. PIPELINE LOOP
     * ---------------------------------------------------------------------
     */
    init_ZigZag_Masks();
    for (y = 0; y < dto->height; y += BLOCK_SIZE) 
    {
        for (x = 0; x < dto->width; x += MACRO_BLOCK_WIDTH) 
        {
            /* --- A. Color Space Extraction (Macro Block) --- */
            t_step = __TSC;
            extractYComponentBlock4x8x8(&inputImg, x, y, macro_y_buffer);
            sum_color += (__TSC - t_step);

            /* --- B. DCT (Macro Block) --- */
            t_step = __TSC;
            computeDCTBlock4x8x8(macro_y_buffer, macro_dct_buffer, MACRO_BLOCK_WIDTH);
            sum_dct += (__TSC - t_step);

            /* --- C. Quantization (Macro Block - NOVO) --- */
            /* Sada se kvantizacija radi za sva 4 bloka odjednom prije ulaska u petlju */
            t_step = __TSC;
            quantizeBlock4x8x8(macro_dct_buffer, macro_quant_buffer);
            sum_quant += (__TSC - t_step);

            /* --- D. Process individual blocks (ZigZag & RLE) --- */
            /* Ovi koraci su teži za vektorizaciju zbog prirode RLE-a (promjenjiva dužina) 
             * i ZigZag-a (scattered reads), pa ih radimo u petlji.
             */
            for (k = 0; k < 4; k++) 
            {
                /* Postavljamo pointere na trenutni blok unutar macro buffera (offset za 64 elementa) */
                current_dct_ptr   = macro_dct_buffer + (k * 64);
                current_quant_ptr = macro_quant_buffer + (k * 64);

                /* --- ZigZag --- */
                t_step = __TSC;
                /* Uzimamo podatke iz macro_quant_buffer i pišemo u zigzag_block */
                performZigZagBlock(current_quant_ptr, zigzag_block);
                sum_zigzag += (__TSC - t_step);

                /* * DEBUG: SAVE ONLY THE FIRST BLOCK (0,0) TO DDR */
                if (y == 0 && x == 0 && k == 0)
                {
                    /* Save Y */
                    for(row=0; row<8; row++) {
                        for(i=0; i<8; i++) {
                            debug_y_ptr[row*8 + i] = (uint8_t)(macro_y_buffer[row*32 + i] + 128);
                        }
                    }
                    /* Save DCT */
                    for(i=0; i<64; i++) debug_dct_ptr[i] = current_dct_ptr[i];
                    
                    /* Save Quant */
                    for(i=0; i<64; i++) debug_quant_ptr[i] = current_quant_ptr[i];

                    /* Save ZigZag */
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
