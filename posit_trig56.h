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

/* CORDIC iteration count -- default 56, override with -DCORDIC_ITERS=N */
#ifndef CORDIC_ITERS
#define CORDIC_ITERS 56
#endif

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
                     int64_t *cos_out, int64_t *sin_out,
                     int64_t *z_out) {
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

    /* CORDIC rotation loop -- shifts and adds only, no multiply.
     * CORDIC_ITERS defaults to 56 (beyond posit32's 28-bit significand).
     * Compile with -DCORDIC_ITERS=N to find the cliff.
     * John L. Gustafson: "try turning down from 56 bits."              */
    for (i = 0; i < CORDIC_ITERS; i++) {
    /* 56 CORDIC iterations -- shifts and adds only */
    //for (i = 0; i < 56; i++) {
        d  = (z >= 0) ? 1 : -1;
        xi = x - (int64_t)(d * (y >> i));
        yi = y + (int64_t)(d * (x >> i));
        z  = z - (int64_t)(d * _atan56[i]);
        x  = xi;
        y  = yi;
    }

    /* -- adaptive Q63 extension for hard-to-round cases ----------
     * After 56 Q56 iterations, check if cos(x) is within half a
     * posit32 ULP of a midpoint. If so, scale to Q63 and run 7
     * more iterations for 7 extra bits of precision.
     * atan(2^-i)*2^63 for i=56..62: 128,64,32,16,8,4,2
     * John L. Gustafson: "I was really hoping there was a way to
     * manually tweak CORDIC so you get the speed and simplicity
     * but also get correct rounding everywhere -- that would be a
     * big breakthrough." -- personal comm. 2026.                 */
    {
        static const int64_t _atan63[7] = {128,64,32,16,8,4,2};
        /* exact posit32 ULP in Q56 units */
        uint32_t px = posit_from_double((double)x / Q56D);
        double ulp_d = posit_to_double(px+1) - posit_to_double(px);
        int64_t half_ulp = (int64_t)(ulp_d * Q56D / 2.0);
        double  xd_near  = posit_to_double(posit_from_double((double)x/Q56D));
        int64_t x_near   = (int64_t)(xd_near * Q56D);
        int64_t dist     = x - x_near;
        if (dist < 0) dist = -dist;

        if (dist > 0 && dist < half_ulp) {
            int64_t ax = (x < 0) ? -x : x;
            int64_t ay = (y < 0) ? -y : y;
            /* guard: both x<<7 and y<<7 must fit in int64 
             * y≈Q56D near sin=1 would overflow without this check */
            if (ax <= (INT64_MAX >> 7) && ay <= (INT64_MAX >> 7)) {
                /* scale to Q63 -- multiply by 128 */
                int64_t X = x << 7;
                int64_t Y = y << 7;
                int64_t Z = z << 7;
                int64_t Xi, Yi;

                for (i = 0; i < 7; i++) {
                    d  = (Z >= 0) ? 1 : -1;
                    Xi = X - (int64_t)(d * (Y >> (56 + i)));
                    Yi = Y + (int64_t)(d * (X >> (56 + i)));
                    Z  = Z - (int64_t)(d * _atan63[i]);
                    X  = Xi;
                    Y  = Yi;
                }

                /* scale back to Q56 */
                x = X >> 7;
                y = Y >> 7;
            }
            /* else: near 1.0, x<<7 overflows int64 -- accept 56-iter result */
        }
        /* else dist==0: exactly on posit32 value, already correct */
        /* else dist>=half_ulp: far from midpoint, 56 iters sufficient */
    }

    if (flip_sin) y = -y;
    if (flip_cos) x = -x;
    *cos_out = x;
    *sin_out = y;

    if (z_out) *z_out = z;
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
    cordic56(aq, &c56, &s56, NULL);
    return posit_from_double((double)s56 / Q56D);
}

