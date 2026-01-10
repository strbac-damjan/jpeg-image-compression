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


/**
 * \brief Main entry point for the DSP processing logic.
 * Handles address translation, memory mapping, and pipeline execution.
 * Here processor cycles are measured for each step.
 */

int32_t convertToJpeg(JPEG_COMPRESSION_DTO* dto) 
{
    uint64_t t_start, t_func_start;

    t_start = __TSC;

    BMPImage inputImg;
    YImage yImg;
    DCTImage dctImg;

    inputImg.width  = dto->width;
    inputImg.height = dto->height;
    inputImg.r = (uint8_t *)(uintptr_t)appMemShared2TargetPtr(dto->r_phy_ptr);
    inputImg.g = (uint8_t *)(uintptr_t)appMemShared2TargetPtr(dto->g_phy_ptr);
    inputImg.b = (uint8_t *)(uintptr_t)appMemShared2TargetPtr(dto->b_phy_ptr);
    yImg.data = (uint8_t *)(uintptr_t)appMemShared2TargetPtr(dto->y_phy_ptr); 
    yImg.width  = dto->width;
    yImg.height = dto->height;
    dctImg.coefficients = (float *)(uintptr_t)appMemShared2TargetPtr(dto->dct_phy_ptr);

    // 1. Color Space Conversion
    t_func_start = __TSC;
    extractYComponent(&inputImg, &yImg);
    dto->cycles_color_conversion = __TSC - t_func_start;

    // 2. DCT
    t_func_start = __TSC;
    computeDCT(&yImg, &dctImg);
    dto->cycles_dct = __TSC - t_func_start;

    QuantizedImage qImg;
    qImg.width = dto->width;
    qImg.height = dto->height;
    qImg.data = (int16_t *)(uintptr_t)appMemShared2TargetPtr(dto->quant_phy_ptr);

    // 3. Quantization
    t_func_start = __TSC;
    quantizeImage(&dctImg, &qImg);
    dto->cycles_quantization = __TSC - t_func_start;

    int16_t *zigzagData = (int16_t *)(uintptr_t)appMemShared2TargetPtr(dto->zigzag_phy_ptr);

    // 4. ZigZag
    t_func_start = __TSC;
    performZigZag(&qImg, zigzagData);
    dto->cycles_zigzag = __TSC - t_func_start;

    RLESymbol *rleData = (RLESymbol *)(uintptr_t)appMemShared2TargetPtr(dto->rle_phy_ptr);
    int32_t max_rle_capacity = dto->width * dto->height * 2; 

    // 5. RLE
    t_func_start = __TSC;
    int32_t produced_symbols = performRLE(zigzagData, dto->width, dto->height, rleData, max_rle_capacity);
    dto->cycles_rle = __TSC - t_func_start;

    if (produced_symbols < 0) return -6;
    dto->rle_count = produced_symbols;

    uint8_t *huffData = (uint8_t *)(uintptr_t)appMemShared2TargetPtr(dto->huff_phy_ptr);
    int32_t huff_capacity = dto->width * dto->height; 

    // 6. Huffman
    t_func_start = __TSC;
    int32_t bytesWritten = performHuffman(rleData, dto->rle_count, huffData, huff_capacity);
    dto->cycles_huffman = __TSC - t_func_start;
    
    if (bytesWritten < 0) return -8;
    dto->huff_size = bytesWritten;

    dto->cycles_total = __TSC - t_start;

    return 0; // Success
}

#endif
