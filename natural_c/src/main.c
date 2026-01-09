#include <stdio.h>
#include "jpeg_handler.h"

int main(int argc, char *argv[]) {
    // Check if sufficient arguments are provided
    // argv[0] is the program name
    // argv[1] is the input file path 
    // argv[2] is the output file path
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
       bool value = saveJPEGGrayscale(outputPath, img);
       if(value) 
       {
        printf("Save is sucesfull");
       }
    } else {
        fprintf(stderr, "Error: Failed to load image from %s\n", inputPath);
        return 1;
    }

    return 0;
}
