#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include "/home/labuser/posits/positenc.h"
#include "/home/labuser/posits/posit_trig56.h"

extern uint32_t rv32i_sin_pi56(uint32_t x);
extern uint32_t rv32i_cos_pi56(uint32_t x);

static int test_one(uint32_t px){
    double xd = posit_to_double(px);
    uint32_t s_rv  = rv32i_sin_pi56(px);
    uint32_t c_rv  = rv32i_cos_pi56(px);
    /* use double math as reference */
    uint32_t s_ref = posit_from_double(sin(M_PI * xd));
    uint32_t c_ref = posit_from_double(cos(M_PI * xd));
    /* allow exact zero vs tiny residual */
    int s_ok = (s_rv == s_ref) || (s_rv == 0 && posit_to_double(s_ref) < 1e-10 && posit_to_double(s_ref) > -1e-10);
    int c_ok = (c_rv == c_ref) || (c_rv == 0 && posit_to_double(c_ref) < 1e-10 && posit_to_double(c_ref) > -1e-10);
    return s_ok && c_ok;
}

int main(void){
    positenv_init(32,2);

    /* spot check named angles */
    printf("Named angles:\n");
    double named[] = {0.0, 0.1, 0.25, 0.333, 0.5, 1.0, 1.5, 2.0,
                      2.5, 3.0, 4.7, 7.3, 12.1, 100.0, 1234.5};
    int n = sizeof(named)/sizeof(named[0]);
    int pass=0, fail=0;
    for(int i=0; i<n; i++){
        uint32_t px = posit_from_double(named[i]);
        double xd = posit_to_double(px);
        uint32_t s_rv = rv32i_sin_pi56(px);
        uint32_t c_rv = rv32i_cos_pi56(px);
        uint32_t s_ref = posit_from_double(sin(M_PI*xd));
        uint32_t c_ref = posit_from_double(cos(M_PI*xd));
        int s_ok = (s_rv==s_ref)||(s_rv==0&&fabs(posit_to_double(s_ref))<1e-10);
        int c_ok = (c_rv==c_ref)||(c_rv==0&&fabs(posit_to_double(c_ref))<1e-10);
        printf("  x=%-8.3f sin=%s(%.8f) cos=%s(%.8f)\n",
               named[i],
               s_ok?"OK  ":"FAIL", posit_to_double(s_rv),
               c_ok?"OK  ":"FAIL", posit_to_double(c_rv));
        if(s_ok&&c_ok) pass++; else fail++;
    }
    printf("Named: %d pass %d fail\n\n", pass, fail);

    /* random sample -- 10000 inputs */
    printf("Random sample (10000 inputs):\n");
    pass=0; fail=0;
    uint32_t p = 1;
    for(int i=0; i<10000; i++){
        /* step through posit32 space */
        p += 214748;  /* ~10000 steps across 2^31 */
        if((int32_t)p < 0) continue;
        if(!test_one(p)) {
            fail++;
            if(fail <= 3)
                printf("  FAIL p=0x%08x x=%.6f\n", p, posit_to_double(p));
        } else pass++;
    }
    printf("Random: %d pass %d fail\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
