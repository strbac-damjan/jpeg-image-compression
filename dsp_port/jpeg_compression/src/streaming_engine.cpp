#ifdef __C7000__

#include "jpeg_compression.h"
#include <c7x.h>
#include <c7x_scalable.h>


extern "C" void setupStreamingEngine(uint8_t* r_vec, uint8_t* gb_vec, uint64_t image_length) {
    
    __SE_TEMPLATE_v1 r_se = __gen_SE_TEMPLATE_v1();

    r_se.ELETYPE = __SE_ELETYPE_8BIT;
    r_se.VECLEN  = __SE_VECLEN_32ELEMS; 
    r_se.ICNT0   = image_length;               
    r_se.DIM1    = 0;
    r_se.ICNT1   = 0;
    r_se.DIM2    = 0;
    r_se.ICNT2   = 0;

    
    __SE_TEMPLATE_v1 gb_se = __gen_SE_TEMPLATE_v1();
    gb_se.ELETYPE = __SE_ELETYPE_8BIT;
    gb_se.VECLEN  = __SE_VECLEN_64ELEMS; 
    gb_se.ICNT0   = image_length * 2;           
    gb_se.DIM1    = 0;
    gb_se.ICNT1   = 0;
    gb_se.DIM2    = 0;
    gb_se.ICNT2   = 0;


    __SE0_OPEN((void*)r_vec, r_se);
    __SE1_OPEN((void*)gb_vec, gb_se);               
}


extern "C" void getNextHalfBlock(short32* r_output, short32* g_output, short32* b_output) {
    

    // Fetch the R components from the first one
    uchar32 r_input = c7x::strm_eng<0, uchar32>::get_adv();
    uchar64 gb_input = c7x::strm_eng<1, uchar64>::get_adv();

    // Upcasting
    *r_output = __convert_short32(r_input);
    *g_output = __convert_short32(gb_input.lo);
    *b_output = __convert_short32(gb_input.hi);
}

extern "C" void closeStreamingEngine() {
    __SE0_CLOSE();
    __SE1_CLOSE();
}

#endif
