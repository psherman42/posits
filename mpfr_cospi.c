#include <stdio.h>
#include <mpfr.h>
#include "/home/labuser/posits/posit_trig56.h"

int main(void){
    positenv_init(32,2);
    
    /* read hard case inputs from stdin */
    uint32_t p;
    mpfr_t x, y, pi;
    mpfr_init2(x, 200);
    mpfr_init2(y, 200);
    mpfr_init2(pi, 200);
    mpfr_const_pi(pi, MPFR_RNDN);
    
    int fixed=0, ok=0, total=0;
    printf("static const uint32_t _cos_pi_hard[][2] = {\n");
    
    while(scanf("%x", &p) == 1){
        double ad = posit_to_double(p);
        mpfr_set_d(x, ad, MPFR_RNDN);
        mpfr_mul(x, x, pi, MPFR_RNDN);
        mpfr_cos(y, x, MPFR_RNDN);
        double result = mpfr_get_d(y, MPFR_RNDN);
        uint32_t correct = posit_from_double(result);
        uint32_t ours    = posit32_cos_pi56(p);
        const char *status = (correct==ours) ? "OK" : "FIXED";
        if(correct!=ours) fixed++; else ok++;
        printf("    { 0x%08x, 0x%08x },  /* a=%.15e %s */\n",
               p, correct, ad, status);
        total++;
    }
    printf("};\n");
    printf("#define N_COS_PI_HARD %d\n", total);
    fprintf(stderr, "total=%d fixed=%d ok=%d\n", total, fixed, ok);
    
    mpfr_clear(x); mpfr_clear(y); mpfr_clear(pi);
    return 0;
}
