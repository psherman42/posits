#ifndef POSIT_Q16_H
#define POSIT_Q16_H
#include "positenc.h"

/* posit32 to Q16 fixed-point -- integer only, no double
 * Layer 1 of Tapered Pong integer-only build.
 * 2026-11-13 pds/claude                                        */
static int32_t posit_to_q16(uint32_t p) {
    int k, e, runlen;
    uint32_t bits;
    int32_t val;
    int frac_bits, shift, total_shift;
    uint32_t frac_field, mantissa;

    if (p == 0) return 0;
    if ((int32_t)p < 0) return 0;

    bits = p << 1;
    if (bits & 0x80000000u) {
        k = -1;
        while (bits & 0x80000000u) { k++; bits <<= 1; }
        bits <<= 1;
        runlen = k + 2;
    } else {
        k = 0;
        while (!(bits & 0x80000000u)) { k--; bits <<= 1; }
        bits <<= 1;
        runlen = -k + 1;
    }

    e = (int)(bits >> 30) & 0x3;
    bits <<= 2;

    frac_bits = _nbits - 1 - (runlen + 1) - _es;
    if (frac_bits < 0) frac_bits = 0;
    if (frac_bits > 28) frac_bits = 28;

    shift = -4*k - e;
    frac_field = (frac_bits > 0) ? bits >> (32 - frac_bits) : 0;
    mantissa = (1u << frac_bits) | frac_field;

    total_shift = frac_bits + shift - 16;
    if (total_shift >= 32) val = 0;
    else if (total_shift >= 0)
        val = (int32_t)(mantissa >> total_shift);
    else if (-total_shift < 32)
        val = (int32_t)(mantissa << (-total_shift));
    else val = 0;

    if (val < 0) val = 0;
    return val;
}
#endif /* POSIT_Q16_H */
