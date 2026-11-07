#include <stdio.h>
#include "quire32_int.h"

int main(void) {
    quire32i_t q; int i; double r;
    #define T(label,val,exp,eps) \
        printf("%-24s = %12.4f  (exp %10.4f)  %s\n", \
               label,(double)(val),(double)(exp), \
               fabs((double)(val)-(double)(exp))<(eps)?"PASS":"FAIL")

    quire32i_clear(&q);
    for(i=1;i<=10;i++) quire32i_addd(&q,(double)i);
    T("sum 1..10", quire32i_round(&q), 55.0, 0.001);

    {double a[]={1,2,3,4},b[]={4,3,2,1};
    quire32i_clear(&q);
    for(i=0;i<4;i++) quire32i_addd(&q,a[i]*b[i]);
    T("dot [1234][4321]", quire32i_round(&q), 20.0, 0.001);}

    quire32i_clear(&q);
    for(i=1;i<=10;i++) quire32i_addd(&q,(double)i);
    for(i=1;i<=5;i++)  quire32i_addd(&q,-(double)i);
    T("sum 1..10 - 1..5", quire32i_round(&q), 40.0, 0.001);

    {double M=1e15,m=1e-15;
    quire32i_clear(&q);
    quire32i_addd(&q,M); quire32i_addd(&q,m); quire32i_addd(&q,-M);
    r=quire32i_round(&q);
    printf("Gustafson test           = %.3e  (exp   %.3e)  %s\n",
           r,m,fabs(r-m)<m*0.001?"PASS":"FAIL");
    printf("  Kahan gives:             %.3e\n",(M+m)-M);}

    quire32i_clear(&q);
    for(i=1;i<=1000;i++) quire32i_addd(&q,1.0);
    T("sum 1000x1.0", quire32i_round(&q), 1000.0, 0.001);

    printf("\nquire32_int.h: pure integer accumulation\n");
    printf("No doubles in the accumulator. No FPU. No F. No D.\n");
    printf("MUL+MULH+carry chain on bare RV32I.\n");
    return 0;
}
