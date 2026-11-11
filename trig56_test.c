#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "posit_trig56.h"

int main(void) {
    positenv_init(32,2);

    printf("posit_trig56.h -- 56-bit CORDIC\n");
    printf("Beyond posit32 28-bit significand precision.\n\n");

    printf("sin56 test:\n");
    double angles[] = {0, M_PI/6, M_PI/4, M_PI/3, M_PI/2,
                       0.1, 1.0, 1.5, 2.0, 2.5, 3.0};
    int perfect=0, off1=0, off2=0, worse=0;
    for (int i=0;i<11;i++){
        uint32_t pa = posit_from_double(angles[i]);
        uint32_t ps = posit32_sin56(pa);
        double rs = posit_to_double(ps);
        double ex = sin(angles[i]);
        int32_t ulp = sin56_ulp(pa);
        printf("  sin(%.4f)=%.8f (exp %.8f) ulp=%d %s\n",
               angles[i],rs,ex,ulp,ulp==0?"PASS":"check");
        if(ulp==0) perfect++;
        else if(abs(ulp)==1) off1++;
        else off2++;
    }

    printf("\ncos56 test:\n");
    for (int i=0;i<11;i++){
        uint32_t pa = posit_from_double(angles[i]);
        uint32_t pc = posit32_cos56(pa);
        double rc = posit_to_double(pc);
        double ex = cos(angles[i]);
        int32_t ulp = cos56_ulp(pa);
        printf("  cos(%.4f)=%.8f (exp %.8f) ulp=%d %s\n",
               angles[i],rc,ex,ulp,ulp==0?"PASS":"check");
        if(ulp==0) perfect++;
        else if(abs(ulp)==1) off1++;
        else off2++;
    }

    printf("\nsin_pi56 test (Gustafson recommendation):\n");
    double xs[] = {0.0,0.5,0.25,0.1,0.333333,1.0};
    for(int i=0;i<6;i++){
        uint32_t px=posit_from_double(xs[i]);
        uint32_t ps=posit32_sin_pi56(px);
        double rs=posit_to_double(ps);
        double ex=sin(M_PI*xs[i]);
        printf("  sin_pi(%.4f)=%.8f (exp %.8f) err=%.2e\n",
               xs[i],rs,ex,fabs(rs-ex));
    }

    printf("\ncos_pi56 test (Gustafson recommendation):\n");
    double xc[] = {0.0,0.5,0.25,0.1,0.333333,1.0};
    for(int i=0;i<6;i++){
        uint32_t px=posit_from_double(xc[i]);
        uint32_t pc=posit32_cos_pi56(px);
        double rc=posit_to_double(pc);
        double ex=cos(M_PI*xc[i]);
        double ref = (fabs(ex) < 1e-15) ? 0.0 : ex;
        int32_t ulp=(int32_t)pc-(int32_t)posit_from_double(ref);
        printf("  cos_pi(%.4f)=%.8f (exp %.8f) err=%.2e ulp=%d %s\n",
               xc[i],rc,ex,fabs(rc-ex),ulp,ulp==0?"PASS":"check");
    }

    printf("\nULP summary (%d tests):\n",22);
    printf("  perfect (ulp=0): %d\n",perfect);
    printf("  off by 1 ulp:    %d\n",off1);
    printf("  worse:           %d\n",off2);

    /* spot check 10000 inputs */
    printf("\nSpot check 10000 posit32 inputs:\n");
    perfect=0;off1=0;off2=0;worse=0;
    for(uint32_t p=0x10000000;p<0x70000000;p+=0x00006000){
        int32_t ulp=sin56_ulp(p);
        if(ulp==0) perfect++;
        else if(abs(ulp)==1) off1++;
        else if(abs(ulp)==2) off2++;
        else worse++;
    }
    int total=perfect+off1+off2+worse;
    printf("  tested: %d  perfect: %d  off1: %d  off2: %d  worse: %d\n",
           total,perfect,off1,off2,worse);
    printf("  correctly rounded: %.1f%%\n",100.0*perfect/total);

    printf("\nSample of bad inputs:\n");
    int shown=0;
    for(uint32_t p=0x10000000;p<0x70000000;p+=0x00006000){
        int32_t u=sin56_ulp(p);
        if(abs(u)>=10 && shown<10){
            double ad=posit_to_double(p);
            double got=posit_to_double(posit32_sin56(p));
            double exp=sin(ad);
            printf("  p=0x%08x a=%.6f got=%.6f exp=%.6f ulp=%d\n",
                   p,ad,got,exp,u);
            shown++;
        }
    }

    printf("\nULP histogram (spot check):\n");
    int hist[10]={0};
    for(uint32_t p=0x10000000;p<0x70000000;p+=0x00006000){
        int32_t u=sin56_ulp(p);
        int au=abs(u);
        if(au<10) hist[au]++;
        else hist[9]++;
    }
    for(int i=0;i<9;i++) printf("  ulp=%d: %d\n",i,hist[i]);
    printf("  ulp>=10: %d\n",hist[9]);

    printf("\nNext: exhaustive 2^32 check on Pioneer 64 harts.\n");
    printf("Then: ask Gustafson for Table-Maker's Dilemma strategies.\n");
    return 0;
}
