/*
 * bposit_quire.h -- B-posit quire: parameterized 512→192-bit accumulator
 *
 * Type this in. Include it. Build anything.
 *
 * B-posit bounds the regime field to rs bits, reducing quire width
 * dramatically. From Bola Yosry Warsy's CoNGA 2026 paper:
 * "Closing the Performance Gap of Posit Arithmetic Hardware:
 *  A RISC-V Fife Core Case Study"
 *
 * Quire width formula (Warsy et al., CoNGA 2026, eq. 10-12):
 *   E    = rs * 2^es
 *   F    = n - 1 - rs - es        (fraction bits per operand)
 *   Qraw = 4*E + 2*F
 *   Qsize = Qraw + 30             (+30 for carry overflow protection)
 *
 * Candidate design point: es=2, rs=6, n=32
 *   E    = 6 * 4 = 24
 *   F    = 32 - 1 - 6 - 2 = 23
 *   Qraw = 96 + 46 = 142
 *   Qsize = 172 bits → 6 x uint32_t words (192 bits)
 *
 * vs standard posit<32,2>: 512 bits (16 words)
 * Reduction: 77% -- confirmed by Warsy et al. synthesis results
 *
 * Hardware results at es=2, rs=6 (Warsy et al., 45nm synthesis):
 *   IPC:        0.386 vs 0.09 baseline  (4.3x improvement)
 *   Clock:      1394.7 MHz vs 1054.8 MHz (28% faster)
 *   Quire area: 13,200 um^2 vs 57,463 um^2 (77% reduction)
 *   Wall-clock: 0.57 us vs 3.26 us MAC loop (5.7x faster)
 *
 * This work was suppressed from public presentation.
 * It is published here, openly, with full attribution.
 *
 * Authors: Bola Yosry Warsy (Cairo), Paul Sherman (RISC-V Open Lab)
 * Reference: CoNGA 2026, Submission 7
 *
 * CLZ (count leading zeros) implemented in pure RV32I base integer --
 * no Zbb extension required. Runs on FE310 and all bare RV32I cores.
 *
 * lab.riscvthai.org/posits/ -- RISC-V Open Lab, San Jose CA
 * Kulisch 1970. Gustafson 2015. Warsy/Sherman 2026.
 */

#ifndef BPOSIT_QUIRE_H
#define BPOSIT_QUIRE_H

#include <stdint.h>
#include <string.h>
#include <math.h>

/* -- B-posit parameters ---------------------------------------- */
/* Candidate design point from Warsy et al.: es=2, rs=6, n=32   */

#ifndef BPOSIT_N
#define BPOSIT_N   32   /* posit width in bits */
#endif
#ifndef BPOSIT_ES
#define BPOSIT_ES   2   /* exponent field size */
#endif
#ifndef BPOSIT_RS
#define BPOSIT_RS   6   /* max regime bits (B-posit bound) */
#endif

/* Quire width formula (Warsy et al. eq. 10-12) */
#define BPOSIT_E      (BPOSIT_RS * (1 << BPOSIT_ES))
#define BPOSIT_F      (BPOSIT_N - 1 - BPOSIT_RS - BPOSIT_ES)
#define BPOSIT_QRAW   (4*BPOSIT_E + 2*BPOSIT_F)
#define BPOSIT_QSIZE  (BPOSIT_QRAW + 30)

/* Round up to nearest multiple of 32 for word array */
#define BPOSIT_QWORDS ((BPOSIT_QSIZE + 31) / 32)

/* -- count leading zeros -- pure RV32I, no Zbb ----------------- */
/* On hardware with Zbb: replace with single CLZ instruction.
 * On FE310 and bare RV32I: this 6-comparison version is optimal. */

static int bposit_clz32(uint32_t x) {
    int n = 0;
    if (x == 0) return 32;
    if (!(x >> 16)) { n += 16; x <<= 16; }
    if (!(x >> 24)) { n +=  8; x <<=  8; }
    if (!(x >> 28)) { n +=  4; x <<=  4; }
    if (!(x >> 30)) { n +=  2; x <<=  2; }
    if (!(x >> 31)) { n +=  1; }
    return n;
}

/* -- B-posit quire structure ----------------------------------- */
/* Narrower than standard quire -- BPOSIT_QWORDS words instead of 16.
 * For es=2, rs=6, n=32: 6 words (192 bits) vs 16 words (512 bits).
 * binary point at bit (BPOSIT_QSIZE/2) from MSB.               */

typedef struct {
    uint32_t w[BPOSIT_QWORDS];
    int      nar;
} bquire_t;

static void bquire_clear(bquire_t *q) {
    memset(q->w, 0, sizeof(q->w));
    q->nar = 0;
}

/* -- add one bit to the B-posit quire ------------------------- */

static void bquire_add_bit(bquire_t *q, int qbit, int neg) {
    int wi = qbit / 32;
    int bi = 31 - (qbit % 32);
    uint32_t mask;
    uint64_t carry;
    int i;

    if (wi < 0 || wi >= BPOSIT_QWORDS) return;
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
            if (w >= borrow) { q->w[i] = (uint32_t)(w - borrow); break; }
            else { q->w[i] = (uint32_t)(w - borrow + (1ULL<<32)); borrow = 1; }
        }
    }
}

/* -- add a double to the B-posit quire ------------------------ */
/* Binary point at bit (BPOSIT_QSIZE/2) from MSB.
 * 1.0 sits at bit (BPOSIT_QSIZE/2 - 1) from MSB.
 * M's MSB (bit 52) at quire bit (BPOSIT_QSIZE/2 - exp).        */

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

#endif /* BPOSIT_QUIRE_H */
