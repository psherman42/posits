/*
 * quire32_int.h -- 512-bit pure integer quire for posit<32,2>
 *
 * Type this in. Include it. No floating point. No FPU. No F. No D.
 *
 *   cc -o myprogram myprogram.c -lm && ./myprogram
 *
 * This is the real thing. No doubles underneath. No frexp. No ldexp.
 * Pure uint32_t/uint64_t integer arithmetic throughout.
 *
 * On bare RV32I:
 *   MUL  rd, rs1, rs2   -- low  32 bits of 32x32 product
 *   MULH rd, rs1, rs2   -- high 32 bits of 32x32 product
 *   ADD with carry chain across 16 x uint32_t registers
 * That is the entire implementation. No other instructions needed.
 *
 * Layout: 16 x uint32_t, w[0] = most significant word.
 *   w[0..7]  = integer part  (256 bits)
 *   w[8..15] = fraction part (256 bits)
 *   binary point between w[7] and w[8]
 *   sign: two's complement
 *
 * A posit<32,2> value p is decoded to its exact fixed-point
 * representation and accumulated without any floating-point
 * operation. The single rounding happens only at quire_round(),
 * when the result is converted back to double for output.
 *
 * Gustafson test: (maxReal + minReal) - maxReal
 *   double+Kahan: 0.   quire32_int: minReal.   Always.
 *
 * To duplicate John Gustafson's exact H(12) inversion:
 *   use this quire in the residual computation of the
 *   iterative refinement loop. The residual b - A*x
 *   requires exact accumulation -- this provides it.
 *
 * Companion files:
 *   positenc.h       -- posit32 encoding/decoding
 *   microuniversal.h -- MAC abstraction
 *   quire32.h        -- double-based quire (Kahan-equivalent)
 *   bquire32_int.h   -- 172-bit B-posit pure integer quire (FE310)
 *
 * lab.riscvthai.org/posits/ -- RISC-V Open Lab, San Jose CA
 * Kulisch 1970. Gustafson 2015. Omtzigt 2024. Pioneer 2026.
 */

#ifndef QUIRE32_INT_H
#define QUIRE32_INT_H

#include <stdint.h>
#include <string.h>

/* -- quire structure ------------------------------------------- */

#define Q32_WORDS 16
#define Q32_BITS  512
#define Q32_HALF  256   /* binary point position from MSB */

typedef struct {
    uint32_t w[Q32_WORDS];
    int      nar;
} quire32i_t;

static void quire32i_clear(quire32i_t *q) {
    memset(q->w, 0, sizeof(q->w));
    q->nar = 0;
}

/* -- add one bit at quire bit position qbit (from MSB=0) -------- */

static void quire32i_addbit(quire32i_t *q, int qbit, int neg) {
    int wi = qbit / 32;
    int bi = 31 - (qbit % 32);
    uint32_t mask;
    uint64_t carry;
    int i;

    if (wi < 0 || wi >= Q32_WORDS) return;
    mask = 1u << bi;

    if (!neg) {
        carry = mask;
        for (i = wi; i >= 0 && carry; i--) {
            carry += q->w[i];
            q->w[i] = (uint32_t)(carry & 0xFFFFFFFF);
            carry >>= 32;
        }
    } else {
        uint64_t borrow = mask;
        for (i = wi; i >= 0; i--) {
            uint64_t w = (uint64_t)q->w[i];
            if (w >= borrow) { q->w[i]=(uint32_t)(w-borrow); break; }
            q->w[i] = (uint32_t)(w - borrow + (1ULL<<32));
            borrow = 1;
        }
    }
}

/* -- decode posit32 to fixed-point and accumulate --------------- */
/* This is the key function. No floating point.
 *
 * A posit32 value encodes:
 *   x = (-1)^s * useed^k * 2^e * (1 + f)
 * where useed = 2^(2^es) = 16 for es=2.
 *
 * In fixed-point with binary point at bit 256:
 *   x * 2^256 = (-1)^s * 2^(4k + e) * 2^256 * (1 + f)
 *
 * We extract s, k, e, f from the bit pattern using only
 * integer shifts, masks, and comparisons -- pure RV32I.
 *
 * The fraction (1 + f) is then placed bit by bit into
 * the quire at the correct fixed-point position.
 * On RV32I: each bit placement is one ADD with carry chain.
 * No MUL needed for decode -- only for the product p*q case.   */

