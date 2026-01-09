#ifdef __C7000__
/* TI C7x DSP Intrinsic definitions */
#include <c7x.h>
/* Memory utility for address translation (Shared -> Target) */
#include <utils/mem/include/app_mem.h> 
/* Project specific headers */
#include <jpeg_compression.h>
#include <stdio.h> 

/**
 * \brief Extracts the Luminance (Y) component and performs Level Shifting (Centering).
 * * This function converts RGB input to Luminance (Y) and then subtracts 128 
 * to center the data around zero. This level shifting is a required step 
 * before Discrete Cosine Transform (DCT) to reduce dynamic range requirements.
 * * Formula: Y_centered = ((77*R + 150*G + 29*B) >> 8) - 128
 * resulting in a range of [-128, 127].
 *
 * \param img    Pointer to the source BMP image structure.
 * \param y_out  Pointer to the destination Y-component structure.
 */
void extractYComponent(BMPImage *img, YImage *y_out) 
{
    int32_t total_pixels = img->width * img->height;
    
    // Process 32 pixels per vector iteration
    int32_t num_vectors = total_pixels / 32;
    int32_t i; 

    /* Input Pointer Setup (Unsigned 8-bit vectors) */
    uchar32 * restrict vec_r = (uchar32 *) img->r;
    uchar32 * restrict vec_g = (uchar32 *) img->g;
    uchar32 * restrict vec_b = (uchar32 *) img->b;
    
    /* Output Pointer Setup (Signed 8-bit vector)
     * The destination is cast to char32* because the result of the level 
     * shifting is a signed value [-128, 127]. This matches the return type 
     * of the pack instructions used later.
     */
    char32 * restrict vec_y = (char32 *) y_out->data;

    /* Coefficient Initialization for RGB to Y conversion */
    short32 coeff_r = (short32) 77;
    short32 coeff_g = (short32) 150;
    short32 coeff_b = (short32) 29;
    
    /* Offset for Level Shifting */
    short32 val_128 = (short32) 128;

    /* Vectorized Loop */
    for (i = 0; i < num_vectors; i++) {
        // Load Data from input channels
        uchar32 r_in = vec_r[i];
        uchar32 g_in = vec_g[i];
        uchar32 b_in = vec_b[i];

        // Unpack 8-bit data to 16-bit to prevent overflow during multiplication
        short32 r_s = convert_short32(r_in);
        short32 g_s = convert_short32(g_in);
        short32 b_s = convert_short32(b_in);

        // Calculate Y component (Matrix multiplication equivalent)
        short32 y_temp = (r_s * coeff_r) + (g_s * coeff_g) + (b_s * coeff_b);

        // Normalize the result to 8-bit range [0, 255]
        y_temp = y_temp >> 8;
        
        // Apply Level Shifting
        // Subtracts 128 to move range from [0, 255] to [-128, 127]
        y_temp = y_temp - val_128;

        // Pack 16-bit data back to 8-bit signed vector and store
        // convert_char32 handles the conversion to signed 8-bit elements.
        vec_y[i] = convert_char32(y_temp);
    }
}

/**
 * \brief Main entry point for the DSP processing logic.
 * Handles address translation, memory mapping, and pipeline execution.
 */
int32_t convertToJpeg(JPEG_COMPRESSION_DTO* dto) 
{
    BMPImage inputImg;
    YImage yImg;
    DCTImage dctImg;

    /* Address Translation (Physical -> Virtual)
     * The pointers received in the DTO are physical addresses (DDR).
     * These must be mapped to the C7x virtual address space using appMemShared2TargetPtr.
     */
    inputImg.width  = dto->width;
    inputImg.height = dto->height;
    inputImg.r = (uint8_t *)(uintptr_t)appMemShared2TargetPtr(dto->r_phy_ptr);
    inputImg.g = (uint8_t *)(uintptr_t)appMemShared2TargetPtr(dto->g_phy_ptr);
    inputImg.b = (uint8_t *)(uintptr_t)appMemShared2TargetPtr(dto->b_phy_ptr);
    
    yImg.width  = dto->width;
    yImg.height = dto->height;
    // The Y data is treated as signed 8-bit internally for DCT processing
    yImg.data = (uint8_t *)(uintptr_t)appMemShared2TargetPtr(dto->y_phy_ptr); 

    dctImg.width = dto->width;
    dctImg.height = dto->height;
    
    // Validate existence of the DCT output buffer
    if (dto->dct_phy_ptr != 0) {
        dctImg.coefficients = (float *)(uintptr_t)appMemShared2TargetPtr(dto->dct_phy_ptr);
    } else {
        return -2; // Error: No output buffer for DCT provided
    }

    // Safety check for null pointers after translation
    if (inputImg.r == NULL || yImg.data == NULL || dctImg.coefficients == NULL) {
        return -1; 
    }

    // Color Space Conversion and Level Shifting (Vectorized)
    extractYComponent(&inputImg, &yImg);

    // Forward Discrete Cosine Transform (Vectorized)
    computeDCT(&yImg, &dctImg);

    QuantizedImage qImg;
    qImg.width = dto->width;
    qImg.height = dto->height;
    
    if (dto->quant_phy_ptr != 0) {
        qImg.data = (int16_t *)(uintptr_t)appMemShared2TargetPtr(dto->quant_phy_ptr);
    } else {
        return -3; // Error: No output buffer for Quantization provided
    }

    quantizeImage(&dctImg, &qImg);

    return 0; // Success
}
#endif
