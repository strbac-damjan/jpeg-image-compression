#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include <TI/tivx.h>
#include <utils/ipc/include/app_ipc.h>
#include <utils/remote_service/include/app_remote_service.h>
#include <utils/app_init/include/app_init.h>
#include <utils/console_io/include/app_log.h>
#include <utils/mem/include/app_mem.h>

#include <jpeg_compression.h>
#include "bmp_handler.h"
#include "jpeg_handler.h"

// RLE symbol structure
// Must exactly match the definition on the DSP side
typedef struct
{
    uint8_t symbol;
    uint8_t codeBits;
    uint16_t code;
} RLESymbol;

// Data Transfer Object shared between A72 host and C7x DSP
// Contains input pointers, output pointers, and profiling data
typedef struct JPEG_COMPRESSION_DTO
{
    int32_t width;
    int32_t height;

    // Input RGB buffers (physical addresses)
    uint64_t r_phy_ptr;
    uint64_t gb_phy_ptr;

    // Intermediate buffers for debugging and profiling
    uint64_t y_phy_ptr;
    uint64_t dct_phy_ptr;
    uint64_t quant_phy_ptr;
    uint64_t zigzag_phy_ptr;

    // RLE output buffer
    uint64_t rle_phy_ptr;
    uint32_t rle_count;

    // Final Huffman bitstream output
    uint64_t huff_phy_ptr;
    uint32_t huff_size;

    // Profiling cycle counters
    uint64_t cycles_color_conversion;
    uint64_t cycles_dct;
    uint64_t cycles_quantization;
    uint64_t cycles_zigzag;
    uint64_t cycles_rle;
    uint64_t cycles_huffman;
    uint64_t cycles_total;

} JPEG_COMPRESSION_DTO;

// Aligns a value to the next multiple of 32
// Used for macro block width alignment
int32_t align32(int32_t x)
{
    return (x + 31) & ~31;
}

// Aligns a value to the next multiple of 8
// Used for block height alignment
int32_t align8(int32_t x)
{
    return (x + 7) & ~7;
}

// Formats a 64-bit cycle count with spaces for readability
void format_cycles(uint64_t num, char *out_buf)
{
    char temp[32];
    sprintf(temp, "%llu", (unsigned long long)num);

    int len = strlen(temp);
    int i, j = 0;

    for (i = 0; i < len; i++)
    {
        out_buf[j++] = temp[i];
        if ((len - 1 - i) > 0 && (len - 1 - i) % 3 == 0)
        {
            out_buf[j++] = ' ';
        }
    }
    out_buf[j] = '\0';
}

void printFirstBlock(BMPImage *img)
{
    int w = img->width; // Treba nam širina slike da bi znali kad počinje novi red u memoriji
    printf("---------------------------------------");
        // --- R KOMPONENTA ---
        printf("--- R Component (First 8x8 Block) ---\n");
    for (int y = 0; y < 8; y++)
    {
        for (int x = 0; x < 8; x++)
        {
            // Indeks u linearnom nizu je: (trenutni_red * ukupna_širina) + trenutna_kolona
            printf("%3d ", img->r[y * w + x]);
        }
        printf("\n"); // Novi red nakon svakih 8 piksela
    }

    // --- G KOMPONENTA ---
    printf("\n--- G Component (First 8x8 Block) ---\n");
    for (int y = 0; y < 8; y++)
    {
        for (int x = 0; x < 8; x++)
        {
            printf("%3d ", img->g[y * w + x]);
        }
        printf("\n");
    }

    // --- B KOMPONENTA ---
    printf("\n--- B Component (First 8x8 Block) ---\n");
    for (int y = 0; y < 8; y++)
    {
        for (int x = 0; x < 8; x++)
        {
            printf("%3d ", img->b[y * w + x]);
        }
        printf("\n");
    }
    printf("---------------------------------------");
}

