#include <stdio.h>
#include "bposit_quire.h"

int main(void) {
    bquire_t q; int i; double r;

    printf("B-posit quire parameters (Warsy et al. CoNGA 2026):\n");
    printf("  n=%d es=%d rs=%d\n", BPOSIT_N, BPOSIT_ES, BPOSIT_RS);
    printf("  E=%d F=%d Qraw=%d Qsize=%d bits (%d words)\n",
           BPOSIT_E, BPOSIT_F, BPOSIT_QRAW, BPOSIT_QSIZE, BPOSIT_QWORDS);
    printf("  vs standard quire: 512 bits (16 words)\n");
    printf("  reduction: %d%%\n\n",
           100 - (BPOSIT_QWORDS*32*100)/512);

    bquire_clear(&q);
    for(i=1;i<=10;i++) bquire_add(&q,(double)i);
    r=bquire_round(&q);
    printf("sum 1..10        = %8.1f (exp  55.0) %s\n",
           r, r==55.0?"PASS":"FAIL");

    {double a[]={1,2,3,4},b[]={4,3,2,1};
    bquire_clear(&q);
    for(i=0;i<4;i++) bquire_fma(&q,a[i],b[i]);
    r=bquire_round(&q);
    printf("dot [1234][4321] = %8.1f (exp  20.0) %s\n",
           r, r==20.0?"PASS":"FAIL");}

    bquire_clear(&q);
    for(i=1;i<=10;i++) bquire_add(&q,(double)i);
    for(i=1;i<=5;i++)  bquire_add(&q,-(double)i);
    r=bquire_round(&q);
    printf("sum1..10-1..5    = %8.1f (exp  40.0) %s\n",
           r, r==40.0?"PASS":"FAIL");

    {double M=1e15,m=1e-15;
    bquire_clear(&q);
    bquire_add(&q,M); bquire_add(&q,m); bquire_add(&q,-M);
    r=bquire_round(&q);
    printf("Gustafson test   = %.3e (exp %.3e) %s\n",
           r,m,fabs(r-m)<m*0.001?"PASS":"FAIL");
    printf("  Kahan gives:     %.3e\n",(M+m)-M);}

    bquire_clear(&q);
    for(i=1;i<=1000;i++) bquire_add(&q,1.0);
    r=bquire_round(&q);
    printf("sum 1000x1.0     = %8.1f (exp1000.0) %s\n",
           r, r==1000.0?"PASS":"FAIL");

    printf("\nclz32(0x80000000) = %d (exp 0)\n", bposit_clz32(0x80000000));
    printf("clz32(0x00000001) = %d (exp 31)\n", bposit_clz32(0x00000001));
    printf("clz32(0x00000000) = %d (exp 32)\n", bposit_clz32(0x00000000));

    return 0;
}
