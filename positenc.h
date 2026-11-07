/*
 * positenc.h -- posit arithmetic encoding primitives
 *
 * Type this in. Include it. Understand posits from first principles.
 *
 * A faithful C89 port of John L. Gustafson's Mathematica notebook:
 * "Inverting Hilbert Matrices with Posit Arithmetic" (2018)
 * DARPA TRADES Program, Contract #HR0011-17-9-0007
 *
 * Full attribution: every algorithm here is Gustafson's.
 * This port: Paul Sherman, RISC-V Open Lab, San Jose CA, 2026.
 *
 * Posit format: n-bit encoding with es exponent bits.
 * Fields from MSB: sign (1) | regime (variable) | exponent (es) | fraction
 *
 * No FPU required. No F or D RISC-V extension.
 * Runs on bare RV32I in integer arithmetic.
 *
 * Companion to microuniversal.h -- the MAC abstraction.
 * Stack: positenc.h -> microuniversal.h -> hilbert_ldl.c
 *
 * lab.riscvthai.org/posits/ -- RISC-V Open Lab
 */

#ifndef POSITENC_H
#define POSITENC_H

#include <stdint.h>
#include <math.h>

/* -- environment ----------------------------------------------- */
/* Call positenv_init(nbits, es) before any other function.
 * Gustafson: setpositenv[{n, e}]                                */

static int    _nbits;    /* total bits in posit */
static int    _es;       /* exponent field size */
static int    _npat;     /* 2^nbits: total patterns */
static int    _NaR;      /* Not-a-Real: npat/2 */
static int    _pmask;    /* npat - 1 */
static int    _esizep;   /* 2^es */
static double _useed;    /* 2^(2^es): the scaling base */
static double _minpos;   /* smallest positive posit */
static double _maxpos;   /* largest positive posit */
static int    _qsize;    /* quire size in bits */

static void positenv_init(int nbits, int es) {
    int k;
    _nbits  = nbits;
    _es     = es;

    /* nbits=32 special case: 1<<32 is undefined behavior in C (overflow).
     * NaR = 0x80000000, pmask = 0xFFFFFFFF handled via unsigned arithmetic.
     * _npat is unused for nbits=32 since all uint32_t values are valid posits
     * except NaR (0x80000000) and 0. */
    _NaR    = (int)(1u << (nbits - 1));
    _npat   = (nbits < 32) ? (1 << nbits) : 0; /* 2^32 overflows int; 0 = sentinel for nbits=32 */
    _pmask  = (int)(0xFFFFFFFFu >> (32 - nbits));
    _esizep = 1 << es;
    _useed  = 1.0;

    for (k = 0; k < _esizep; k++) _useed *= 2.0;   /* 2^(2^es) */
    /* Gustafson: minpos = useed^(-nbits+2), maxpos = useed^(nbits-2)
     * For posit<8,0>: useed=2, minpos=2^-6=0.015625, maxpos=2^6=64
     * The posit number line spans [minpos, maxpos] symmetrically     */
    _minpos = pow(_useed, -(nbits - 2));
    _maxpos = pow(_useed,  (nbits - 2));

    /* quire size: smallest power of 2 >= (nbits-2)*2^(es+2) + 5 */
    {
        int need = (nbits - 2) * (1 << (es + 2)) + 5;
        _qsize = 1;
        while (_qsize < need) _qsize <<= 1;
    }
}

/* -- validity ------------------------------------------------ */
/* Gustafson: positQ[p] := 0 <= p < npat                        */

static int posit_valid(uint32_t p) {
    return (int)p < _npat;
}

/* -- two's complement ----------------------------------------- */
/* Gustafson: twoscomp[sign, p] := Mod[If[sign>0, p, npat-p], npat]
 * sign > 0: return p unchanged
 * sign <= 0: negate (two's complement)                          */

static uint32_t posit_twoscomp(int sign, uint32_t p) {
    if (sign > 0) return p & _pmask;
    return (uint32_t)((_npat - (int)p) & _pmask);
}

/* -- sign bit ------------------------------------------------- */
/* Gustafson: signbit[p] := IntegerDigits[p,2,nbits][[1]]
 * MSB of the nbits-wide representation                          */

static int posit_signbit(uint32_t p) {
    return (int)((p >> (_nbits - 1)) & 1);
}

/* -- regime ------------------------------------------------- */
/* Gustafson: regimebits[p]
 * After two's complementing by sign, extract the run of
 * identical bits starting at bit position 2 (after sign bit).
 * Returns regime value k and run length in *runlen.
 *
 * Regime encoding:
 *   k >= 0: k+1 ones followed by a zero  -> value k
 *   k <  0: |k| zeros followed by a one  -> value k           */

static int posit_regime(uint32_t p, int *runlen) {
    uint32_t q;
    int bits[64], bit2, i, run, k;

    q = posit_twoscomp(1 - posit_signbit(p), p);

    /* extract nbits bits of q */
    for (i = _nbits - 1; i >= 0; i--)
        bits[_nbits - 1 - i] = (int)((q >> i) & 1);

    bit2 = bits[1];   /* first bit after sign */
    run  = 0;
    for (i = 1; i < _nbits; i++) {
        if (bits[i] == bit2) run++;
        else break;
    }

    *runlen = run;
    k = (bit2 == 1) ? run - 1 : -run;
    return k;
}

