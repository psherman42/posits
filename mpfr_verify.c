#include <stdio.h>
#include <mpfr.h>
#include "/home/labuser/posits/posit_trig56.h"

int main(int argc, char **argv){
    positenv_init(32,2);
    int is_cos = (argc > 1 && argv[1][0]=='c');
    
    mpfr_t x, y, pi;
    mpfr_init2(x, 200);
    mpfr_init2(y, 200);
    mpfr_init2(pi, 200);
    mpfr_const_pi(pi, MPFR_RNDN);
    
    uint32_t p;
    int genuine=0, artifact=0;
    while(scanf("%x", &p) == 1){
        double ad = posit_to_double(p);
        mpfr_set_d(x, ad, MPFR_RNDN);
        mpfr_mul(x, x, pi, MPFR_RNDN);
        if(is_cos) mpfr_cos(y, x, MPFR_RNDN);
        else       mpfr_sin(y, x, MPFR_RNDN);
        double result = mpfr_get_d(y, MPFR_RNDN);
        uint32_t correct = posit_from_double(result);
        uint32_t ours = is_cos ? posit32_cos_pi56(p) : posit32_sin_pi56(p);
        if(correct != ours){
            printf("GENUINE p=0x%08x correct=0x%08x ours=0x%08x\n",
                   p, correct, ours);
            genuine++;
        } else {
            artifact++;
        }
    }
    fprintf(stderr, "genuine=%d artifact=%d\n", genuine, artifact);
    mpfr_clear(x); mpfr_clear(y); mpfr_clear(pi);
    return genuine > 0 ? 1 : 0;
}