// Prints profiling statistics returned from the DSP
void print_profiling_stats(JPEG_COMPRESSION_DTO *dto)
{
    char buf[32];

    printf("==========================================\n");
    printf("   DSP C7x PROFILING REPORT (Cycles)      \n");
    printf("==========================================\n");
    printf("Image Resolution : %dx%d\n", dto->width, dto->height);
    printf("------------------------------------------\n");
    printf("Step               Cycles                 \n");
    printf("------------------------------------------\n");

    format_cycles(dto->cycles_color_conversion, buf);
    printf("Color Conversion : %15s\n", buf);

    format_cycles(dto->cycles_dct, buf);
    printf("DCT              : %15s\n", buf);

    format_cycles(dto->cycles_quantization, buf);
    printf("Quantization     : %15s\n", buf);

    format_cycles(dto->cycles_zigzag, buf);
    printf("ZigZag           : %15s\n", buf);

    format_cycles(dto->cycles_rle, buf);
    printf("RLE              : %15s\n", buf);

    format_cycles(dto->cycles_huffman, buf);
    printf("Huffman          : %15s\n", buf);

    printf("------------------------------------------\n");

    format_cycles(dto->cycles_total, buf);
    printf("TOTAL CYCLES     : %15s\n", buf);
    printf("==========================================\n\n");
}

// Prints an 8x8 debug block for inspection
// Type selects the data format
void print_debug_block(const char *name, void *data, int type)
{
    printf("\n--- DEBUG BLOCK: %s ---\n", name);

    for (int i = 0; i < 8; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            int idx = i * 8 + j;

            if (type == 0)
            {
                printf("%3d ", ((uint8_t *)data)[idx]);
            }
            else if (type == 1)
            {
                printf("%6.1f ", ((float *)data)[idx]);
            }
            else if (type == 2)
            {
                printf("%4d ", ((int16_t *)data)[idx]);
            }
        }
        printf("\n");
    }

    printf("--------------------------\n");
}

// Helper function to print usage instructions
void print_usage(const char *prog_name)
{
    appLogPrintf("Usage: %s --input_path <path_to_bmp> --output_path <path_to_jpg>\n", prog_name);
}

/**
 * Popunjava pre-alocirane buffere podacima iz BMP slike.
 * dst_r: mora biti velicine (blocks_w * blocks_h * 64) bajtova
 * dst_gb: mora biti velicine (blocks_w * blocks_h * 64 * 2) bajtova
 */
void bmp_extract_linear_blocks(const BMPImage *img,
                               uint8_t *blk_r,
                               uint8_t *blk_g,
                               uint8_t *blk_b,
                               uint32_t *out_w,
                               uint32_t *out_h)
{

    uint32_t blocks_w = (img->width + 7) / 8;
    uint32_t blocks_h = (img->height + 7) / 8;

    *out_w = blocks_w;
    *out_h = blocks_h;

    uint32_t idx = 0; // Linearni indeks za izlazne buffere

    for (uint32_t by = 0; by < blocks_h; by++)
    {
        for (uint32_t bx = 0; bx < blocks_w; bx++)
        {

            // Unutar 8x8 bloka
            for (uint32_t y = 0; y < 8; y++)
            {
                for (uint32_t x = 0; x < 8; x++)
                {

                    // Računanje stvarne pozicije na slici
                    int32_t img_x = bx * 8 + x;
                    int32_t img_y = by * 8 + y;

                    // Clamping (ako smo van granica slike, uzmi zadnji piksel)
                    if (img_x >= img->width)
                        img_x = img->width - 1;
                    if (img_y >= img->height)
                        img_y = img->height - 1;

                    // Indeks u originalnoj BMP slici
                    int32_t src_idx = img_y * img->width + img_x;

                    // Kopiranje u linearne blok buffere
                    blk_r[idx] = img->r[src_idx];
                    blk_g[idx] = img->g[src_idx];
                    blk_b[idx] = img->b[src_idx];

                    idx++;
                }
            }
        }
    }
}

