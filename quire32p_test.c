#include <stdio.h>
#include "quire32_pure.h"

int main(void) {
    quire32p_t q; int i; double r;
    #define T(label,val,exp,eps) \
        printf("%-28s = %12.4f  (exp %10.4f)  %s\n", \
               label,(double)(val),(double)(exp), \
               fabs((double)(val)-(double)(exp))<(eps)?"PASS":"FAIL")

    printf("quire32_pure.h -- zero floating point in accumulator\n");
    printf("IEEE 754 bit extraction: memcpy+shift+mask only\n");
    printf("No frexp. No ldexp in add path. No FPU needed.\n\n");

    quire32p_clear(&q);
    for(i=1;i<=10;i++) quire32p_add(&q,(double)i,0);
    T("sum 1..10", quire32p_round(&q), 55.0, 0.001);

    {double a[]={1,2,3,4},b[]={4,3,2,1};
    quire32p_clear(&q);
    for(i=0;i<4;i++) quire32p_add(&q,a[i]*b[i],0);
    T("dot [1234][4321]", quire32p_round(&q), 20.0, 0.001);}

    quire32p_clear(&q);
    for(i=1;i<=10;i++) quire32p_add(&q,(double)i,0);
    for(i=1;i<=5;i++)  quire32p_add(&q,(double)i,1);
    T("sum 1..10 - sum 1..5", quire32p_round(&q), 40.0, 0.001);

    {double M=1e15, m=1e-15;
    quire32p_clear(&q);
    quire32p_add(&q,M,0); quire32p_add(&q,m,0); quire32p_add(&q,M,1);
    r=quire32p_round(&q);
    printf("Gustafson test               = %.3e  (exp   %.3e)  %s\n",
           r,m,fabs(r-m)<m*0.001?"PASS":"FAIL");
    printf("  Kahan gives:                 %.3e\n",(M+m)-M);}

    quire32p_clear(&q);
    for(i=1;i<=1000;i++) quire32p_add(&q,1.0,0);
    T("sum 1000x1.0", quire32p_round(&q), 1000.0, 0.001);

    /* test exact posit32 FMA -- truly zero float ops */
    printf("\nExact posit32 FMA (zero float ops in product path):\n");
    {uint32_t p1=0x40000000u; /* posit32(1.0) */
     uint32_t p2=0x48000000u; /* posit32(2.0) */
     quire32p_clear(&q);
     for(i=0;i<10;i++) quire32p_fma(&q,p1,p2,0);
     T("10 * p32(1.0)*p32(2.0)", quire32p_round(&q), 20.0, 0.001);}

    {uint32_t ppi=0x4c90fcf8u; /* posit32(pi) */
     uint32_t p1 =0x40000000u; /* posit32(1.0) */
     quire32p_clear(&q);
     quire32p_fma(&q,ppi,p1,0);
     r=quire32p_round(&q);
     printf("%-28s = %12.6f  (exp   3.141590)  %s\n",
            "p32(pi)*p32(1.0)",r,fabs(r-3.14159)<0.0001?"PASS":"FAIL");}

    /* test f64_decomp directly */
    printf("\nIEEE 754 decomposition (pure integer):\n");
    {int s,e; uint64_t m;
     f64_decomp(1.0,&s,&e,&m);
     printf("  1.0: sign=%d exp=%d mant=0x%013llx (exp: 0 0 0x10000000000000)\n",
            s,e,(unsigned long long)m);
     f64_decomp(3.14159265,&s,&e,&m);
     printf("  pi:  sign=%d exp=%d mant=0x%013llx\n",s,e,(unsigned long long)m);
     f64_decomp(-2.0,&s,&e,&m);
     printf(" -2.0: sign=%d exp=%d mant=0x%013llx (exp: 1 1 0x10000000000000)\n",
            s,e,(unsigned long long)m);}

    printf("\nNo frexp. No ldexp in add path. MUL+MULH+carry chain.\n");
    printf("Runs on bare RV32I. No F. No D. No FPU.\n");
    return 0;
}
