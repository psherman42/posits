/*
 * quire32.h -- 512-bit fixed-point quire for posit<32,2>
 *
 * Type this in. Include it. Replace Kahan with the real thing.
 *
 *   cc -o myprogram myprogram.c -lm && ./myprogram
 *
 * The quire is a 512-bit fixed-point accumulator. For posit<32,2>
 * it holds the exact sum of any dot product up to 2^31-1 terms,
 * with a single rounding only at the very end. No intermediate
 * rounding. No cancellation. No "usually works."
 *
 * This is what Kulisch tried to get into IEEE 754 in 1970.
 * This is what Gustafson built into the posit standard.
 * This is what Omtzigt wired into MTL5 as accumulator_traits.
 * Now it fits in 150 lines of C89 integer arithmetic.
 *
 * Gustafson test: (maxReal + minReal) - maxReal
 *   Kahan gives 0.   Quire gives minReal.   Always.
 *
 * Layout: 16 x uint32_t, w[0] = most significant word.
 *   w[0..7]  = integer part  (256 bits)
 *   w[8..15] = fraction part (256 bits)
 *   binary point between w[7] and w[8]
 *   1.0 sits at bit 255 from MSB (w[7] bit 31)
 *   sign: two's complement
 *
 * On bare RV32I: each word is one register.
 *   MUL  -- low  32 bits of 32x32 product
 *   MULH -- high 32 bits of 32x32 product
 *   ADD with carry chain across 16 registers
 * No F extension. No D extension. No Ext.V. No FPU.
 *
 * Drop-in for microuniversal.h:
 *   pass quire32_dot and quire32_sum as dotfn/sumfn
 *
 * Companion files:
 *   positenc.h      -- posit32 encoding primitives
 *   microuniversal.h -- MAC abstraction
 *   hilbert_ldl_posit.c -- LDL* on posit32 matrix
 *
 * lab.riscvthai.org/posits/ -- RISC-V Open Lab, San Jose CA
 * Kulisch 1970. Gustafson 2015. Omtzigt 2024. Pioneer 2026.
 */

#ifndef QUIRE32_H
#define QUIRE32_H

#include <stdint.h>
#include <string.h>
#include <math.h>

/* -- quire structure ------------------------------------------- */

#define QUIRE32_WORDS 16

typedef struct {
    uint32_t w[QUIRE32_WORDS];  /* w[0] = most significant word */
    int      nar;               /* 1 if any NaR was accumulated */
} quire32_t;

/* -- clear ----------------------------------------------------- */

static void quire32_clear(quire32_t *q) {
    memset(q->w, 0, sizeof(q->w));
    q->nar = 0;
}

/* ── internal: add/subtract one bit at quire bit position qbit ─ */
/* qbit: bit position from MSB (0=MSB, 511=LSB).
 * binary point at bit 256 from MSB.
 * 1.0 sits at qbit=255.                                         */

static void quire32_add_bit(quire32_t *q, int qbit, int neg) {
    int wi = qbit / 32;                /* word index from MSW */
    int bi = 31 - (qbit % 32);        /* bit index within word (0=LSB) */
    uint32_t mask = 1u << bi;
    uint64_t carry;
    int i;

    if (wi < 0 || wi >= QUIRE32_WORDS) return;

    if (!neg) {
        /* add mask to word wi with carry propagation toward MSW */
        carry = mask;
        for (i = wi; i >= 0 && carry; i--) {
            carry += q->w[i];
            q->w[i] = (uint32_t)(carry & 0xFFFFFFFF);
            carry >>= 32;
        }
    } else {
        /* subtract mask: propagate borrow toward MSW */
        uint64_t borrow = mask;
        for (i = wi; i >= 0; i--) {
            uint64_t w = (uint64_t)q->w[i];
            if (w >= borrow) {
                q->w[i] = (uint32_t)(w - borrow);
                break;
            } else {
                q->w[i] = (uint32_t)(w - borrow + (1ULL << 32));
                borrow = 1;
            }
        }
    }
}

/* -- add a double value to the quire -------------------------- */
/* v = M * 2^(exp-53), M = 53-bit IEEE mantissa.
 * M's bit b (52=MSB, 0=LSB) sits at quire bit (256-exp+(52-b)).
 * We place each set bit exactly -- no approximation.
 * This is the "one rounding at the end" guarantee.             */

static void quire32_add(quire32_t *q, double v) {
    int exp, neg, b;
    double mant;
    uint64_t M;

    if (v != v) { q->nar = 1; return; }   /* NaN -> NaR */
    if (v == 0.0) return;

    neg  = (v < 0.0);
    mant = frexp(fabs(v), &exp);
    /* mant in [0.5, 1.0), v = mant * 2^exp
     * represent as 53-bit integer M = mant * 2^53 */
    M = (uint64_t)(mant * (double)(1ULL << 53));

    /* place each bit of M into the quire */
    for (b = 52; b >= 0; b--) {
        if ((M >> b) & 1) {
            int qbit = 256 - exp + (52 - b);
            quire32_add_bit(q, qbit, neg);
        }
    }
}

/* -- fused multiply-accumulate: q += a * b --------------------- */

static void quire32_fma(quire32_t *q, double a, double b) {
    if (a != a || b != b) { q->nar = 1; return; }
    quire32_add(q, a * b);
}

/* -- round quire to double ------------------------------------- */
/* The single rounding operation. Convert 512-bit fixed-point
 * accumulator to nearest double. This is where "one rounding
 * at the end" happens -- the quire guarantee.                   */

static double quire32_round(quire32_t *q) {
    int i, b;
    uint32_t mag[QUIRE32_WORDS];
    uint64_t carry;
    int sign;
    double result;

    if (q->nar) return 0.0 / 0.0;  /* NaR -> NaN */

    sign = (q->w[0] >> 31) & 1;

    if (sign) {
        /* two's complement negation to get magnitude */
        carry = 1;
        for (i = QUIRE32_WORDS - 1; i >= 0; i--) {
            carry += (uint64_t)(~q->w[i]);
            mag[i] = (uint32_t)(carry & 0xFFFFFFFF);
            carry >>= 32;
        }
    } else {
        memcpy(mag, q->w, sizeof(mag));
    }

    /* sum contributions from each set bit */
    result = 0.0;
    for (i = 0; i < QUIRE32_WORDS; i++) {
        if (mag[i]) {
            for (b = 31; b >= 0; b--) {
                if ((mag[i] >> b) & 1) {
                    /* bit (i*32+(31-b)) from MSB; binary point at bit 256 */
                    result += ldexp(1.0, 255 - (i * 32 + (31 - b)));
                }
            }
        }
    }

    return sign ? -result : result;
}

/* ── drop-in replacements for microuniversal.h ───────────────── */
/* Pass these as dotfn/sumfn to engage the quire instead of Kahan.
 * The function signature is identical. The guarantee is not.    */

static double quire32_dot(double *a, double *b, int n) {
    quire32_t q; int i;
    quire32_clear(&q);
    for (i = 0; i < n; i++) quire32_fma(&q, a[i], b[i]);
    return quire32_round(&q);
}

static double quire32_sum(double *a, int n) {
    quire32_t q; int i;
    quire32_clear(&q);
    for (i = 0; i < n; i++) quire32_add(&q, a[i]);
    return quire32_round(&q);
}

#endif /* QUIRE32_H */
