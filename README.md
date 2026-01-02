# jpeg-image-compression

This project is a custom C implementation of the JPEG image compression standard. It reads raw bitmap (BMP) images, processes them using JPEG compression algorithms (DCT, Quantization, Huffman coding), and outputs the result.

## Project structure

Here is an overview of the repository organization and the purpose of each directory:

```text
.
├── assets/              # Contains test images and results
│   ├── input/           # Source BMP images to be compressed
│   └── output/          # Resulting images after processing
├── build/               # Compiled object files (.o) and the final executable
├── include/             # Header files (.h) defining interfaces and constants
├── src/                 # Source code (.c)
│   ├── core/            # Core project functionality
│   ├── io/              # I/O implementation for file formats
│   └── main.c           # Entry point of the application
├── LICENSE              # Project license information
├── Makefile             # Build configuration and automation
└── README.md            # Project documentation
```

## How to run
1. Run the command `make` which will build the project. This will generate `jpeg_compression_app` in the build folder.
2. Run the binary like `./build/jpeg_compression_app {path to input image} {name of output image}`
3. Image will be saved in the `assets/output/` folder