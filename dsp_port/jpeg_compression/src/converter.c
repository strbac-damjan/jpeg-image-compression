#ifdef __C7000__
/* TI C7x DSP Intrinsic definitions */
#include <c7x.h>
/* Memory utility for address translation (Shared -> Target) */
#include <utils/mem/include/app_mem.h> 
/* Project specific headers */
#include <jpeg_compression.h>
#include <stdio.h> 

/**
 * \brief Extracts the Luminance (Y) component from RGB data using SIMD vectors.
 * * This function utilizes the C7x vector engine to process 32 pixels simultaneously per iteration.
 * The formula used is standard JPEG conversion: Y = (0.299*R + 0.587*G + 0.114*B).
 * Fixed-point approximation: Y = (77*R + 150*G + 29*B) >> 8.
 *
 * \param img    Pointer to the source BMP image structure (host pointers converted to DSP pointers).
 * \param y_out  Pointer to the destination Y-component structure.
 */
void extractYComponent(BMPImage *img, YImage *y_out) 
{
    int32_t total_pixels = img->width * img->height;
    
    // We process 32 pixels at a time because the C7x vector registers (short32/uchar64) 
    // allow us to handle 512 bits of data.
    // For 8-bit data processing extended to 16-bit, a block of 32 fits perfectly.
    int32_t num_vectors = total_pixels / 32;
    int32_t i; 

    // --- Pointer Setup ---
    // We cast the byte pointers (uint8_t*) to vector pointers (uchar32*).
    // The 'restrict' keyword tells the compiler that these memory regions do not overlap,
    // allowing for aggressive optimization (pipelining).
    uchar32 * restrict vec_r = (uchar32 *) img->r;
    uchar32 * restrict vec_g = (uchar32 *) img->g;
    uchar32 * restrict vec_b = (uchar32 *) img->b;
    uchar32 * restrict vec_y = (uchar32 *) y_out->data;

    // --- Coefficient Initialization ---
    // Initialize vectors where every element contains the specific coefficient.
    // (short32) 77 creates a vector: [77, 77, 77, ... 77] (32 times).
    // We use 'short' (16-bit) to prevent overflow during multiplication.
    short32 coeff_r = (short32) 77;
    short32 coeff_g = (short32) 150;
    short32 coeff_b = (short32) 29;

    // --- Vector Loop ---
    for (i = 0; i < num_vectors; i++) {
        // 1. Load Data
        // Load 32 consecutive pixels (bytes) for each channel.
        uchar32 r_in = vec_r[i];
        uchar32 g_in = vec_g[i];
        uchar32 b_in = vec_b[i];

        // 2. Unpack / Type Conversion (8-bit -> 16-bit)
        // 'convert_short32' promotes the 8-bit unsigned chars to 16-bit signed shorts.
        // This expands the data width to allow multiplication without immediate overflow.
        short32 r_s = convert_short32(r_in);
        short32 g_s = convert_short32(g_in);
        short32 b_s = convert_short32(b_in);

        // 3. Math Operations (SIMD)
        // Perform the weighted sum on 32 pixels in parallel.
        // The operations happen on full 512-bit registers.
        short32 y_temp = (r_s * coeff_r) + (g_s * coeff_g) + (b_s * coeff_b);

        // 4. Scaling
        // Bitwise shift right by 8 is equivalent to dividing by 256.
        // This normalizes the result back to the 0-255 range.
        y_temp = y_temp >> 8;

        // 5. Pack / Store (16-bit -> 8-bit)
        // 'convert_uchar32' takes the 16-bit results, truncates/saturates them 
        // back to 8-bit bytes, and packs them into a uchar32 vector.
        // Finally, store the result into the output buffer.
        vec_y[i] = convert_uchar32(y_temp);
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

    // The addresses in 'dto' are physical addresses (uint64_t) from the system memory map.
    // The DSP (C7x) operates on a local Virtual Address view.
    // 'appMemShared2TargetPtr' translates the Shared/Physical address to the Target/Virtual address.
    
    // Note: We cast to (uintptr_t) first to handle the 64-bit to pointer conversion cleanly.
    inputImg.r = (uint8_t *)(uintptr_t)appMemShared2TargetPtr(dto->r_phy_ptr);
    inputImg.g = (uint8_t *)(uintptr_t)appMemShared2TargetPtr(dto->g_phy_ptr);
    inputImg.b = (uint8_t *)(uintptr_t)appMemShared2TargetPtr(dto->b_phy_ptr);
    
    outputImg.data = (uint8_t *)(uintptr_t)appMemShared2TargetPtr(dto->y_phy_ptr);

    // --- Safety Check ---
    // Ensure all pointers were translated successfully and are not NULL.
    if (inputImg.r == NULL || inputImg.g == NULL || inputImg.b == NULL || outputImg.data == NULL) {
        // Return -1 to indicate memory mapping failure
        return -1; 
    }

    // Perform the vector processing
    extractYComponent(&inputImg, &outputImg);

    return 0; // Success
}
#endif