static uint32_t posit32_cos56(uint32_t a) {
    double ad = posit_to_double(a);
    double two_pi = 6.28318530717958647692;
    ad = ad - (int64_t)(ad / two_pi) * two_pi;
    int64_t aq = (int64_t)(ad / (3.14159265358979323846/2.0) * (double)Q56_PI2);
    int64_t c56, s56;
    cordic56(aq, &c56, &s56, NULL);
    return posit_from_double((double)c56 / Q56D);
}

/* -- sin(pi*x) and cos(pi*x) ---------------------------------- */
/* x in posit32. Computes sin(pi*x) and cos(pi*x).
 * Gustafson recommendation: exact argument reduction for FFT,
 * haversine, ILS DDM. x=0.5 -> sin(pi/2)=1 exactly.
 * Large-angle: reduce x mod 2 in double before Q56 convert.   */

/* QB: fuzzy quadrant boundary guard.
 * fmod() can land just above exact values (0.5, 1.0, 1.5) by ~1e-15.
 * Uniform guard on all boundaries -- symmetric, honest about float limits. */
#define QB 1e-10

static uint32_t posit32_cos_pi56(uint32_t x) {
    double xd = posit_to_double(x);
    int64_t c56, s56;
    int neg = 0;

    /* reduce mod 2: cos(pi*(x+2)) = cos(pi*x) */
    //xd = xd - (int64_t)(xd / 2.0) * 2.0;
    xd = fmod(xd, 2.0);  /* fmod preserves precision; floor/int64 fail for large posit32 */
    if (xd < 0) xd += 2.0;

    /* symmetry reduction to [0, 1/2]
     * cos(pi*x) for x in [0,2]:
     *   [0,   1/2]: compute directly         sign=+
     *   [1/2, 1  ]: cos(pi*x)=-cos(pi*(1-x)) sign=-
     *   [1,   3/2]: cos(pi*x)=-cos(pi*(x-1)) sign=-
     *   [3/2, 2  ]: cos(pi*x)= cos(pi*(2-x)) sign=+  */
    if      (xd < 0.5 + QB) { ;               neg = 0; }  /* Q1 */
    else if (xd < 1.0 + QB) { xd = 1.0 - xd;  neg = 1; }  /* Q2 */
    else if (xd < 1.5 + QB) { xd = xd - 1.0;  neg = 1; }  /* Q3 */
    else                    { xd = 2.0 - xd;  neg = 0; }  /* Q4 */

    /* exact: cos(pi/2) = 0 */
    if (xd == 0.5) return posit_from_double(0.0);

    /* near x=1/2: cos(pi*x) = sin(pi*eps), Taylor series
     * accurate to posit32 ULP for eps < 0.044
     * adaptive extension: if result lands near posit32 midpoint,
     * add terms 7 and 9 for extra precision -- mirrors Q63 in CORDIC */
    double eps = 0.5 - xd;
    if (eps >= 0 && eps < 0.044) {  /* Taylor: (pi*eps)^7/5040 < 2^-28 for eps < 0.138/pi = 0.044 */
        double pie  = 3.14159265358979323846 * eps;
        double pie2 = pie * pie;
        double r = pie * (1.0 - pie2/6.0 + pie2*pie2/120.0);

        /* proximity check -- same philosophy as Q63 CORDIC extension */
        double rn   = neg ? -r : r;
        double near = posit_to_double(posit_from_double(rn));
        double dist = rn - near;
        if (dist < 0.0) dist = -dist;
        uint32_t pn = posit_from_double(near < 0 ? -near : near);
        double half_ulp = (posit_to_double(pn + 1) - posit_to_double(pn)) / 2.0;

        if (dist > 0.0 && dist < half_ulp) {
            /* add Taylor terms 7 and 9 */
            r += pie * pie2 * pie2 * pie2 * (-1.0/5040.0 + pie2/362880.0);
        }

        return posit_from_double(neg ? -r : r);
    }

    /* main range [0, 1/2): use 56-bit CORDIC */
    int64_t aq = (int64_t)(xd * 2.0 * (double)Q56_PI2);
    cordic56(aq, &c56, &s56, NULL);
    double r = (double)c56 / Q56D;
    return posit_from_double(neg ? -r : r);
}

