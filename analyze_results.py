import argparse
import os
import sys
import math
import numpy as np
import matplotlib.pyplot as plt
from PIL import Image

# Pokusaj importa scikit-image za SSIM
try:
    from skimage.metrics import structural_similarity as ssim
except ImportError:
    print("Greska: Nedostaje biblioteka 'scikit-image'.")
    print("Instalirajte je komandom: pip install scikit-image")
    sys.exit(1)

def calculate_mse(image1, image2):
    """Calculates the Mean Squared Error (MSE)."""
    img1_data = np.asarray(image1, dtype=np.float64)
    img2_data = np.asarray(image2, dtype=np.float64)
    diff = img1_data - img2_data
    sqr_diff = diff ** 2
    mse = np.mean(sqr_diff)
    return mse

def calculate_psnr(mse):
    """Calculates Peak Signal-to-Noise Ratio (PSNR)."""
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

    # --- 1. PROVERA FAJLOVA ---
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

        # Convert to Grayscale ('L')
        img_orig_gray = img_orig.convert('L')
        img_comp_gray = img_comp.convert('L')
        
        # Resize if dimensions differ
        if img_orig_gray.size != img_comp_gray.size:
            print(f"Warning: Dimensions do not match! Resizing original to match compressed.")
            img_orig_gray = img_orig_gray.resize(img_comp_gray.size)

        # --- 2. FILE SIZE METRICS ---
        file_size_orig = os.path.getsize(args.original_path)
        file_size_comp = os.path.getsize(args.compressed_path)
        
        if file_size_comp > 0:
            compression_ratio = file_size_orig / file_size_comp
        else:
            compression_ratio = 0

        width, height = img_orig_gray.size
        total_pixels = width * height
        bpp = (file_size_comp * 8) / total_pixels

        # --- 3. QUALITY METRICS ---
        mse_value = calculate_mse(img_orig_gray, img_comp_gray)
        psnr_value = calculate_psnr(mse_value)

        img_orig_np = np.asarray(img_orig_gray)
        img_comp_np = np.asarray(img_comp_gray)
        ssim_value = ssim(img_orig_np, img_comp_np, data_range=255)

        # --- 4. CONSOLE OUTPUT ---
        print("-" * 50)
        print(f"ANALYSIS RESULTS")
        print("-" * 50)
        print(f"File Size Orig : {file_size_orig} bytes")
        print(f"File Size Comp : {file_size_comp} bytes")
        print(f"Comp. Ratio    : {compression_ratio:.2f} : 1")
        print(f"Bits Per Pixel : {bpp:.3f} bpp")
        print("-" * 50)
        print(f"MSE            : {mse_value:.4f}")
        print(f"PSNR           : {psnr_value:.2f} dB")
        print(f"SSIM           : {ssim_value:.4f}")
        print("-" * 50)

        # --- 5. VISUALIZATION ---
        plt.figure(figsize=(15, 6)) # Malo sire da stane tekst u jedan red

        # Original image
        plt.subplot(1, 3, 1)
        plt.title("Original")
        plt.imshow(img_orig_gray, cmap='gray')
        plt.axis('off')

        # Compressed image
        plt.subplot(1, 3, 2)
        plt.title(f"Compressed JPEG")
        plt.imshow(img_comp_gray, cmap='gray')
        plt.axis('off')

        # Difference Map
        diff_img = np.abs(np.asarray(img_orig_gray, dtype=float) - np.asarray(img_comp_gray, dtype=float))
        plt.subplot(1, 3, 3)
        plt.title("Difference Map")
        plt.imshow(diff_img, cmap='jet') 
        plt.colorbar(fraction=0.046, pad=0.04, label='Abs Error')
        plt.axis('off')

        # --- Single Line Results ---
        results_text = (
            f"MSE: {mse_value:.2f}  |  PSNR: {psnr_value:.2f} dB  |  SSIM: {ssim_value:.4f}  |  "
            f"CR: {compression_ratio:.1f}:1  |  BPP: {bpp:.3f}"
        )
        
        # Position text centered at bottom with less gap
        # bottom=0.12 means plots take up top 88%, leaving 12% at bottom
        # y=0.04 places text comfortably inside that 12%
        plt.figtext(0.5, 0.04, results_text, ha="center", fontsize=13, 
                    bbox={"facecolor":"#f0f0f0", "edgecolor":"gray", "alpha":0.8, "pad":8})

        # Adjust layout to reduce gap
        plt.subplots_adjust(bottom=0.12, top=0.92, wspace=0.2)
        
        # Save and Show
        print(f"Saving plot to '{args.output}'...")
        plt.savefig(args.output, dpi=300, bbox_inches='tight')

    except Exception as e:
        print(f"An unexpected error occurred: {e}")
        import traceback
        traceback.print_exc()

if __name__ == "__main__":
    main()