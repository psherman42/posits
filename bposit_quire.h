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

/* -- B-posit range helpers (pure integer, no FPU) ------------- */

static uint32_t bposit_minpos(void)
{
    /* k=-(RS-1), e=0, f=0: (RS-1) zeros + stop-1 + ES zeros */
    return 1u << (BPOSIT_N - 1 - BPOSIT_RS);
}

static uint32_t bposit_maxpos(void)
{
    /* k=+(RS-1), e=max, f=all-ones */
    int stop = BPOSIT_N - 1 - BPOSIT_RS;
    uint32_t p;
    p  = ((1u << (BPOSIT_RS - 1)) - 1u) << (stop + 1); /* regime ones  */
    p |= ((1u << BPOSIT_ES)       - 1u) << (stop - BPOSIT_ES); /* exp ones */
    p |= (1u << (stop - BPOSIT_ES)) - 1u;               /* frac ones    */
    return p;
}

/* -- B-posit integer posit-bits API ---------------------------- */

static void bquire_add_posit(bquire_t *q, uint32_t p)
{
    uint32_t mag;
    int sign, neg, k, e, E, bit, fi, bp;

    if (p == 0x80000000u) { q->nar = 1; return; }
    if (p == 0)            return;

    sign = (int)((p >> 31) & 1u);
    neg  = sign;
    mag  = sign ? (uint32_t)(-(int32_t)p) : p;

    bit = 30;
    if ((mag >> 30) & 1u) {              /* leading 1s: k >= 0 */
        k = -1;
        while (bit >= 0 && ((mag >> bit) & 1u)) {
            k++;
            if (k >= BPOSIT_RS - 1) { bit--; break; }  /* cap at rs */
            bit--;
        }
        if (bit >= 0 && k < BPOSIT_RS - 1) bit--;      /* stop-0   */
    } else {                             /* leading 0s: k < 0  */
        k = 0;
        while (bit >= 0 && !((mag >> bit) & 1u)) {
            k--;
            if (-k >= BPOSIT_RS) { bit--; break; }     /* cap at rs */
            bit--;
        }
        if (bit >= 0 && -k < BPOSIT_RS) bit--;         /* stop-1   */
    }

    e = 0;
    if (bit >= 0) { e  = (int)((mag >> bit) & 1u) << 1; bit--; }
    if (bit >= 0) { e |= (int)((mag >> bit) & 1u);       bit--; }

    E  = (1 << BPOSIT_ES) * k + e;
    bp = BPOSIT_QSIZE / 2;              /* binary point from MSB */

    bquire_add_bit(q, bp - 1 - E, neg); /* implicit leading 1   */

    fi = 0;
    while (bit >= 0) {
        if ((mag >> bit) & 1u)
            bquire_add_bit(q, bp - E + fi, neg);
        bit--;
        fi++;
    }
}

static int bqget(const uint32_t *w, int qb, int qsize)
{
    if (qb < 0 || qb >= qsize) return 0;
    return (int)((w[qb / 32] >> (31 - qb % 32)) & 1u);
}