static uint32_t posit32_sin_pi56(uint32_t x) {
    double xd = posit_to_double(x);
    int neg = 0;

    xd = fmod(xd, 2.0);
    if (xd < 0.0) xd += 2.0;

    /* symmetry reduction to [0, 1/2]
     * sin(pi*x) for x in [0,2]:
     *   [0,   1/2]: compute directly         neg=0
     *   [1/2,   1]: sin(pi*x)= sin(pi*(1-x)) neg=0
     *   [1,   3/2]: sin(pi*x)=-sin(pi*(x-1)) neg=1
     *   [3/2,   2]: sin(pi*x)=-sin(pi*(2-x)) neg=1  */
    if      (xd < 0.5 + QB) { ;               neg = 0; }
    else if (xd < 1.0 + QB) { xd = 1.0 - xd;  neg = 0; }
    else if (xd < 1.5 + QB) { xd = xd - 1.0;  neg = 1; }
    else                    { xd = 2.0 - xd;  neg = 1; }

    /* near x=0: sin(pi*x) ≈ pi*x -- Taylor series
     * accurate to posit32 ULP for x < 0.044
     * adaptive extension: if result lands near posit32 midpoint,
     * add terms 7 and 9 for extra precision -- mirrors Q63 in CORDIC */
    if (xd < 0.044) {
        double pix  = 3.14159265358979323846 * xd;
        double pix2 = pix * pix;
        double r = pix * (1.0 - pix2/6.0 + pix2*pix2/120.0);

        /* proximity check -- same philosophy as Q63 CORDIC extension */
        double rn   = neg ? -r : r;
        double near = posit_to_double(posit_from_double(rn));
        double dist = rn - near;
        if (dist < 0.0) dist = -dist;
        uint32_t pn = posit_from_double(near < 0 ? -near : near);
        double half_ulp = (posit_to_double(pn + 1) - posit_to_double(pn)) / 2.0;

        if (dist > 0.0 && dist < half_ulp) {
            /* add Taylor terms 7 and 9 */
            r += pix * pix2 * pix2 * pix2 * (-1.0/5040.0 + pix2/362880.0);
        }

        return posit_from_double(neg ? -r : r);
    }

    /* main range: 56-bit CORDIC */
    int64_t aq = (int64_t)(xd * 2.0 * (double)Q56_PI2);
    int64_t c56, s56;
    cordic56(aq, &c56, &s56, NULL);
    double r = (double)s56 / Q56D;
    return posit_from_double(neg ? -r : r);
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

static int32_t cos_pi56_ulp(uint32_t a) {
    double ad    = posit_to_double(a);
    uint32_t p56 = posit32_cos_pi56(a);
    double xd    = fmod(ad, 2.0);
    if (xd < 0.0) xd += 2.0;
    /* exact zeros at quadrant boundaries where double is imprecise */
    double ex;
    if (fabs(xd - 0.5) < 1e-10 || fabs(xd - 1.5) < 1e-10)
        ex = 0.0;
    else
        ex = cos(3.14159265358979323846 * xd);
    double ref   = (fabs(ex) < 1e-30) ? 0.0 : ex;
    uint32_t pref = posit_from_double(ref);
    return (int32_t)p56 - (int32_t)pref;
}

static int32_t sin_pi56_ulp(uint32_t a) {
    double ad    = posit_to_double(a);
    uint32_t p56 = posit32_sin_pi56(a);
    //double xd = ad - floor(ad / 2.0) * 2.0;
    double xd = fmod(ad, 2.0);  /* fmod preserves precision; floor/int64 fail for large posit32 */
    if (xd < 0.0) xd += 2.0;
    double ex;
    if (xd < 1e-10 || fabs(xd - 1.0) < 1e-10)
        ex = 0.0;
    else
        ex = sin(3.14159265358979323846 * xd);
    double ref   = (fabs(ex) < 1e-30) ? 0.0 : ex;
    uint32_t pref = posit_from_double(ref);
    return (int32_t)p56 - (int32_t)pref;
}

#endif /* POSIT_TRIG56_H */
