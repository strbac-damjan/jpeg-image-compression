#include <stdio.h>
#include "bmp_handler.h"

int main(int argc, char *argv[]) {
    // Check if sufficient arguments are provided
    // argv[0] is the program name
    // argv[1] is the input file path (passed via INPUT=... in Make)
    // argv[2] is the output file path (automatically set in Make)
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input_file_path> <output_file_path>\n", argv[0]);
        return 1;
    }

    const char* inputPath = argv[1];
    const char* outputPath = argv[2];

    printf("Starting processing...\n");
    printf("Input: %s\n", inputPath);

    // Load BMP using the provided path
    BMPImage* img = loadBMPImage(inputPath);
    
    if (img) {
        printf("Image loaded: %dx%d pixels\n", img->width, img->height);
        
        // Save BMP to the output path defined by Makefile
        if (saveBMPImage(outputPath, img)) {
            printf("Image saved successfully to: %s\n", outputPath);
        } else {
            fprintf(stderr, "Error: Failed to save image.\n");
        }
        
        freeBMPImage(img); // Important: Free memory!
    } else {
        fprintf(stderr, "Error: Failed to load image from %s\n", inputPath);
        return 1;
    }

    return 0;
}