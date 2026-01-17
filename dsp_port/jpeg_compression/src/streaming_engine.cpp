#ifdef __C7000__

#include "jpeg_compression.h"
#include <c7x.h>
#include <c7x_scalable.h>


extern "C" void setupStreamingEngine(uint8_t* r_vec, uint8_t* gb_vec, uint64_t image_length) {
    
    // Streaming Engine 0: R channel 
    __SE_TEMPLATE_v1 r_se = __gen_SE_TEMPLATE_v1();

    r_se.ELETYPE = __SE_ELETYPE_8BIT;
    r_se.VECLEN  = __SE_VECLEN_32ELEMS;      // Produces uchar32 vectors
    r_se.ICNT0   = image_length;             // Total number of R samples          
    r_se.DIM1    = 0;
    r_se.ICNT1   = 0;
    r_se.DIM2    = 0;
    r_se.ICNT2   = 0;

    // Streaming Engine 1: interleaved G+B channels 
    __SE_TEMPLATE_v1 gb_se = __gen_SE_TEMPLATE_v1();
    gb_se.ELETYPE = __SE_ELETYPE_8BIT;  
    gb_se.VECLEN  = __SE_VECLEN_64ELEMS;    // Produces uchar64 vectors
    gb_se.ICNT0   = image_length * 2;       // G and B for each pixel
    gb_se.DIM1    = 0;
    gb_se.ICNT1   = 0;
    gb_se.DIM2    = 0;
    gb_se.ICNT2   = 0;

    // Open streaming engines 
    __SE0_OPEN((void*)r_vec, r_se);
    __SE1_OPEN((void*)gb_vec, gb_se);               
}


extern "C" void getNextHalfBlock(short32* r_output, short32* g_output, short32* b_output) {
    

    // Fetch next vectors and advance stream position 
    uchar32 r_input = c7x::strm_eng<0, uchar32>::get_adv();
    uchar64 gb_input = c7x::strm_eng<1, uchar64>::get_adv();

    // Convert 8-bit samples to 16-bit for processing 
    *r_output = __convert_short32(r_input);
    *g_output = __convert_short32(gb_input.lo);
    *b_output = __convert_short32(gb_input.hi);
}

extern "C" void closeStreamingEngine() {
    __SE0_CLOSE();
    __SE1_CLOSE();
}

#endif
