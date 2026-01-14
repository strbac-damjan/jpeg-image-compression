#ifdef __C7000__
#include "jpeg_compression.h"
#include <c7x.h> 

/* -------------------------------------------------------------------------------------
 * HELPER FUNCTIONS 
 * -------------------------------------------------------------------------------------
 */

static inline uint8_t getBitLength(int16_t val) 
{
    if (val == 0) return 0;
    
    /* * __abs(x) -> Absolute value
     * __norm(x) -> Count Leading Zeros/Ones).
     */
    int32_t v32 = (int32_t)__abs(val);
    return (uint8_t)(32 - __norm(v32) - 1);
}

static inline uint16_t getAmplitudeCode(int16_t val) 
{
    /*
     * If val < 0, (val >> 15) then -1 (sve jedinice: 0xFFFF).
     * If val > 0, (val >> 15) then 0  (0x0000).
     * result = val + (val >> 15).
     */
    int16_t mask = val >> 15; 
    return (uint16_t)(val + mask);
}

/**
 * \brief Encodes a MACROBLOCK (4 blocks of 8x8) using RLE.
 * * \param macro_zigzag_buffer  Pointer to 256 int16_t elements (4 blocks contiguous).
 * \param rle_out              Pointer to output buffer.
 * \param max_capacity         Max remaining space in output buffer.
 * \param last_dc_ptr          Pointer to global Last DC value (updated sequentially).
 * \return                     Total symbols written for all 4 blocks (or -1 on error).
 */
int32_t performRLEBlock4x8x8(const int16_t * __restrict macro_zigzag_buffer, 
                             RLESymbol * __restrict rle_out, 
                             int32_t max_capacity, 
                             int16_t *last_dc_ptr)
{
    int32_t total_symbols = 0;
    int blk, i, k;
    
    /* Privremeni buffer za indekse ne-nula elemenata.
     * Max 64 elementa (najgori slucaj: niko nije nula).
     * Smjestamo ga na stack (L1 memorija).
     */
    int16_t nz_indices[64]; 
    int nz_count;

    const int16_t *current_block_ptr;
    
    /* Loop over 4 blocks */
    for (blk = 0; blk < 4; blk++)
    {
        current_block_ptr = macro_zigzag_buffer + (blk * 64);
        
        /* -----------------------------------------------------------
         * DC COEFF (Uvijek obradjujemo posebno)
         * -----------------------------------------------------------
         */
        int16_t currentDC = current_block_ptr[0];
        int16_t diff      = currentDC - *last_dc_ptr;
        *last_dc_ptr      = currentDC;

        if (total_symbols >= max_capacity) return -1;
        
        // DC upis
        uint8_t dcSize = getBitLength(diff);
        RLESymbol *sym = &rle_out[total_symbols++];
        sym->symbol   = dcSize;
        sym->code     = getAmplitudeCode(diff);
        sym->codeBits = dcSize;

        /* -----------------------------------------------------------
         * PASS 1: SCAN & COLLECT INDICES (The Speedup)
         * Ovu petlju kompajler OBOZAVA. Jednostavna je, nema break-a,
         * nema while-a. C7000 ce ovdje koristiti vektorske instrukcije
         * da skenira 64 elementa veoma brzo.
         * -----------------------------------------------------------
         */
        nz_count = 0;
        
        // Hint: petlja se vrti max 63 puta.
        #pragma MUST_ITERATE(0, 63) 
        for (k = 1; k < 64; k++) {
            if (current_block_ptr[k] != 0) {
                nz_indices[nz_count++] = k;
            }
        }

        /* -----------------------------------------------------------
         * PASS 2: GENERATE SYMBOLS FROM INDICES
         * Sada ne iteriramo 63 puta, vec samo onoliko puta koliko
         * ima ne-nula elemenata (npr. 5-10 puta).
         * -----------------------------------------------------------
         */
        int last_k = 0; // Pozicija prethodnog ne-nula elementa (ili DC-a)

        // Iteriramo kroz pronadjene indekse
        for (i = 0; i < nz_count; i++) 
        {
            int curr_k = nz_indices[i];
            int16_t val = current_block_ptr[curr_k];
            
            // Broj nula izmedju trenutnog i prethodnog
            int zero_run = curr_k - last_k - 1;

            // Handle ZRL (runs >= 16)
            // Posto je ovo rijetko, 'if' je bolji od 'while' ovdje,
            // ali matematicki pristup je najbrzi (bez grananja).
            if (zero_run >= 16) {
                int num_zrl = zero_run >> 4; // podijeli sa 16
                zero_run    = zero_run & 0xF; // ostatak (modulo 16)

                // Emituj ZRL simbole (0xF0)
                while (num_zrl > 0) {
                   if (total_symbols >= max_capacity) return -1;
                   RLESymbol *z = &rle_out[total_symbols++];
                   z->symbol   = 0xF0;
                   z->code     = 0;
                   z->codeBits = 0;
                   num_zrl--;
                }
            }
            
            // Emituj normalan AC simbol
            if (total_symbols >= max_capacity) return -1;
            
            uint8_t size = getBitLength(val);
            RLESymbol *ac = &rle_out[total_symbols++];
            
            ac->symbol   = (uint8_t)((zero_run << 4) | size);
            ac->code     = getAmplitudeCode(val);
            ac->codeBits = size;

            // Azuriraj poziciju za sledeci korak
            last_k = curr_k;
        }

        /* -----------------------------------------------------------
         * EOB (End of Block)
         * Ako je zadnji indeks manji od 63, znaci da su ostalo nule.
         * -----------------------------------------------------------
         */
        if (last_k < 63) {
            if (total_symbols >= max_capacity) return -1;
            RLESymbol *eob = &rle_out[total_symbols++];
            eob->symbol   = 0x00;
            eob->code     = 0;
            eob->codeBits = 0;
        }
    }

    return total_symbols;
}
#endif
