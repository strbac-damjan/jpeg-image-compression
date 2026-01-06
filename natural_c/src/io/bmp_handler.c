#include "bmp_handler.h"


// Helper function to free the image memory
void freeBMPImage(BMPImage* image) {
    if (image) {
        if (image->data) {
            free(image->data);
        }
        free(image);
    }
}

// Loads a BMP image from a file. Returns NULL on error.
BMPImage* loadBMPImage(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "Error: Unable to open file: %s\n", filename);
        return NULL;
    }

    BMPFileHeader fileHeader;
    if (fread(&fileHeader, sizeof(BMPFileHeader), 1, file) != 1) {
        fprintf(stderr, "Error: Failed to read BMP file header.\n");
        fclose(file);
        return NULL;
    }

    // Check if the file is a BMP file by checking the magic number
    if (fileHeader.bfType != 0x4D42) {
        fprintf(stderr, "Error: File is not a valid BMP file.\n");
        fclose(file);
        return NULL;
    }

    BMPInfoHeader infoHeader;
    if (fread(&infoHeader, sizeof(BMPInfoHeader), 1, file) != 1) {
        fprintf(stderr, "Error: Failed to read BMP info header.\n");
        fclose(file);
        return NULL;
    }

    // Check if the image is 24-bit and uncompressed
    if (infoHeader.biBitCount != 24) {
        fprintf(stderr, "Error: Only 24-bit BMP images are supported.\n");
        fclose(file);
        return NULL;
    }
    if (infoHeader.biCompression != 0) {
        fprintf(stderr, "Error: Compressed BMP images are not supported.\n");
        fclose(file);
        return NULL;
    }

    // Allocate memory for the BMPImage structure
    BMPImage* image = (BMPImage*)malloc(sizeof(BMPImage));
    if (!image) {
        fprintf(stderr, "Error: Memory allocation failed for BMPImage struct.\n");
        fclose(file);
        return NULL;
    }

    image->width = infoHeader.biWidth;
    image->height = infoHeader.biHeight;

    // If the image height is negative, the image is in top-down format;
    // otherwise, we need to invert the rows to store them in top-down format.
    bool flipVertical = true;
    if (image->height < 0) {
        image->height = -image->height;
        flipVertical = false;
    }

    // Every row of pixel data is padded to a multiple of 4 bytes.
    int rowPadded = (image->width * 3 + 3) & (~3);
    
    // Allocate memory for pixel data
    size_t dataSize = image->width * image->height * 3;
    image->data = (uint8_t*)malloc(dataSize);
    if (!image->data) {
        fprintf(stderr, "Error: Memory allocation failed for pixel data.\n");
        free(image);
        fclose(file);
        return NULL;
    }

    // Seek to the offset where the pixel data starts
    if (fseek(file, fileHeader.bfOffBits, SEEK_SET) != 0) {
        fprintf(stderr, "Error: Unable to seek to bitmap data.\n");
        freeBMPImage(image);
        fclose(file);
        return NULL;
    }

    uint8_t* rowDataBuffer = (uint8_t*)malloc(rowPadded);
    if (!rowDataBuffer) {
        fprintf(stderr, "Error: Memory allocation failed for row buffer.\n");
        freeBMPImage(image);
        fclose(file);
        return NULL;
    }

    for (int i = 0; i < image->height; i++) {
        if (fread(rowDataBuffer, 1, rowPadded, file) != (size_t)rowPadded) {
            fprintf(stderr, "Error: Insufficient data reading row %d\n", i);
            free(rowDataBuffer);
            freeBMPImage(image);
            fclose(file);
            return NULL;
        }

        int destRow = flipVertical ? (image->height - 1 - i) : i;
        for (int j = 0; j < image->width; j++) {
            // BMP format stores pixels in BGR order, so we need to swap them to RGB.
            uint8_t B = rowDataBuffer[j * 3 + 0];
            uint8_t G = rowDataBuffer[j * 3 + 1];
            uint8_t R = rowDataBuffer[j * 3 + 2];

            int idx = (destRow * image->width + j) * 3;
            image->data[idx + 0] = R;
            image->data[idx + 1] = G;
            image->data[idx + 2] = B;
        }
    }

    free(rowDataBuffer);
    fclose(file);
    return image;
}

// Saves a BMP image to a file (24-bit uncompressed BMP format).
// Returns true if the operation is successful, otherwise false.
bool saveBMPImage(const char* filename, const BMPImage* image) {
    if (!image || !image->data) return false;

    // Calculating the size of a row with padding (row must be aligned to 4 bytes)
    int rowSize = image->width * 3;
    int rowPadded = (rowSize + 3) & (~3);
    uint32_t dataSize = rowPadded * image->height;

    // Preparing BMP file header
    BMPFileHeader fileHeader;
    fileHeader.bfType = 0x4D42; // "BM"
    fileHeader.bfOffBits = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader);
    fileHeader.bfSize = fileHeader.bfOffBits + dataSize;
    fileHeader.bfReserved1 = 0;
    fileHeader.bfReserved2 = 0;

    BMPInfoHeader infoHeader;
    infoHeader.biSize = sizeof(BMPInfoHeader);
    infoHeader.biWidth = image->width;
    infoHeader.biHeight = image->height;
    infoHeader.biPlanes = 1;
    infoHeader.biBitCount = 24;
    infoHeader.biCompression = 0;
    infoHeader.biSizeImage = dataSize;

    // Using arbitrary values for resolution (e.g., 2835 pixels/meter ~ 72 DPI)
    infoHeader.biXPelsPerMeter = 2835;
    infoHeader.biYPelsPerMeter = 2835;
    infoHeader.biClrUsed = 0;
    infoHeader.biClrImportant = 0;

    FILE* file = fopen(filename, "wb");
    if (!file) {
        fprintf(stderr, "Error: Could not open file for writing: %s\n", filename);
        return false;
    }

    // Write the file header and info header to the file
    if (fwrite(&fileHeader, sizeof(BMPFileHeader), 1, file) != 1) {
        fclose(file);
        return false;
    }
    if (fwrite(&infoHeader, sizeof(BMPInfoHeader), 1, file) != 1) {
        fclose(file);
        return false;
    }

    // Preparing a temporary buffer for the row (with padding bytes)
    uint8_t* rowData = (uint8_t*)calloc(rowPadded, 1); // calloc initializes to 0
    if (!rowData) {
        fclose(file);
        return false;
    }

    // BMP expects bottom-up order; since the data in BMPImage is in top-down order, 
    // we iterate in reverse order.
    for (int i = image->height - 1; i >= 0; i--) {
        for (int j = 0; j < image->width; j++) {
            int idx = (i * image->width + j) * 3;
            uint8_t R = image->data[idx + 0];
            uint8_t G = image->data[idx + 1];
            uint8_t B = image->data[idx + 2];
            
            // BMP format stores pixels in BGR order
            rowData[j * 3 + 0] = B;
            rowData[j * 3 + 1] = G;
            rowData[j * 3 + 2] = R;
        }
        if (fwrite(rowData, 1, rowPadded, file) != (size_t)rowPadded) {
            free(rowData);
            fclose(file);
            return false;
        }
    }

    free(rowData);
    fclose(file);
    return true;
}