/* -- exponent field ------------------------------------------ */
/* Gustafson: exponentbits[p]
 * Starts after sign (1) + regime (runlen+1) bits.
 * Up to es bits; zero-padded on right if clipped.              */

static int posit_exponent(uint32_t p) {
    uint32_t q;
    int runlen, startbit, i, e, bits[64];

    posit_regime(p, &runlen);
    q = posit_twoscomp(1 - posit_signbit(p), p);
    startbit = runlen + 2;   /* 1 sign + runlen + 1 terminator */

    for (i = _nbits - 1; i >= 0; i--)
        bits[_nbits - 1 - i] = (int)((q >> i) & 1);

    e = 0;
    for (i = 0; i < _es; i++) {
        int pos = startbit + i;
        e = (e << 1) | (pos < _nbits ? bits[pos] : 0);
    }
    return e;
}

/* -- fraction field ------------------------------------------ */
/* Gustafson: fractionbits[p]
 * Starts after sign + regime + es bits.
 * Returns fraction as double in [0, 1).                        */

static double posit_fraction(uint32_t p) {
    uint32_t q;
    int runlen, startbit, i, bits[64];
    double f;

    posit_regime(p, &runlen);
    q = posit_twoscomp(1 - posit_signbit(p), p);
    startbit = runlen + 2 + _es;

    for (i = _nbits - 1; i >= 0; i--)
        bits[_nbits - 1 - i] = (int)((q >> i) & 1);

    f = 0.0;
    for (i = startbit; i < _nbits; i++)
        f = f * 2.0 + bits[i];
    for (i = startbit; i < _nbits; i++)
        f /= 2.0;

    return f;
}

/* -- posit to double ---------------------------------------- */
/* Gustafson: p2x[p]
 * Decode a posit bit pattern to its real value.
 * p == 0:    zero
 * p == NaR:  not-a-real (return NaN)
 * otherwise: (-1)^s * useed^k * 2^e * (1 + f)                 */

static double posit_to_double(uint32_t p) {
    int s, k, e, runlen;
    double f, val;

    if (p == 0)              return 0.0;
    if ((int)p == _NaR)      return 0.0 / 0.0;  /* NaN */

    s = posit_signbit(p);
    k = posit_regime(p, &runlen);
    e = posit_exponent(p);
    f = posit_fraction(p);

    val = pow(_useed, k) * pow(2.0, e) * (1.0 + f);
    return s ? -val : val;
}

/* -- double to posit ----------------------------------------- */
/* Gustafson: x2p[x]
 * Encode a real value as the nearest posit.
 * Uses iterative shift-and-compare (Gustafson's algorithm).    */

static uint32_t posit_from_double(double x) {
    int s, i;
    uint32_t p;  /* must be unsigned -- int overflows during bit assembly
                  * for nbits=32; signed shift of MSB is undefined behavior */
    double y, e;

    if (x != x)             return (uint32_t)_NaR;  /* NaN -> NaR */
    if (x == 0.0)           return 0;
    if (x >= _maxpos * 2.0) return (uint32_t)(_NaR - 1); /* maxpos */
    if (x <= -_maxpos * 2.0)return (uint32_t)(_NaR + 1); /* -maxpos */

    s = (x < 0) ? 1 : 0;
    y = fabs(x);
    if (y > _maxpos) y = _maxpos;
    if (y < _minpos) y = _minpos;

    if (y >= 1.0) {
        p = 1; i = 2;
        while (y >= _useed && i < _nbits) {
            p = 2 * p + 1; y /= _useed; i++;
        }
        p = 2 * p; i++;
    } else {
        p = 0; i = 1;
        while (y < 1.0 && i <= _nbits) {
            y *= _useed; i++;
        }
        if (i >= _nbits) { p = 2; i = _nbits + 1; }
        else { p = 1; i++; }
    }

    /* exponent bits */
    e = (double)(1 << (_es - 1 > 0 ? _es - 1 : 0));
    while (e > 0.5 && i <= _nbits) {
        p = 2 * p;
        if (y >= pow(2.0, e)) { y /= pow(2.0, e); p++; }
        e /= 2.0; i++;
    }

    /* fraction bits */
    y -= 1.0;
    while (y > 0.0 && i <= _nbits) {
        y *= 2.0;
        p = 2 * p + (int)y;
        y -= (int)y;
        i++;
    }

    /* align and round */
    p <<= (_nbits + 1 - i);
    i++;
    {
        int last   = p & 1;
        p >>= 1;
        p += last & (p & 1 ? 1 : (y > 0.0 ? 1 : 0));
    }

    /* Two's complement negation for negative posits.
     * Cast to int32_t first to get arithmetic negation,
     * then mask to nbits. For nbits=32, _pmask=0xFFFFFFFF (no-op mask).
     * Gustafson: Mod[npat - p, npat] */
    return s ? (uint32_t)((-(int32_t)p) & _pmask) : (uint32_t)(p & _pmask);
}

#endif /* POSITENC_H */
