#!/bin/bash

mkdir -p assets/difference

# Folder paths
INPUT_DIR="assets/input"
OUTPUT_DIR="assets/output"
DIFF_DIR="assets/difference"

echo "Starting analysis"

# Iterate through .bmp files
for filepath in "$INPUT_DIR"/*.bmp; do
    
    # Get file name
    filename=$(basename -- "$filepath")
    
    # Remove extension
    name="${filename%.*}"

    # Check if .jpeg file exists
    if [ -f "$OUTPUT_DIR/$name.jpeg" ]; then
        echo "Analyzing image: $name"
        
        # Run script
        python3 analyze_results.py \
            "$INPUT_DIR/$name.bmp" \
            "$OUTPUT_DIR/$name.jpeg" \
            -o "$DIFF_DIR/$name.png"
    else
        echo "ERROR: Couldn't find $OUTPUT_DIR/$name.jpeg - skipping."
    fi

done

echo "Done."