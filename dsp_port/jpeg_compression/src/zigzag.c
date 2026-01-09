#ifdef __C7000__
#include "jpeg_compression.h"

/* -------------------------------------------------------------------------------------
 * ZIG-ZAG LOOKUP TABLE
 * -------------------------------------------------------------------------------------
 */
static const uint8_t ZIGZAG_ORDER[64] = {
    0, 1, 8, 16, 9, 2, 3, 10,
    17, 24, 32, 25, 18, 11, 4, 5,
    12, 19, 26, 33, 40, 48, 41, 34,
    27, 20, 13, 6, 7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36,
    29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46,
    53, 60, 61, 54, 47, 55, 62, 63
};

/**
 * \brief Performs Zig-Zag scanning on quantized image blocks.
 * * Reads data from the quantized 2D image (raster order) and writes it 
 * into a linear buffer where each 64-element chunk represents one 8x8 block
 * sorted in Zig-Zag order.
 * * \param q_img      Input quantized image structure.
 * \param zigzag_out Pointer to the output buffer (linear int16_t array).
 */
void performZigZag(QuantizedImage *q_img, int16_t *zigzag_out)
{
    int width = q_img->width;
    int height = q_img->height;
    int16_t *srcData = q_img->data;
    
    // Total number of blocks counter
    int blockIndex = 0;
    
    int blockY, blockX;
    int i;

    // Loop through the image in 8x8 blocks
    for (blockY = 0; blockY < height; blockY += 8)
    {
        for (blockX = 0; blockX < width; blockX += 8)
        {
            // Calculate the starting address for this block in the output buffer
            int16_t *blockOutputPtr = &zigzag_out[blockIndex * 64];

            // Iterate 0..63 for the current block
            for (i = 0; i < 64; i++)
            {
                // Get the ZigZag coordinate index from the table
                int zzPos = ZIGZAG_ORDER[i];
                
                // Convert 1D ZigZag index (0-63) to 2D local coordinates (0-7, 0-7)
                int localRow = zzPos / 8; 
                int localCol = zzPos % 8; 

                // Calculate the index in the source image (Raster Scan)
                // Source Index = (CurrentBlockRow + localRow) * Width + (CurrentBlockCol + localCol)
                int srcIndex = (blockY + localRow) * width + (blockX + localCol);

                // Copy the value
                blockOutputPtr[i] = srcData[srcIndex];
            }

            blockIndex++;
        }
    }
}
#endif