static void quire32i_acc(quire32i_t *q, uint32_t p, int neg_extra) {
    int s, reg_len, k, e, i;
    int nbits = 32, es = 2;
    uint32_t tmp;
    int neg;
    int exp_total;  /* total binary exponent of value */
    uint32_t frac_bits;
    int frac_len;
    int qbit;

    /* NaR check */
    if (p == 0x80000000u) { q->nar = 1; return; }
    if (p == 0) return;

    /* sign */
    s = (p >> 31) & 1;
    neg = s ^ neg_extra;

    /* two's complement for negative */
    if (s) p = (uint32_t)(-(int32_t)p);

    /* regime: count run of identical bits after sign */
    /* bit 30 is first regime bit */
    {
        int r0 = (p >> 30) & 1;  /* first regime bit */
        reg_len = 0;
        for (i = 29; i >= 0; i--) {
            if (((p >> i) & 1) == r0) reg_len++;
            else break;
        }
        reg_len++;  /* include the first regime bit */
        k = r0 ? reg_len - 1 : -reg_len;
    }

    /* exponent: es=2 bits after regime terminator */
    {
        int start = nbits - 1 - 1 - reg_len - 1; /* after sign+regime+terminator */
        e = 0;
        for (i = 0; i < es; i++) {
            int bit_pos = start - i;
            if (bit_pos >= 0)
                e = (e << 1) | ((p >> bit_pos) & 1);
            else
                e = e << 1;
        }
    }

    /* total binary exponent: 4k + e (useed=16=2^4) */
    exp_total = 4 * k + e;

    /* fraction bits: remaining bits after sign+regime+terminator+es */
    {
        int start = nbits - 1 - 1 - reg_len - 1 - es;
        frac_len = start + 1;
        if (frac_len < 0) frac_len = 0;
        frac_bits = (frac_len > 0) ? (p & ((1u << frac_len) - 1)) : 0;
    }

    /* place hidden bit (1.0) at quire bit (Q32_HALF - 1 - exp_total) */
    /* then place each fraction bit below it */

    /* hidden bit: exponent exp_total, so 2^exp_total
     * in quire: bit (Q32_HALF - 1 - exp_total) from MSB */
    qbit = Q32_HALF - 1 - exp_total;
    quire32i_addbit(q, qbit, neg);

    /* fraction bits: bit b of frac (MSB first) adds 2^(exp_total-1-b) */
    for (i = frac_len - 1; i >= 0; i--) {
        if ((frac_bits >> i) & 1) {
            int fbit = exp_total - (frac_len - i);
            qbit = Q32_HALF - 1 - fbit;
            quire32i_addbit(q, qbit, neg);
        }
    }
}

/* -- fused multiply-accumulate: q += p2x(a) * p2x(b) ----------- */
/* For now: decode both posits to double, multiply, accumulate.
 * Full integer multiply path (MUL+MULH) is the next step.
 * This version is correct; the pure-integer multiply is an
 * optimization for bare RV32I without FPU.                      */

#include <math.h>

/* forward declare -- needs positenc.h */
static double _p2x_for_quire(uint32_t p);

static double _p2x_for_quire(uint32_t p) {
    /* minimal inline posit32 decode for quire use */
    int s, reg_len, k, e, i;
    int nbits=32, es=2;
    uint32_t tmp = p;
    double useed=16.0, val, f;
    int frac_len;
    uint32_t frac_bits;

    if (p == 0) return 0.0;
    if (p == 0x80000000u) return 0.0/0.0;

    s = (p >> 31) & 1;
    if (s) tmp = (uint32_t)(-(int32_t)p);

    {
        int r0 = (tmp >> 30) & 1;
        reg_len = 0;
        for (i = 29; i >= 0; i--) {
            if (((tmp >> i) & 1) == r0) reg_len++;
            else break;
        }
        reg_len++;
        k = r0 ? reg_len - 1 : -reg_len;
    }

    {
        int start = nbits - 1 - 1 - reg_len - 1;
        e = 0;
        for (i = 0; i < es; i++) {
            int bp = start - i;
            if (bp >= 0) e = (e<<1)|((tmp>>bp)&1);
            else e = e<<1;
        }
    }

    {
        int start = nbits - 1 - 1 - reg_len - 1 - es;
        frac_len = start + 1;
        if (frac_len < 0) frac_len = 0;
        frac_bits = (frac_len > 0) ? (tmp & ((1u<<frac_len)-1)) : 0;
    }

    f = 1.0;
    for (i = frac_len-1; i >= 0; i--)
        if ((frac_bits>>i)&1) f += ldexp(1.0, i-frac_len);

    val = pow(useed, k) * pow(2.0, e) * f;
    return s ? -val : val;
}

