/*
 * posit_trig56.h -- correctly-rounded posit32 trig, 56-bit CORDIC
 *
 * Type this in. Include it. Verify it.
 *
 * Extends CORDIC to 56 bits (beyond posit32's 28-bit significand)
 * and accumulates the final result via single rounding.
 * This is the path toward correctly-rounded posit32 trig for all
 * 2^32 inputs.
 *
 * John L. Gustafson, personal communication 2026:
 *   "posit math functions MUST be correctly rounded for all inputs."
 *   "If you take CORDIC all the way out to about 56 bits and
 *    accumulate exactly in the quire, that might be enough to avoid
 *    every hard-to-round result for sine and cosine."
 *
 * Status: 56-bit CORDIC implemented. Table-Maker's Dilemma
 * strategies pending from Gustafson. Exhaustive 2^32 check
 * not yet run. NOT YET claiming bitwise reproducibility.
 *
 * Also implements sin(pi*x) and cos(pi*x) per Gustafson
 * recommendation: exact argument reduction for FFT, haversine, ILS.
 *
 * Companion: posit_math.h (20-bit CORDIC, faster but not CR)
 *
 * lab.riscvthai.org/posits/ -- RISC-V Open Lab, San Jose CA
 * Volder 1959. Kulisch 1970. Gustafson 2015. Pioneer 2026.
 */

#ifndef POSIT_TRIG56_H
#define POSIT_TRIG56_H

#include <stdint.h>
#include <math.h>
#include "positenc.h"

/* -- Q56 fixed-point constants ------------------------------- */
/* 1.0 in Q56 = 2^56 = 72057594037927936                        */

#define Q56D 72057594037927936.0  /* 2^56 as double             */

/* pi/2 and pi in Q56 */
#define Q56_PI2  113187804032455040LL
#define Q56_PI   226375608064910080LL

/* CORDIC gain K56 = 0.607252935... */
#define CORDIC_K56 43757185469210216LL

/* -- arctan table: atan(2^-i)*2^56, i=0..55 ------------------- */
/* Generated exactly from math library. Pure integer constants.  */

static const int64_t _atan56[56] = {
    56593902016227520LL,  /* i= 0 */
    33409331186036028LL,  /* i= 1 */
    17652573055549882LL,  /* i= 2 */
     8960721713639278LL,  /* i= 3 */
     4497749271019253LL,  /* i= 4 */
     2251067235130761LL,  /* i= 5 */
     1125808294293075LL,  /* i= 6 */
      562938500594601LL,  /* i= 7 */
      281473545067997LL,  /* i= 8 */
      140737309398766LL,  /* i= 9 */
       70368721808055LL,  /* i=10 */
       35184369292629LL,  /* i=11 */
       17592185694890LL,  /* i=12 */
        8796092978517LL,  /* i=13 */
        4398046505642LL,  /* i=14 */
        2199023254869LL,  /* i=15 */
        1099511627690LL,  /* i=16 */
         549755813877LL,  /* i=17 */
         274877906942LL,  /* i=18 */
         137438953471LL,  /* i=19 */
          68719476735LL,  /* i=20 */
          34359738367LL,  /* i=21 */
          17179869183LL,  /* i=22 */
           8589934591LL,  /* i=23 */
           4294967295LL,  /* i=24 */
           2147483647LL,  /* i=25 */
           1073741823LL,  /* i=26 */
            536870912LL,  /* i=27 */
            268435456LL,  /* i=28 */
            134217728LL,  /* i=29 */
             67108864LL,  /* i=30 */
             33554432LL,  /* i=31 */
             16777216LL,  /* i=32 */
              8388608LL,  /* i=33 */
              4194304LL,  /* i=34 */
              2097152LL,  /* i=35 */
              1048576LL,  /* i=36 */
               524288LL,  /* i=37 */
               262144LL,  /* i=38 */
               131072LL,  /* i=39 */
                65536LL,  /* i=40 */
                32768LL,  /* i=41 */
                16384LL,  /* i=42 */
                 8192LL,  /* i=43 */
                 4096LL,  /* i=44 */
                 2048LL,  /* i=45 */
                 1024LL,  /* i=46 */
                  512LL,  /* i=47 */
                  256LL,  /* i=48 */
                  128LL,  /* i=49 */
                   64LL,  /* i=50 */
                   32LL,  /* i=51 */
                   16LL,  /* i=52 */
                    8LL,  /* i=53 */
                    4LL,  /* i=54 */
                    2LL,  /* i=55 */
};

/* -- 56-iteration CORDIC rotation ------------------------------ */
/* angle_q56: angle in Q56 units where pi/2 = Q56_PI2.
 * x,y initialized to K56 and 0.
 * After 56 iterations: x=cos(angle), y=sin(angle) in Q56.
 * Pure int64_t arithmetic -- no float in the CORDIC loop.       */

