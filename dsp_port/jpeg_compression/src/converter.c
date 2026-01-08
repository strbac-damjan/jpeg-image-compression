#ifdef __C7000__
/* TI C7x DSP Intrinsic definitions */
#include <c7x.h>
/* Memory utility for address translation (Shared -> Target) */
#include <utils/mem/include/app_mem.h> 
/* Project specific headers */
#include <jpeg_compression.h>
#include <stdio.h> 

/**
 * \brief Extracts the Luminance (Y) component AND performs Level Shifting (Centering).
 * * Process:
 * 1. Convert RGB to Y (Luminance) -> Range [0, 255]
 * 2. Level Shift: Subtract 128 -> Range [-128, 127]
 * * Formula: Y_centered = ((77*R + 150*G + 29*B) >> 8) - 128
 *
 * \param img    Pointer to the source BMP image structure.
 * \param y_out  Pointer to the destination Y-component structure.
 */
void extractYComponent(BMPImage *img, YImage *y_out) 
{
    int32_t total_pixels = img->width * img->height;
    
    // Process 32 pixels per vector
    int32_t num_vectors = total_pixels / 32;
    int32_t i; 

    // --- Pointer Setup ---
    uchar32 * restrict vec_r = (uchar32 *) img->r;
    uchar32 * restrict vec_g = (uchar32 *) img->g;
    uchar32 * restrict vec_b = (uchar32 *) img->b;
    
    // CHANGE: Cast destination to (char32 *) instead of (uchar32 *)
    // Since we are writing signed values (-128 to 127), using a signed vector pointer
    // matches the return type of convert_char32() and avoids the illegal cast error.
    char32 * restrict vec_y = (char32 *) y_out->data;

    // --- Coefficient Initialization ---
    short32 coeff_r = (short32) 77;
    short32 coeff_g = (short32) 150;
    short32 coeff_b = (short32) 29;
    
    // Offset for centering (Level Shift)
    short32 val_128 = (short32) 128;

    // --- Vector Loop ---
    for (i = 0; i < num_vectors; i++) {
        // 1. Load Data
        uchar32 r_in = vec_r[i];
        uchar32 g_in = vec_g[i];
        uchar32 b_in = vec_b[i];

        // 2. Unpack (8-bit -> 16-bit)
        short32 r_s = convert_short32(r_in);
        short32 g_s = convert_short32(g_in);
        short32 b_s = convert_short32(b_in);

        // 3. Math Operations (RGB -> Y)
        short32 y_temp = (r_s * coeff_r) + (g_s * coeff_g) + (b_s * coeff_b);

        // 4. Scaling (normalize to 0-255)
        y_temp = y_temp >> 8;
        
        // 5. Centering / Level Shifting
        // Current range: [0, 255] -> New range: [-128, 127]
        y_temp = y_temp - val_128;

        // 6. Pack / Store (16-bit -> 8-bit)
        // We use convert_char32 which returns a 'char32' (signed vector).
        // Since vec_y is now 'char32*', direct assignment is legal.
        // No explicit cast (uchar32) is needed anymore.
        vec_y[i] = convert_char32(y_temp);
    }
}

int32_t convertToJpeg(JPEG_COMPRESSION_DTO* dto) 
{
    BMPImage inputImg;
    YImage outputImg;

    // Set dimensions
    inputImg.width = dto->width;
    inputImg.height = dto->height;
    outputImg.width = dto->width;
    outputImg.height = dto->height;

    // Address Translation (Physical -> Virtual)
    inputImg.r = (uint8_t *)(uintptr_t)appMemShared2TargetPtr(dto->r_phy_ptr);
    inputImg.g = (uint8_t *)(uintptr_t)appMemShared2TargetPtr(dto->g_phy_ptr);
    inputImg.b = (uint8_t *)(uintptr_t)appMemShared2TargetPtr(dto->b_phy_ptr);
    
    outputImg.data = (uint8_t *)(uintptr_t)appMemShared2TargetPtr(dto->y_phy_ptr);

    // Safety Check
    if (inputImg.r == NULL || inputImg.g == NULL || inputImg.b == NULL || outputImg.data == NULL) {
        return -1; 
    }

    // Perform vector processing (Extraction + Centering)
    extractYComponent(&inputImg, &outputImg);

    return 0; // Success
}
#endif
