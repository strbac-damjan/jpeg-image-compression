#include "bmp_handler.h"

// Helper function to free the image memory
void freeBMPImage(BMPImage* image) {
    if (image) {
        // Moramo ponovo izracunati velicinu da bi znali koliko memorije osloboditi
        // appMemFree zahtijeva 'size' parametar.
        uint32_t size = image->width * image->height;

        if (image->r) appMemFree(APP_MEM_HEAP_DDR, image->r, size);
        if (image->g) appMemFree(APP_MEM_HEAP_DDR, image->g, size);
        if (image->b) appMemFree(APP_MEM_HEAP_DDR, image->b, size);
        
        // Strukturu oslobadjamo obicnim free jer je alocirana obicnim malloc-om
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

    // Strukturu alociramo na heapu (CPU only), to je OK.
    BMPImage* image = (BMPImage*)malloc(sizeof(BMPImage));
    if (!image) {
        fprintf(stderr, "Error: Memory allocation failed for BMPImage struct.\n");
        fclose(file);
        return NULL;
    }
    // Initialize pointers to NULL for safe freeing on error
    image->r = NULL;
    image->g = NULL;
    image->b = NULL;

    image->width = infoHeader.biWidth;
    image->height = infoHeader.biHeight;

    bool flipVertical = true;
    if (image->height < 0) {
        image->height = -image->height;
        flipVertical = false;
    }

    int rowPadded = (image->width * 3 + 3) & (~3);
    size_t pixelCount = (size_t)image->width * image->height;

    // --- PROMJENA: Alociranje Shared Memory za kanale ---
    // Koristimo appMemAlloc sa poravnanjem od 64 bajta (kljucno za C7x vektore)
    image->r = (uint8_t*)appMemAlloc(APP_MEM_HEAP_DDR, pixelCount, 64);
    image->g = (uint8_t*)appMemAlloc(APP_MEM_HEAP_DDR, pixelCount, 64);
    image->b = (uint8_t*)appMemAlloc(APP_MEM_HEAP_DDR, pixelCount, 64);

    if (!image->r || !image->g || !image->b) {
        fprintf(stderr, "Error: Shared Memory allocation failed for pixel channels.\n");
        freeBMPImage(image); // This handles freeing whatever was allocated
        fclose(file);
        return NULL;
    }

    if (fseek(file, fileHeader.bfOffBits, SEEK_SET) != 0) {
        fprintf(stderr, "Error: Unable to seek to bitmap data.\n");
        freeBMPImage(image);
        fclose(file);
        return NULL;
    }

    // rowDataBuffer je privremeni buffer samo za CPU, on moze ostati obican malloc
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
            // Calculate linear index for this pixel
            int pixelIdx = destRow * image->width + j;

            // BMP is BGR
            uint8_t B = rowDataBuffer[j * 3 + 0];
            uint8_t G = rowDataBuffer[j * 3 + 1];
            uint8_t R = rowDataBuffer[j * 3 + 2];

            // Store in separate arrays (koji su sada u shared memory)
            image->r[pixelIdx] = R;
            image->g[pixelIdx] = G;
            image->b[pixelIdx] = B;
        }
    }

    free(rowDataBuffer);
    fclose(file);
    return image;
}

// Saves a BMP image to a file.
// Ovu funkciju ne moramo mijenjati jer CPU (A72) moze citati i shared memory 
// preko virtualnih adresa koje su sacuvane u image->r/g/b.
bool saveBMPImage(const char* filename, const BMPImage* image) {
    if (!image || !image->r || !image->g || !image->b) return false;

    int rowSize = image->width * 3;
    int rowPadded = (rowSize + 3) & (~3);
    uint32_t dataSize = rowPadded * image->height;

    BMPFileHeader fileHeader;
    fileHeader.bfType = 0x4D42; 
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
    infoHeader.biXPelsPerMeter = 2835;
    infoHeader.biYPelsPerMeter = 2835;
    infoHeader.biClrUsed = 0;
    infoHeader.biClrImportant = 0;

    FILE* file = fopen(filename, "wb");
    if (!file) {
        fprintf(stderr, "Error: Could not open file for writing: %s\n", filename);
        return false;
    }

    if (fwrite(&fileHeader, sizeof(BMPFileHeader), 1, file) != 1) {
        fclose(file);
        return false;
    }
    if (fwrite(&infoHeader, sizeof(BMPInfoHeader), 1, file) != 1) {
        fclose(file);
        return false;
    }

    uint8_t* rowData = (uint8_t*)calloc(rowPadded, 1);
    if (!rowData) {
        fclose(file);
        return false;
    }

    // Iterate bottom-up (standard BMP)
    for (int i = image->height - 1; i >= 0; i--) {
        for (int j = 0; j < image->width; j++) {
            // Get linear index for the pixel
            int pixelIdx = i * image->width + j;

            // Retrieve from separate arrays
            uint8_t R = image->r[pixelIdx];
            uint8_t G = image->g[pixelIdx];
            uint8_t B = image->b[pixelIdx];
            
            // Pack into BGR row buffer
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
