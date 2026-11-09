/*
 * quire32i_fma exact -- posit32 product without doubles
 *
 * A posit32 value decodes to:
 *   x = (-1)^s * 2^(4k+e) * (1 + f)
 * where f has at most 28 bits (fraction field of posit32).
 *
 * The significand (1 + f) fits in 29 bits.
 * Product of two significands fits in 58 bits.
 * Total binary exponent of product: (4k1+e1) + (4k2+e2).
 *
 * So: p1*p2 = (-1)^(s1^s2) * 2^(exp1+exp2) * (sig1*sig2)
 * where sig1*sig2 is a 58-bit integer.
 *
 * We compute sig1*sig2 using uint64_t multiply -- exact.
 * Then place the bits into the quire at position (exp1+exp2).
 * No doubles. No rounding. Pure integer.
 *
 * On RV32I:
 *   MUL  -- low  32 bits
 *   MULH -- high 32 bits
 *   Two instructions = exact 64-bit product of 32-bit significands
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#define Q32_WORDS 16
#define Q32_HALF  256

typedef struct { uint32_t w[Q32_WORDS]; int nar; } Q32i;

static void qclear(Q32i *q){ memset(q->w,0,sizeof(q->w)); q->nar=0; }

static void qaddbit(Q32i *q, int qbit, int neg) {
    int wi=qbit/32, bi=31-(qbit%32), i;
    uint32_t mask; uint64_t carry;
    if(wi<0||wi>=Q32_WORDS) return;
    mask=1u<<bi;
    if(!neg){
        carry=mask;
        for(i=wi;i>=0&&carry;i--){carry+=q->w[i];q->w[i]=(uint32_t)(carry&0xFFFFFFFF);carry>>=32;}
    } else {
        uint64_t borrow=mask;
        for(i=wi;i>=0;i--){
            uint64_t w=(uint64_t)q->w[i];
            if(w>=borrow){q->w[i]=(uint32_t)(w-borrow);break;}
            q->w[i]=(uint32_t)(w-borrow+(1ULL<<32));borrow=1;
        }
    }
}

/* -- decode posit32 to sign, binary exponent, significand -------- */
/* Returns:
 *   sign:  0 or 1
 *   exp:   total binary exponent (4k + e)
 *   sig:   significand as integer with sig_bits fractional bits
 *          sig = (1 + fraction) * 2^frac_bits
 *   frac_bits: number of fraction bits                            */

static void p32_decode(uint32_t p,
                       int *sign, int *exp, uint64_t *sig, int *frac_bits) {
    int i, r0, reg_len, k, e, flen;
    uint32_t tmp = p;

    *sign = (p >> 31) & 1;
    if (*sign) tmp = (uint32_t)(-(int32_t)p);

    /* regime */
    r0 = (tmp >> 30) & 1;
    reg_len = 0;
    for (i = 29; i >= 0; i--) {
        if (((tmp >> i) & 1) == r0) reg_len++;
        else break;
    }
    reg_len++;
    k = r0 ? reg_len - 1 : -reg_len;

    /* exponent (es=2) */
    {
        int start = 30 - reg_len - 1; /* after sign+regime+terminator */
        e = 0;
        for (i = 0; i < 2; i++) {
            int bp = start - i;
            if (bp >= 0) e = (e<<1)|((tmp>>bp)&1);
            else e = e<<1;
        }
    }

    /* fraction */
    {
        int start = 30 - reg_len - 1 - 2;
        flen = start + 1;
        if (flen < 0) flen = 0;
        uint32_t fbits = (flen > 0) ? (tmp & ((1u<<flen)-1)) : 0;

        /* significand = (1 + fraction) as integer
         * = 2^flen + fbits   (hidden bit shifted left by flen) */
        *sig = ((uint64_t)1 << flen) | fbits;
        *frac_bits = flen;
    }

    *exp = 4*k + e;
}

/* -- exact posit32 FMA: q += p2x(a) * p2x(b) ------------------- */
/* Pure integer. No doubles. MUL+MULH equivalent.                 */

static void qfma_exact(Q32i *q, uint32_t a, uint32_t b) {
    int sa, sb, ea, eb, fba, fbb;
    uint64_t siga, sigb, prod;
    int neg, exp_prod, frac_prod, bit, qbit, i;

    if (a==0x80000000u||b==0x80000000u){q->nar=1;return;}
    if (a==0||b==0) return;

    p32_decode(a, &sa, &ea, &siga, &fba);
    p32_decode(b, &sb, &eb, &sigb, &fbb);

    /* product sign */
    neg = sa ^ sb;

    /* product significand: siga * sigb
     * siga has fba fractional bits (up to 28)
     * sigb has fbb fractional bits (up to 28)
     * product has fba+fbb fractional bits (up to 56)
     * fits exactly in uint64_t (max 58 bits total)              */
    prod = siga * sigb;  /* exact -- no overflow for posit32 */
    frac_prod = fba + fbb;

    /* product exponent: ea + eb - frac_prod
     * (since siga = (1+fa)*2^fba, sigb = (1+fb)*2^fbb)
     * actual value = (-1)^neg * 2^(ea+eb) * (siga/2^fba) * (sigb/2^fbb)
     *              = (-1)^neg * 2^(ea+eb-fba-fbb) * prod              */
    exp_prod = ea + eb;

    /* place each bit of prod into quire
     * prod bit i (from MSB, 0-indexed from bit 57 down) has value
     * 2^(exp_prod - frac_prod + (total_bits-1-i))
     * where total_bits = position of MSB of prod + 1              */
    {
        /* find MSB of prod */
        int msb = 0;
        for (i = 63; i >= 0; i--) {
            if ((prod >> i) & 1) { msb = i; break; }
        }

        for (i = msb; i >= 0; i--) {
            if ((prod >> i) & 1) {
                /* this bit represents 2^(exp_prod - frac_prod + i) */
                int e = exp_prod - frac_prod + i;
                qbit = Q32_HALF - 1 - e;
                qaddbit(q, qbit, neg);
            }
        }
    }
}

