/*
 * bposit_quire_double.h -- B-posit quire: double-precision API
 *
 * Extends bposit_quire.h with double-valued add, fma, round, dot, sum.
 * Requires math.h and IEEE 754 double (64-bit).
 *
 * For pure integer operation (no FPU, bare RV32I):
 *   include bposit_quire.h only -- use bquire_add_posit/round_posit.
 *
 * Warsy/Sherman CoNGA 2026. lab.riscvthai.org/posits/
 */

#ifndef BPOSIT_QUIRE_DOUBLE_H
#define BPOSIT_QUIRE_DOUBLE_H

#include <math.h>
#include "bposit_quire.h"

static void bquire_add(bquire_t *q, double v) {
    int exp, neg, b;
    double mant;
    uint64_t M;
    int half = BPOSIT_QSIZE / 2;

    if (v != v) { q->nar = 1; return; }
    if (v == 0.0) return;

    neg  = (v < 0.0);
    mant = frexp(fabs(v), &exp);
    M    = (uint64_t)(mant * (double)(1ULL << 53));

    for (b = 52; b >= 0; b--) {
        if ((M >> b) & 1) {
            int qbit = half - exp + (52 - b);
            bquire_add_bit(q, qbit, neg);
        }
    }
}

static void bquire_fma(bquire_t *q, double a, double b) {
    if (a != a || b != b) { q->nar = 1; return; }
    bquire_add(q, a * b);
}

/* -- round B-posit quire to double ---------------------------- */

static double bquire_round(bquire_t *q) {
    int i, b;
    uint32_t mag[BPOSIT_QWORDS];
    uint64_t carry;
    int sign, half;
    double result;

    if (q->nar) return 0.0 / 0.0;

    sign = (q->w[0] >> 31) & 1;
    if (sign) {
        carry = 1;
        for (i = BPOSIT_QWORDS-1; i >= 0; i--) {
            carry += (uint64_t)(~q->w[i]);
            mag[i] = (uint32_t)(carry & 0xFFFFFFFF);
            carry >>= 32;
        }
    } else {
        memcpy(mag, q->w, sizeof(mag));
    }

    half = BPOSIT_QSIZE / 2;
    result = 0.0;
    for (i = 0; i < BPOSIT_QWORDS; i++) {
        if (mag[i]) {
            for (b = 31; b >= 0; b--) {
                if ((mag[i] >> b) & 1) {
                    int e = half - 1 - (i*32 + (31-b));
                    result += ldexp(1.0, e);
                }
            }
        }
    }
    return sign ? -result : result;
}

/* -- drop-in for microuniversal.h ------------------------------ */

static double bquire_dot(double *a, double *b, int n) {
    bquire_t q; int i;
    bquire_clear(&q);
    for (i=0;i<n;i++) bquire_fma(&q,a[i],b[i]);
    return bquire_round(&q);
}

static double bquire_sum(double *a, int n) {
    bquire_t q; int i;
    bquire_clear(&q);
    for (i=0;i<n;i++) bquire_add(&q,a[i]);
    return bquire_round(&q);
}


#endif /* BPOSIT_QUIRE_DOUBLE_H */