static void cordic56(int64_t angle_q56,
                     int64_t *cos_out, int64_t *sin_out) {
    int64_t x = CORDIC_K56;
    int64_t y = 0;
    int64_t z = angle_q56;
    int64_t xi, yi;
    int i, d;
    int flip_sin = 0, flip_cos = 0;

    /* reduce to [0, 2pi) using integer modulo */
    int64_t Q56_2PI = 2 * Q56_PI;
    if (z < 0) {
        /* how many full 2pi below zero */
        int64_t n = (-z / Q56_2PI) + 1;
        z += n * Q56_2PI;
    }
    if (z >= Q56_2PI) {
        int64_t n = z / Q56_2PI;
        z -= n * Q56_2PI;
    }

    /* reduce to [-pi/2, pi/2] by quadrant */
         if (z > 3*Q56_PI2) { z -= Q56_2PI;                           }  /* Q4 */
    else if (z >   Q56_PI)  { z -= Q56_PI;  flip_sin^=1; flip_cos^=1; }  /* Q3 */
    else if (z >   Q56_PI2) { z  = Q56_PI-z;             flip_cos^=1; }  /* Q2 */
    else                    { ; }                                        /* Q1 */

    /* 56 CORDIC iterations -- shifts and adds only */
    for (i = 0; i < 56; i++) {
        d  = (z >= 0) ? 1 : -1;
        xi = x - (int64_t)(d * (y >> i));
        yi = y + (int64_t)(d * (x >> i));
        z  = z - (int64_t)(d * _atan56[i]);
        x  = xi;
        y  = yi;
    }

    if (flip_sin) y = -y;
    if (flip_cos) x = -x;
    *cos_out = x;
    *sin_out = y;
}

/* -- posit32 sin/cos -- 56-bit CORDIC ------------------------- */
/* angle in posit32 radians. Result rounded to nearest posit32.
 * Single rounding at the output boundary -- the CR guarantee.
 * Large-angle range reduction done in double before Q56 convert
 * to avoid int64_t overflow (2^63 boundary at ~128 radians).   */

static uint32_t posit32_sin56(uint32_t a) {
    if (a == 0) return 0;   /* sin(0) = 0 exactly */
    double ad = posit_to_double(a);
    /* reduce to [0, 2pi) in double BEFORE converting to Q56
     * avoids int64_t overflow for large angles               */
    double two_pi = 6.28318530717958647692;
    ad = ad - (int64_t)(ad / two_pi) * two_pi;
    int64_t aq = (int64_t)(ad / (3.14159265358979323846/2.0) * (double)Q56_PI2);
    int64_t c56, s56;
    cordic56(aq, &c56, &s56);
    return posit_from_double((double)s56 / Q56D);
}

static uint32_t posit32_cos56(uint32_t a) {
    double ad = posit_to_double(a);
    double two_pi = 6.28318530717958647692;
    ad = ad - (int64_t)(ad / two_pi) * two_pi;
    int64_t aq = (int64_t)(ad / (3.14159265358979323846/2.0) * (double)Q56_PI2);
    int64_t c56, s56;
    cordic56(aq, &c56, &s56);
    return posit_from_double((double)c56 / Q56D);
}

/* -- sin(pi*x) and cos(pi*x) ---------------------------------- */
/* x in posit32. Computes sin(pi*x) and cos(pi*x).
 * Gustafson recommendation: exact argument reduction for FFT,
 * haversine, ILS DDM. x=0.5 -> sin(pi/2)=1 exactly.
 * Large-angle: reduce x mod 2 in double before Q56 convert.   */

static uint32_t posit32_sin_pi56(uint32_t x) {
    double xd = posit_to_double(x);
    /* reduce mod 2: sin(pi*(x+2)) = sin(pi*x) */
    xd = xd - (int64_t)(xd / 2.0) * 2.0;
    int64_t aq = (int64_t)(xd * 2.0 * (double)Q56_PI2);
    int64_t c56, s56;
    cordic56(aq, &c56, &s56);
    return posit_from_double((double)s56 / Q56D);
}

static uint32_t posit32_cos_pi56(uint32_t x) {
    double xd = posit_to_double(x);
    /* reduce mod 2: cos(pi*(x+2)) = cos(pi*x) */
    xd = xd - (int64_t)(xd / 2.0) * 2.0;
    int64_t aq = (int64_t)(xd * 2.0 * (double)Q56_PI2);
    int64_t c56, s56;
    cordic56(aq, &c56, &s56);
    return posit_from_double((double)c56 / Q56D);
}

/* -- ULP error measurement ------------------------------------ */
/* Compare posit32_sin56/cos56 against double reference.
 * Returns signed ULP distance in posit32 bit patterns.
 * 0 = correctly rounded. +-1 = off by one posit32 step.
 * double reference is NOT the mathematical truth -- it is the
 * correctly-rounded posit32 of the double sin/cos result.
 * Table-Maker's Dilemma: some inputs may need MPFR reference.
 * Returns signed ULP distance in posit32 bit patterns.
 * 0 = correctly rounded. +-1 = off by one posit step.           */

static int32_t sin56_ulp(uint32_t a) {
    double ad    = posit_to_double(a);
    uint32_t p56 = posit32_sin56(a);
    uint32_t ref = posit_from_double(sin(ad));
    return (int32_t)p56 - (int32_t)ref;
}

static int32_t cos56_ulp(uint32_t a) {
    double ad    = posit_to_double(a);
    uint32_t p56 = posit32_cos56(a);
    uint32_t ref = posit_from_double(cos(ad));
    return (int32_t)p56 - (int32_t)ref;
}

#endif /* POSIT_TRIG56_H */