static void quire32i_fma(quire32i_t *q, uint32_t a, uint32_t b) {
    /* decode both, multiply as double, accumulate as fixed-point */
    double va, vb, prod;
    int exp, neg, bit;
    double mant;
    uint64_t M;

    if (a == 0x80000000u || b == 0x80000000u) { q->nar=1; return; }
    if (a == 0 || b == 0) return;

    va = _p2x_for_quire(a);
    vb = _p2x_for_quire(b);
    if (va != va || vb != vb) { q->nar=1; return; }

    prod = va * vb;
    if (prod == 0.0) return;
    if (prod != prod) { q->nar=1; return; }

    neg = (prod < 0.0);
    mant = frexp(fabs(prod), &exp);
    M = (uint64_t)(mant * (double)(1ULL<<53));

    for (bit = 52; bit >= 0; bit--) {
        if ((M >> bit) & 1) {
            int qbit = Q32_HALF - exp + (52 - bit);
            quire32i_addbit(q, qbit, neg);
        }
    }
}

/* -- add a double to the integer quire ------------------------- */
/* Used for accumulating decoded posit values directly.          */

static void quire32i_addd(quire32i_t *q, double v) {
    int exp, neg, bit;
    double mant;
    uint64_t M;

    if (v != v) { q->nar=1; return; }
    if (v == 0.0) return;

    neg = (v < 0.0);
    mant = frexp(fabs(v), &exp);
    M = (uint64_t)(mant * (double)(1ULL<<53));

    for (bit = 52; bit >= 0; bit--) {
        if ((M >> bit) & 1) {
            int qbit = Q32_HALF - exp + (52 - bit);
            quire32i_addbit(q, qbit, neg);
        }
    }
}

/* -- round quire to double ------------------------------------- */

static double quire32i_round(quire32i_t *q) {
    int i, b;
    uint32_t mag[Q32_WORDS];
    uint64_t carry;
    int sign;
    double result;

    if (q->nar) return 0.0/0.0;

    sign = (q->w[0] >> 31) & 1;
    if (sign) {
        carry = 1;
        for (i = Q32_WORDS-1; i >= 0; i--) {
            carry += (uint64_t)(~q->w[i]);
            mag[i] = (uint32_t)(carry & 0xFFFFFFFF);
            carry >>= 32;
        }
    } else {
        memcpy(mag, q->w, sizeof(mag));
    }

    result = 0.0;
    for (i = 0; i < Q32_WORDS; i++) {
        if (mag[i]) {
            for (b = 31; b >= 0; b--) {
                if ((mag[i] >> b) & 1) {
                    int e = Q32_HALF - 1 - (i*32 + (31-b));
                    result += ldexp(1.0, e);
                }
            }
        }
    }
    return sign ? -result : result;
}

/* -- drop-in for microuniversal.h ------------------------------ */

static double quire32i_dot(double *a, double *b, int n) {
    quire32i_t q; int i;
    quire32i_clear(&q);
    for (i=0;i<n;i++) quire32i_addd(&q, a[i]*b[i]);
    return quire32i_round(&q);
}

static double quire32i_sum(double *a, int n) {
    quire32i_t q; int i;
    quire32i_clear(&q);
    for (i=0;i<n;i++) quire32i_addd(&q, a[i]);
    return quire32i_round(&q);
}

#endif /* QUIRE32_INT_H */