// Funkcija 2: Formira finalne R i GB pointere iz pripremljenih blokova
// block_size_bytes obično odgovara ukupnom broju piksela (blocks_w * blocks_h * 64)
void interleave_gb_channels(const uint8_t *in_blk_r,
                            const uint8_t *in_blk_g,
                            const uint8_t *in_blk_b,
                            uint32_t total_pixels,
                            uint8_t *out_ptr_r,
                            uint8_t *out_ptr_gb)
{

    // 1. R kanal se samo kopira (jer je već linearizovan po blokovima u prvoj funkciji)
    // Možeš koristiti memcpy za maksimalnu brzinu
    memcpy(out_ptr_r, in_blk_r, total_pixels);

    // 2. G i B kanali se isprepliću (interleave)
    // Logika: 32 bajta G, pa 32 bajta B
    // i iterira kroz ulazne nizove (koji su iste dužine)
    // k iterira kroz izlazni gb niz (koji je duplo veći)

    for (uint32_t i = 0, k = 0; i < total_pixels; i += 32, k += 64)
    {
        for (int j = 0; j < 32; j++)
        {
            // Prvih 32 bajta u izlaznom bloku su G
            out_ptr_gb[k + j] = in_blk_g[i + j];

            // Narednih 32 bajta su B
            out_ptr_gb[k + j + 32] = in_blk_b[i + j];
        }
    }
}

