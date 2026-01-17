import argparse
import os
import sys
import math
import numpy as np
import matplotlib.pyplot as plt
from PIL import Image

def calculate_mse(image1, image2):
    """
    Calculates the Mean Squared Error (MSE) between two images.
    """
    img1_data = np.asarray(image1, dtype=np.float64)
    img2_data = np.asarray(image2, dtype=np.float64)
    
    diff = img1_data - img2_data
    sqr_diff = diff ** 2
    mse = np.mean(sqr_diff)
    return mse

def calculate_psnr(mse):
    """
    Calculates Peak Signal-to-Noise Ratio (PSNR).
    """
    if mse == 0:
        return float('inf') 
    max_pixel = 255.0
    psnr = 20 * math.log10(max_pixel / math.sqrt(mse))
    return psnr

def main():
    parser = argparse.ArgumentParser(description='Analyze JPEG compression quality.')
    parser.add_argument('original_path', type=str, help='Path to the original .bmp image')
    parser.add_argument('compressed_path', type=str, help='Path to the compressed .jpg image')
    parser.add_argument('-o', '--output', type=str, default='analysis_result.png', help='Path to save the resulting plot')
    
    args = parser.parse_args()

    if not os.path.exists(args.original_path):
        print(f"Error: The file '{args.original_path}' was not found.")
        sys.exit(1)
    
    if not os.path.exists(args.compressed_path):
        print(f"Error: The file '{args.compressed_path}' was not found.")
        sys.exit(1)

    try:
        print(f"Loading images...")
        img_orig = Image.open(args.original_path)
        img_comp = Image.open(args.compressed_path)

        # Convert original to Grayscale
        img_orig_gray = img_orig.convert('L')
        
        if img_orig_gray.size != img_comp.size:
            print(f"Warning: Dimensions do not match! Resizing original.")
            img_orig_gray = img_orig_gray.resize(img_comp.size)

        # Calculate Metrics
        mse_value = calculate_mse(img_orig_gray, img_comp)
        psnr_value = calculate_psnr(mse_value)

        # Console Output
        print("-" * 40)
        print(f"ANALYSIS RESULTS")
        print("-" * 40)
        print(f"MSE  : {mse_value:.4f}")
        print(f"PSNR : {psnr_value:.2f} dB")
        print("-" * 40)

        # Visualization
        plt.figure(figsize=(12, 6)) 

        # Original image
        plt.subplot(1, 3, 1)
        plt.title("Original (Grayscale)")
        plt.imshow(img_orig_gray, cmap='gray')
        plt.axis('off')

        # Compressed image
        plt.subplot(1, 3, 2)
        plt.title("Compressed JPEG")
        plt.imshow(img_comp, cmap='gray')
        plt.axis('off')

        # Difference
        diff_img = np.abs(np.asarray(img_orig_gray, dtype=float) - np.asarray(img_comp, dtype=float))
        plt.subplot(1, 3, 3)
        plt.title("Difference Map")
        plt.imshow(diff_img, cmap='jet') 
        plt.colorbar(fraction=0.046, pad=0.04, label='Absolute Error')
        plt.axis('off')

        # Add MSE and PSNR to image
        results_text = f"Analysis Results:\nMSE = {mse_value:.4f}  |  PSNR = {psnr_value:.2f} dB"
        plt.figtext(0.5, 0.08, results_text, ha="center", fontsize=14, 
                    bbox={"facecolor":"orange", "alpha":0.2, "pad":5})

        # Adjust bottom margin to make space for text
        plt.subplots_adjust(bottom=0.2)
        
        # Save and Show
        print(f"Saving plot to '{args.output}'...")
        plt.savefig(args.output, dpi=300)
        
        print("Displaying plots...")
        plt.show()

    except Exception as e:
        print(f"An unexpected error occurred: {e}")

if __name__ == "__main__":
    main()