/* -- round quire to double ------------------------------------- */

static double qround(Q32i *q) {
    int i, b; uint32_t mag[Q32_WORDS];
    uint64_t carry; int sign; double r=0.0;
    if(q->nar) return 0.0/0.0;
    sign=(q->w[0]>>31)&1;
    if(sign){carry=1;for(i=Q32_WORDS-1;i>=0;i--){carry+=(uint64_t)(~q->w[i]);mag[i]=(uint32_t)(carry&0xFFFFFFFF);carry>>=32;}}
    else memcpy(mag,q->w,sizeof(mag));
    for(i=0;i<Q32_WORDS;i++) if(mag[i])
        for(b=31;b>=0;b--) if((mag[i]>>b)&1)
            r+=ldexp(1.0, Q32_HALF-1-(i*32+(31-b)));
    return sign?-r:r;
}

/* -- also need addd for the residual computation --------------- */

static void qaddd(Q32i *q, double v) {
    int exp,neg,bit; double mant; uint64_t M;
    if(v!=v){q->nar=1;return;} if(v==0.0)return;
    neg=(v<0.0); mant=frexp(fabs(v),&exp);
    M=(uint64_t)(mant*(double)(1ULL<<53));
    for(bit=52;bit>=0;bit--) if((M>>bit)&1)
        qaddbit(q,Q32_HALF-exp+(52-bit),neg);
}

/* -- test ------------------------------------------------------- */

int main(void) {
    Q32i q; int i; double r;

    printf("quire32i_fma exact -- pure integer posit32 products\n\n");

    /* test basic accumulation */
    qclear(&q);
    for(i=1;i<=10;i++) qaddd(&q,(double)i);
    r=qround(&q);
    printf("sum 1..10        = %8.1f (exp  55) %s\n",r,r==55?"PASS":"FAIL");

    /* Gustafson test */
    {double M=1e15,m=1e-15;
    qclear(&q); qaddd(&q,M); qaddd(&q,m); qaddd(&q,-M);
    r=qround(&q);
    printf("Gustafson test   = %.3e (exp %.3e) %s\n",r,m,
           fabs(r-m)<m*0.001?"PASS":"FAIL");}

    /* test exact posit32 FMA
     * encode 1.0 and 2.0 as posit32, multiply exactly */
    {
        uint32_t p1=0x40000000u; /* posit32(1.0) */
        uint32_t p2=0x48000000u; /* posit32(2.0) */
        int sa,ea,fba,sb,eb,fbb;
        uint64_t siga,sigb;

        p32_decode(p1,&sa,&ea,&siga,&fba);
        p32_decode(p2,&sb,&eb,&sigb,&fbb);
        printf("\nposit32(1.0): sign=%d exp=%d sig=%llu frac_bits=%d\n",
               sa,ea,(unsigned long long)siga,fba);
        printf("posit32(2.0): sign=%d exp=%d sig=%llu frac_bits=%d\n",
               sb,eb,(unsigned long long)sigb,fbb);

        qclear(&q);
        for(i=0;i<10;i++) qfma_exact(&q,p1,p2); /* 10 * 1.0 * 2.0 = 20 */
        r=qround(&q);
        printf("10 * p32(1)*p32(2) = %8.1f (exp 20) %s\n",
               r,r==20?"PASS":"FAIL");
    }

    /* test with pi */
    {
        uint32_t ppi=0x4c90fcf8u; /* posit32(pi) */
        int s,e,fb; uint64_t sig;
        p32_decode(ppi,&s,&e,&sig,&fb);
        printf("\nposit32(pi): sign=%d exp=%d sig=%llu frac_bits=%d\n",
               s,e,(unsigned long long)sig,fb);

        qclear(&q);
        qfma_exact(&q,ppi,0x40000000u); /* pi * 1.0 */
        r=qround(&q);
        printf("p32(pi)*p32(1.0)   = %.6f (exp %.6f) %s\n",
               r,3.141593,fabs(r-3.141593)<0.0001?"PASS":"FAIL");
    }

    return 0;
}