static uint32_t bquire_round_posit(bquire_t *q)
{
    uint32_t mag[BPOSIT_QWORDS], bits, frac, sb_mask;
    uint64_t carry;
    int sign_neg, all_zero, lead_qbit;
    int i, j, k, e, E, pos, bp;
    int rb, exp_bits, frac_bits, max_e;
    int round_qb, round_bit, sticky, sb_wi, sb_bi;
    int useed_exp = (1 << BPOSIT_ES);
    int max_k = BPOSIT_RS - 1;

    if (q->nar) return 0x80000000u;

    sign_neg = (int)((q->w[0] >> 31) & 1u);
    for (i = 0; i < BPOSIT_QWORDS; i++) mag[i] = q->w[i];
    if (sign_neg) {
        carry = 1;
        for (i = BPOSIT_QWORDS - 1; i >= 0; i--) {
            uint64_t t = (uint64_t)(~mag[i]) + carry;
            mag[i] = (uint32_t)t;
            carry  = t >> 32;
        }
    }

    all_zero = 1;
    for (i = 0; i < BPOSIT_QWORDS; i++) if (mag[i]) { all_zero = 0; break; }
    if (all_zero) return 0u;

    lead_qbit = -1;
    for (i = 0; i < BPOSIT_QWORDS && lead_qbit < 0; i++) {
        if (mag[i]) {
            for (j = 31; j >= 0; j--) {
                if ((mag[i] >> j) & 1u) {
                    lead_qbit = i * 32 + (31 - j);
                    break;
                }
            }
        }
    }

    bp = BPOSIT_QSIZE / 2;
    E  = bp - 1 - lead_qbit;

    bits = 0x7FFFFFFFu;
    if (E >  useed_exp * max_k + (useed_exp-1))
        return sign_neg ? (uint32_t)(-(int32_t)bits) : bits;
    if (E < -useed_exp * max_k)
        return sign_neg ? 0xFFFFFFFFu : 0u;

    k = (E >= 0) ? (E / useed_exp) : ((E - useed_exp + 1) / useed_exp);
    e = E - useed_exp * k;

    rb        = (k >= 0) ? k + 2 : (-k) + 1;
    if (rb        > BPOSIT_RS + 1) rb = BPOSIT_RS + 1;
    exp_bits  = BPOSIT_N - 1 - rb;
    if (exp_bits  > BPOSIT_ES) exp_bits  = BPOSIT_ES;
    if (exp_bits  < 0)         exp_bits  = 0;
    frac_bits = BPOSIT_N - 1 - rb - exp_bits;
    if (frac_bits < 0) frac_bits = 0;

    frac = 0u;
    for (i = 0; i < frac_bits; i++)
        if (bqget(mag, bp - E + i, BPOSIT_QWORDS * 32))
            frac |= 1u << (frac_bits - 1 - i);

    round_qb  = bp - E + frac_bits;
    round_bit = bqget(mag, round_qb, BPOSIT_QWORDS * 32);
    sticky    = 0;
    if (round_qb + 1 < BPOSIT_QWORDS * 32) {
        sb_wi   = (round_qb + 1) / 32;
        sb_bi   = 31 - (round_qb + 1) % 32;
        sb_mask = (sb_bi >= 31) ? 0xFFFFFFFFu : (1u << (sb_bi + 1)) - 1u;
        if (sb_wi < BPOSIT_QWORDS && (mag[sb_wi] & sb_mask)) sticky = 1;
        for (i = sb_wi + 1; i < BPOSIT_QWORDS && !sticky; i++)
            if (mag[i]) sticky = 1;
    }

    if (round_bit && (sticky || (frac & 1u))) {
        max_e = exp_bits ? (1 << exp_bits) - 1 : 0;
        frac++;
        if (frac >= (1u << frac_bits)) {
            frac = 0u;
            if (++e > max_e) {
                e = 0;
                if (++k > max_k) {
                    bits = 0x7FFFFFFFu;
                    return sign_neg ? (uint32_t)(-(int32_t)bits) : bits;
                }
                rb        = k + 2;
                if (rb > BPOSIT_RS + 1) rb = BPOSIT_RS + 1;
                exp_bits  = BPOSIT_N - 1 - rb;
                if (exp_bits > BPOSIT_ES) exp_bits = BPOSIT_ES;
                if (exp_bits < 0)         exp_bits = 0;
                frac_bits = BPOSIT_N - 1 - rb - exp_bits;
                if (frac_bits < 0) frac_bits = 0;
            }
        }
    }

    bits = 0u;
    pos  = 30;
    if (k >= 0) {
        for (i = 0; i < k + 1 && pos >= 0; i++) { bits |= 1u << pos; pos--; }
        if (pos >= 0) pos--;
    } else {
        for (i = 0; i < -k && pos >= 0; i++) pos--;
        if (pos >= 0) { bits |= 1u << pos; pos--; }
    }
    if (exp_bits >= 1 && pos >= 0) { bits |= (uint32_t)((e >> 1) & 1u) << pos; pos--; }
    if (exp_bits >= 2 && pos >= 0) { bits |= (uint32_t)(e & 1u)        << pos; pos--; }
    for (i = frac_bits - 1; i >= 0 && pos >= 0; i--, pos--) {
        bits |= (uint32_t)((frac >> i) & 1u) << pos;
    }

    if (sign_neg) bits = (uint32_t)(-(int32_t)bits);

    return bits;
}
#endif /* BPOSIT_QUIRE_H */