void fill_planar_blocks(const BMPImage *img,
                        uint8_t *dst_r,
                        uint8_t *dst_gb,
                        uint32_t blocks_w,
                        uint32_t blocks_h)
{

    uint32_t total_pixels = blocks_w * blocks_h * 64;

    // 1. Alokacija privremenih buffera za linearne blokove
    // Ovi bufferi služe samo za tranziciju podataka
    uint8_t *temp_r = (uint8_t *)malloc(total_pixels);
    uint8_t *temp_g = (uint8_t *)malloc(total_pixels);
    uint8_t *temp_b = (uint8_t *)malloc(total_pixels);

    if (!temp_r || !temp_g || !temp_b)
    {
        printf("Error: Failed to allocate temp buffers in fill_planar_blocks\n");
        if (temp_r)
            free(temp_r);
        if (temp_g)
            free(temp_g);
        if (temp_b)
            free(temp_b);
        return;
    }

    // 2. Ekstrakcija podataka iz BMP-a u linearne 8x8 blokove
    uint32_t dummy_w, dummy_h;
    bmp_extract_linear_blocks(img, temp_r, temp_g, temp_b, &dummy_w, &dummy_h);

    // 3. Permutacija i spajanje u finalni format za DSP (R + Interleaved GB)
    interleave_gb_channels(temp_r, temp_g, temp_b, total_pixels, dst_r, dst_gb);

    // 4. Čišćenje
    free(temp_r);
    free(temp_g);
    free(temp_b);
}
int main(int argc, char *argv[])
{
    int32_t status;
    const char *inputPath = NULL;
    const char *outputPath = NULL;

    // --- 1. Parse Arguments ---
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--input_path") == 0)
        {
            if (i + 1 < argc)
                inputPath = argv[++i];
            else
            {
                appLogPrintf("Error: --input_path requires value.\n");
                return -1;
            }
        }
        else if (strcmp(argv[i], "--output_path") == 0)
        {
            if (i + 1 < argc)
                outputPath = argv[++i];
            else
            {
                appLogPrintf("Error: --output_path requires value.\n");
                return -1;
            }
        }
    }

    if (inputPath == NULL || outputPath == NULL)
    {
        appLogPrintf("Usage: %s --input_path <file> --output_path <file>\n", argv[0]);
        return -1;
    }

    // --- 2. Init App ---
    appLogPrintf("JPEG: Initializing App...\n");
    status = appInit();
    if (status != 0)
    {
        appLogPrintf("JPEG: App init failed!\n");
        return 1;
    }

    // --- 3. Load BMP ---
    appLogPrintf("JPEG: Loading BMP image form %s...\n", inputPath);
    BMPImage *img = loadBMPImage(inputPath);
    if (!img)
    {
        appLogPrintf("JPEG: Image loading failed!\n");
        appDeInit();
        return 1;
    }

    // --- 4. Compute Block Dimensions ---
    uint32_t blocks_w = (img->width + 7) / 8;
    uint32_t blocks_h = (img->height + 7) / 8;

    // Ukupan broj piksela poravnat na blokove (npr. 800x600 -> 800x600, ali 801x600 -> 808x600)
    uint32_t total_pixels_aligned = blocks_w * blocks_h * 64;

    appLogPrintf("JPEG: Loaded %dx%d -> Blocks %dx%d (Total Padded Pixels: %d)\n",
                 img->width, img->height, blocks_w, blocks_h, total_pixels_aligned);

    // --- 5. Allocate Input Buffers (Shared Memory DDR) ---

    // R kanal: 1 bajt po pikselu
    uint8_t *r_input_virt = (uint8_t *)appMemAlloc(APP_MEM_HEAP_DDR, total_pixels_aligned, 64);

    // GB kanal: 2 bajta po pikselu (spojeni G i B, interleaved)
    uint32_t gb_size_bytes = total_pixels_aligned * 2;
    uint8_t *gb_input_virt = (uint8_t *)appMemAlloc(APP_MEM_HEAP_DDR, gb_size_bytes, 64);

    if (!r_input_virt || !gb_input_virt)
    {
        appLogPrintf("JPEG: Failed to allocate input buffers!\n");
        freeBMPImage(img);
        appDeInit(); // Pretpostavljam da ovo čisti i preostale alokacije ako appMemAlloc fail-a
        return 1;
    }

    // --- 6. Convert BMP to Planar/Block Format ---
    // Koristimo wrapper funkciju definisanu iznad
    appLogPrintf("JPEG: Converting format and interleaving blocks...\n");
    fill_planar_blocks(img, r_input_virt, gb_input_virt, blocks_w, blocks_h);

    // --- 7. Flush Cache (CRITICAL) ---
    // Upisujemo pripremljene podatke iz L2/L3 cache-a u DDR da ih DSP vidi
    appMemCacheWb(r_input_virt, total_pixels_aligned);
    appMemCacheWb(gb_input_virt, gb_size_bytes);

    // --- 8. Allocate Output Buffers ---
    // Y (Luma) output
    uint8_t *y_output_virt = (uint8_t *)appMemAlloc(APP_MEM_HEAP_DDR, total_pixels_aligned, 64);

    // DCT coefficients (float)
    uint32_t dct_size_bytes = total_pixels_aligned * sizeof(float);
    float *dct_output_virt = (float *)appMemAlloc(APP_MEM_HEAP_DDR, dct_size_bytes, 64);

    // Quantized coefficients (int16)
    uint32_t quant_size_bytes = total_pixels_aligned * sizeof(int16_t);
    int16_t *quant_output_virt = (int16_t *)appMemAlloc(APP_MEM_HEAP_DDR, quant_size_bytes, 64);

    // ZigZag output (int16)
    int16_t *zigzag_output_virt = (int16_t *)appMemAlloc(APP_MEM_HEAP_DDR, quant_size_bytes, 64);

    // RLE output
    uint32_t rle_size_bytes = total_pixels_aligned * sizeof(RLESymbol); // Pretpostavka za sizeof(RLESymbol)
    RLESymbol *rle_output_virt = (RLESymbol *)appMemAlloc(APP_MEM_HEAP_DDR, rle_size_bytes, 64);

    // Huffman bitstream output
    uint32_t huff_capacity_bytes = total_pixels_aligned; // Worst case buffer size
    uint8_t *huff_output_virt = (uint8_t *)appMemAlloc(APP_MEM_HEAP_DDR, huff_capacity_bytes, 64);

    if (!y_output_virt || !dct_output_virt || !quant_output_virt ||
        !zigzag_output_virt || !rle_output_virt || !huff_output_virt)
    {
        appLogPrintf("JPEG: Failed to allocate output memory!\n");
        // Ovdje bi trebalo dodati free() za r_input i gb_input
        freeBMPImage(img);
        appDeInit();
        return 1;
    }

    // Inicijalizacija outputa na 0 (dobra praksa)
    memset(y_output_virt, 0, total_pixels_aligned);
    appMemCacheWb(y_output_virt, total_pixels_aligned);
    // Napomena: Za ostale output buffere writeback nije nužan jer ih samo čitamo,
    // ali Invalidate je obavezan poslije DSP-a.

    // --- 9. Prepare DTO ---
    JPEG_COMPRESSION_DTO dto;
    memset(&dto, 0, sizeof(dto));

    dto.width = blocks_w * 8;
    dto.height = blocks_h * 8;

    // -- Konverzija Virtualnih adresa u Fizičke (za DSP DMA) --
    dto.r_phy_ptr = appMemGetVirt2PhyBufPtr((uint64_t)r_input_virt, APP_MEM_HEAP_DDR);

    // BITNO: gb_phy_ptr pokazuje na početak interleavovanog buffera.
    // DSP kod mora znati da ovaj pointer sadrži i G i B podatke.
    dto.gb_phy_ptr = appMemGetVirt2PhyBufPtr((uint64_t)gb_input_virt, APP_MEM_HEAP_DDR);

    dto.y_phy_ptr = appMemGetVirt2PhyBufPtr((uint64_t)y_output_virt, APP_MEM_HEAP_DDR);
    dto.dct_phy_ptr = appMemGetVirt2PhyBufPtr((uint64_t)dct_output_virt, APP_MEM_HEAP_DDR);
    dto.quant_phy_ptr = appMemGetVirt2PhyBufPtr((uint64_t)quant_output_virt, APP_MEM_HEAP_DDR);
    dto.zigzag_phy_ptr = appMemGetVirt2PhyBufPtr((uint64_t)zigzag_output_virt, APP_MEM_HEAP_DDR);
    dto.rle_phy_ptr = appMemGetVirt2PhyBufPtr((uint64_t)rle_output_virt, APP_MEM_HEAP_DDR);
    dto.huff_phy_ptr = appMemGetVirt2PhyBufPtr((uint64_t)huff_output_virt, APP_MEM_HEAP_DDR);

    // --- 10. Run DSP Service ---
    appLogPrintf("JPEG: Sending data to DSP (Blocks: %dx%d)...\n", blocks_w, blocks_h);

    status = appRemoteServiceRun(
        APP_IPC_CPU_C7x_1,                 // Target Core
        "com.etfbl.sdos.jpeg_compression", // Service Name
        0,                                 // Command
        &dto,                              // Payload params
        sizeof(dto),                       // Payload size
        0                                  // Flags
    );

    if (status != 0)
    {
        appLogPrintf("JPEG: DSP Execution Failed! Status: %d\n", status);
    }
    else
    {

        // --- INVALIDATE CACHE (Da CPU pročita nove podatke iz DDR-a) ---
        // Moramo invalidirati cache za sve output buffere koje želimo čitati
        appMemCacheInv(y_output_virt, total_pixels_aligned);
        appMemCacheInv(huff_output_virt, dto.huff_size);
        // appMemCacheInv(dct_output_virt, dct_size_bytes); // Ako trebaš debug ispis DCT-a

        appLogPrintf("\n=== RESULT ===\n");
        appLogPrintf("JPEG: Final Huffman Size: %d bytes\n", dto.huff_size);

        // Snimanje kompresovanog fajla
        // Koristimo originalne dimenzije ili alignovane, zavisno od saveJPEG implementacije
        bool saved = saveJPEG(outputPath, dto.width, dto.height, huff_output_virt, dto.huff_size);

        if (saved)
        {
            appLogPrintf("SUCCESS: Saved compressed image to %s\n", outputPath);
        }
        else
        {
            appLogPrintf("ERROR: Failed to write output file!\n");
        }

        // Opcionalno: Debug ispis prvog bloka Y komponente
        // print_debug_block("Y component (first block)", y_output_virt, 0);

        // Opcionalno: Profiling stats iz DTO (ako ih DSP popunjava)
        print_profiling_stats(&dto);
    }

    // --- 11. Cleanup ---
    appLogPrintf("JPEG: Cleaning up memory...\n");

    // Input buffers
    appMemFree(APP_MEM_HEAP_DDR, r_input_virt, total_pixels_aligned);
    appMemFree(APP_MEM_HEAP_DDR, gb_input_virt, gb_size_bytes);

    // Output buffers
    appMemFree(APP_MEM_HEAP_DDR, y_output_virt, total_pixels_aligned);
    appMemFree(APP_MEM_HEAP_DDR, dct_output_virt, dct_size_bytes);
    appMemFree(APP_MEM_HEAP_DDR, quant_output_virt, quant_size_bytes);
    appMemFree(APP_MEM_HEAP_DDR, zigzag_output_virt, quant_size_bytes);
    appMemFree(APP_MEM_HEAP_DDR, rle_output_virt, rle_size_bytes);
    appMemFree(APP_MEM_HEAP_DDR, huff_output_virt, huff_capacity_bytes);

    freeBMPImage(img);
    appDeInit();

    return 0;
}