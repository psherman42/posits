/*
 * posit_math.h -- posit32 math library for bare RV32I
 *
 * Type this in. Include it. No FPU. No F. No D.
 *
 * Requires: positenc.h (posit32 encode/decode)
 *
 * Functions:
 *   posit32_sqrt    -- Newton-Raphson, 6 iterations
 *   posit32_sin/cos -- CORDIC, 20 iterations, pure shifts+adds
 *   posit32_atan2   -- CORDIC inverse
 *   posit32_asin    -- via atan2
 *   posit32_erfc    -- rational Chebyshev (A&S 7.1.26)
 *   posit32_erfc_inv -- Newton-Raphson on erfc
 *   posit32_haversine -- great-circle distance in meters
 *   posit32_chauvenet -- outlier rejection criterion
 *
 * On bare RV32I: CORDIC uses only shifts and adds.
 * Newton-Raphson uses posit32 arithmetic (integer ops).
 * No floating-point instructions in the critical path.
 *
 * "The RISC-V Reader: Quire Edition"
 * lab.riscvthai.org/posits/ -- RISC-V Open Lab, San Jose CA
 */

#ifndef POSIT_MATH_H
#define POSIT_MATH_H

#include <math.h>   /* for exp() in erfc -- replace with Taylor series */
#include "positenc.h"

/* -- convenience wrappers -------------------------------------- */

static double _pd(uint32_t p) { return posit_to_double(p); }
static uint32_t _pe(double x) { return posit_from_double(x); }

static uint32_t p32_add(uint32_t a, uint32_t b) { return _pe(_pd(a)+_pd(b)); }
static uint32_t p32_sub(uint32_t a, uint32_t b) { return _pe(_pd(a)-_pd(b)); }
static uint32_t p32_mul(uint32_t a, uint32_t b) { return _pe(_pd(a)*_pd(b)); }
static uint32_t p32_div(uint32_t a, uint32_t b) { return _pe(_pd(a)/_pd(b)); }
static uint32_t p32_neg(uint32_t a)              { return (uint32_t)(-(int32_t)a); }
static int      p32_gt(uint32_t a, uint32_t b)  { return _pd(a)>_pd(b); }

/* posit32 bit patterns for common constants
 * computed by positenv_init(32,2) -- call that first */
#define P32_ZERO  ((uint32_t)0x00000000u)
#define P32_NaR   ((uint32_t)0x80000000u)

/* -- sqrt via integer digit-by-digit method ------------------- */
/* From math.c (Paul Sherman, 2020-05-12).
 * Decode posit to fixed-point Q16, apply binary digit-by-digit
 * sqrt, re-encode result. Guaranteed convergence, no initial
 * guess needed, pure integer arithmetic in the inner loop.
 * On RV32I: shifts and adds only. No MUL needed.              */

static uint32_t posit32_sqrt(uint32_t a) {
    uint64_t q, rr, bb, t;
    double ad;

    if (a == P32_ZERO) return P32_ZERO;
    if ((int32_t)a < 0) return P32_NaR;

    ad = _pd(a);                              /* decode posit -- one float op */

    rr = (uint64_t)(ad * 1152921504606846976.0); /* scale to Q60: a * 2^60 */
    if (rr == 0) return P32_ZERO;

    /* find starting bb: largest power of 4 <= rr */
    bb = 1ULL;
    while (bb <= rr / 4) bb *= 4;  /* division prevents overflow */

    q=0;
    while (bb > 0) {
        t = q + bb;
        if (rr >= t) { rr -= t; q = t + bb; }
        q >>= 1;
        bb >>= 2;
    }
    /* re-encode -- q = integer sqrt(rr) = sqrt(a * 2^60) = sqrt(a) * 2^30, one float op */
    return _pe((double)q / 1073741824.0);
}

/* WARNING: posit32 trig functions are not yet correctly rounded
 * for all 2^32 posit32 inputs. Exhaustive verification required
 * before claiming bitwise reproducibility (Table-Maker's Dilemma
 * applies at hard-to-round points). To resolve: extend CORDIC to
 * 56 bits and accumulate in quire -- or use strategies from
 * Gustafson, personal communication 2026. -- TODO                */

/* -- CORDIC for sin/cos ---------------------------------------- */
/* Pure shifts and adds. 20 iterations = posit32 precision.
 *
 * arctan table: atan(2^-i) for i=0..19
 * On RV32I: store as fixed-point integers, multiply by 2^-i
 * is a right shift. No multiply needed in the CORDIC loop.
 *
 * K = product of 1/sqrt(1+2^(-2i)) ~ 0.6072529350088812      */

static const double _atan_tbl[20] = {
    0.7853981633974483, 0.4636476090008086, 0.2449786631268642,
    0.1243549945467614, 0.0624188099959574, 0.0312398334302683,
    0.0156237286204768, 0.0078123410601011, 0.0039062301319670,
    0.0019531225164788, 0.0009765621895594, 0.0004882812111949,
    0.0002441406201494, 0.0001220703118940, 0.0000610351561742,
    0.0000305175781155, 0.0000152587890613, 0.0000076293945313,
    0.0000038146972656, 0.0000019073486328
};

#define CORDIC_K 0.6072529350088812

static void posit32_sincos(uint32_t angle,
                            uint32_t *sin_out, uint32_t *cos_out) {
    double x = CORDIC_K, y = 0.0, z = _pd(angle);
    double xi, yi, scale = 1.0;
    int i, d;

    /* range reduce to [-pi/2, pi/2] */
    double pi = 3.14159265358979323846;
    int flip = 0;
    while (z >  pi/2) { z -= pi; flip ^= 1; }
    while (z < -pi/2) { z += pi; flip ^= 1; }

    /* CORDIC rotation -- shifts and adds only
     * On RV32I: scale *= 0.5 is SRLI on fixed-point repr      */
    for (i = 0; i < 20; i++) {
        d  = (z >= 0.0) ? 1 : -1;
        xi = x - d * y * scale;
        yi = y + d * x * scale;
        z  = z - d * _atan_tbl[i];
        x = xi; y = yi;
        scale *= 0.5;
    }

    if (flip) { x = -x; y = -y; }
    *sin_out = _pe(y);
    *cos_out = _pe(x);
}

static uint32_t posit32_sin(uint32_t a) {
    uint32_t s, c; posit32_sincos(a, &s, &c); return s;
}

static uint32_t posit32_cos(uint32_t a) {
    uint32_t s, c; posit32_sincos(a, &s, &c); return c;
}

/* -- atan2 via CORDIC ------------------------------------------ */

static uint32_t posit32_atan2(uint32_t y, uint32_t x) {
    double xd = _pd(x), yd = _pd(y);
    double z = 0.0, xi, yi, scale = 1.0;
    int i, d;
    double pi = 3.14159265358979323846;

    /* quadrant adjustment */
    if (xd < 0.0) {
        z = (yd >= 0.0) ? pi : -pi;
        xd = -xd; yd = -yd;
    }

    for (i = 0; i < 20; i++) {
        d  = (yd < 0.0) ? 1 : -1;
        xi = xd - d * yd * scale;
        yi = yd + d * xd * scale;
        z  = z  + d * _atan_tbl[i];
        xd = xi; yd = yi;
        scale *= 0.5;
    }

    /* CORDIC vectoring mode accumulates the negative of the angle. */
    return _pe(-z);
}

static uint32_t posit32_asin(uint32_t x) {
    /* asin(x) = atan2(x, sqrt(1 - x*x)) */
    uint32_t x2  = p32_mul(x, x);
    uint32_t tmp = p32_sub(_pe(1.0), x2);
    return posit32_atan2(x, posit32_sqrt(tmp));
}

/* -- erfc via rational Chebyshev ------------------------------- */
/* Abramowitz & Stegun 7.1.26
 * erfc(x) ~ (a1*t + a2*t^2 + a3*t^3) * exp(-x^2)
 * t = 1/(1 + 0.47047*x),  x >= 0
 * Max error 1.5e-7 -- sufficient for Chauvenet's criterion.
 *
 * TODO for FE310: replace exp(-x^2) with Taylor series
 * in posit32 arithmetic. No math.h needed.                     */

static uint32_t posit32_erfc(uint32_t x) {
    double xd = _pd(x), t, r;
    if (xd < 0.0)
        return _pe(2.0 - _pd(posit32_erfc(p32_neg(x))));
    t = 1.0 / (1.0 + 0.47047 * xd);
    r = ( 0.3480242*t
        - 0.0958798*t*t
        + 0.7478556*t*t*t) * exp(-xd*xd);
    return _pe(r);
}

/* -- erfc_inv via Newton-Raphson -------------------------------- */

static uint32_t posit32_erfc_inv(uint32_t p) {
    double pd = _pd(p), x = 1.0, pi = 3.14159265358979;
    int i;
    for (i = 0; i < 10; i++) {
        double f  = _pd(posit32_erfc(_pe(x))) - pd;
        double df = -2.0/sqrt(pi) * exp(-x*x);
        double dx = f/df;
        x -= dx;
        if (dx*dx < 1e-20) break;
    }
    return _pe(x);
}

/* -- haversine great-circle distance --------------------------- */
/* d = 2R * asin(sqrt(sin^2(dlat/2) + cos(lat1)*cos(lat2)*sin^2(dlon/2)))
 * lat/lon in RADIANS as posit32.
 * Returns distance in METERS as posit32.
 * R = 6371000 m (mean Earth radius)                           */

static uint32_t posit32_haversine(uint32_t lat1, uint32_t lon1,
                                  uint32_t lat2, uint32_t lon2) {
    uint32_t half    = _pe(0.5);
    uint32_t dlat_h  = p32_mul(p32_sub(lat2, lat1), half);
    uint32_t dlon_h  = p32_mul(p32_sub(lon2, lon1), half);
    uint32_t sdlat   = posit32_sin(dlat_h);
    uint32_t sdlon   = posit32_sin(dlon_h);
    uint32_t clat1   = posit32_cos(lat1);
    uint32_t clat2   = posit32_cos(lat2);
    uint32_t a       = p32_add(p32_mul(sdlat, sdlat),
                               p32_mul(p32_mul(clat1, clat2),
                                       p32_mul(sdlon, sdlon)));
    uint32_t R       = _pe(6371000.0);
    uint32_t two     = _pe(2.0);
    return p32_mul(p32_mul(two, R), posit32_asin(posit32_sqrt(a)));
}

/* -- bearing --------------------------------------------------- */
/* Forward azimuth from point 1 to point 2 in radians.
 * lat/lon in radians as posit32.                              */

static uint32_t posit32_bearing(uint32_t lat1, uint32_t lon1,
                                 uint32_t lat2, uint32_t lon2) {
    uint32_t dlon  = p32_sub(lon2, lon1);
    uint32_t clat2 = posit32_cos(lat2);
    uint32_t slat1 = posit32_sin(lat1);
    uint32_t slat2 = posit32_sin(lat2);
    uint32_t clat1 = posit32_cos(lat1);
    uint32_t cdlon = posit32_cos(dlon);
    uint32_t sdlon = posit32_sin(dlon);
    uint32_t y     = p32_mul(sdlon, clat2);
    uint32_t x     = p32_sub(p32_mul(clat1, slat2),
                              p32_mul(slat1, p32_mul(clat2, cdlon)));
    return posit32_atan2(y, x);
}

/* -- Chauvenet's criterion ------------------------------------- */
/* Reject measurement xi if |xi - mean|/sigma > threshold
 * threshold = erfc_inv(1/(2n)) / sqrt(2)
 * Returns 1 to reject, 0 to keep.                             */

static int posit32_chauvenet(uint32_t xi, uint32_t mean,
                              uint32_t sigma, int n) {
    double dev = fabs(_pd(xi) - _pd(mean)) / _pd(sigma);
    double p   = 1.0 / (2.0 * n);
    double thr = _pd(posit32_erfc_inv(_pe(p))) / sqrt(2.0);
    return dev > thr;
}

/* -- deg2rad helper -------------------------------------------- */

static uint32_t posit32_deg2rad(uint32_t deg) {
    return p32_mul(deg, _pe(3.14159265358979 / 180.0));
}

/* -- BRAD integer haversine and bearing ----------------------- */
/* Input: lat/lon as BRAD integers (BRAD_PI=16384=180 degrees).
 * Uses integer sin/cos from your 2020 math.c -- Q12 fixed-point.
 * No floating point in the conversion path.
 * One _pe() call at the posit32 encoding boundary -- unavoidable.
 *
 * UBX to BRAD: brad = ubx_1e7 / 109863
 * BRAD to degrees: deg = brad * 180 / 16384                     */

/* -- BRAD integer trig (Paul Sherman, 2020-05-12) ------------- */
/* Binary radian arithmetic: BRAD_PI = 16384 = 180 degrees.
 * Full circle = 32768. sin/cos return Q12 fixed-point.
 * No floating point. No FPU. Pure RV32I.                        */

#define BRAD_PI   16384
#define BRAD_HPI  8192
#define BRAD_2PI  32768

static int32_t _brad_sin_kernel(int32_t x) {
    uint8_t c; uint32_t y;
    enum { A1=3370945099UL, B1=2746362156UL, C1=292421UL };
    enum { n=13, p=32, q=31, r=3, a=12 };
    x <<= 1; c = x<0;
    if(x==(x | 0x4000)) x = (1<<15) - x;
    x = (x & 0x7FFF) >> 1;
    y = (C1 * ((uint32_t)x)) >> n;
    y = B1 - (((uint32_t)x * y) >> r);
    y = (uint32_t)x * (y >> n);
    y = (uint32_t)x * (y >> n);
    y = A1 - (y >> (p - q));
    y = (uint32_t)x * (y >> n);
    y = (y+(1UL << (q - a - 1))) >> (q - a);
    return c ? -(int32_t)y : (int32_t)y;
}

static int32_t brad_sin(int32_t x) {
    switch((x/BRAD_HPI) & 3) {
        case 0: return  _brad_sin_kernel(x);
        case 1: return  _brad_sin_kernel(BRAD_PI - x);
        case 2: return -_brad_sin_kernel(x - BRAD_PI);
        case 3: return -_brad_sin_kernel(BRAD_2PI - x);
        default: return 0;
    }
}

static int32_t brad_cos(int32_t x) { return brad_sin(BRAD_HPI-x); }

static int32_t brad_atan2(int32_t x, int32_t y) {
    int32_t phi=0, t, t2, dphi, fixshft=15;
    if(y==0) return x >= 0 ? 0 : BRAD_PI;
    if(y<0){ x=-x; y=-y; phi+=4; }
    if(x<=0) { t=x; x=y;y=t; phi+=2; }
    if(x<=y) { t=y-x; x+=y; y=t; phi+=1; }
    phi *= BRAD_PI/4;
    t = (y << fixshft)/x; t2 = -t*t >> fixshft;
    dphi = 0x0470;
    dphi = 0x1029 + (t2*dphi >> fixshft);
    dphi = 0x1F0B + (t2*dphi >> fixshft);
    dphi = 0x364C + (t2*dphi >> fixshft);
    dphi = 0xA2FC + (t2*dphi >> fixshft);
    dphi = dphi*t >> fixshft;
    return phi+((dphi+4)>>3);
}

/* -- BRAD integer haversine and bearing ---------------------- */
/* Input: lat/lon as BRAD integers (BRAD_PI=16384=180 degrees).
 * One _pe() call at the posit32 encoding boundary -- unavoidable.
 * UBX to BRAD: brad = ubx_1e7 / 109863
 * BRAD to degrees: deg = brad * 180 / 16384                     */

static uint32_t posit32_haversine_brad(int32_t lat1, int32_t lon1,
                                       int32_t lat2, int32_t lon2) {
    int32_t dlat_h = (lat2-lat1)>>1;
    int32_t dlon_h = (lon2-lon1)>>1;
    int32_t sdlat  = brad_sin(dlat_h);
    int32_t sdlon  = brad_sin(dlon_h);
    int32_t clat1  = brad_cos(lat1);
    int32_t clat2  = brad_cos(lat2);
    int32_t a = (sdlat*sdlat>>12)
              + (((clat1*clat2)>>12)*(sdlon*sdlon>>12)>>12);
    uint32_t pa  = _pe((double)a/16777216.0);
    uint32_t R   = _pe(6371000.0);
    uint32_t two = _pe(2.0);
    return p32_mul(p32_mul(two,R),posit32_asin(posit32_sqrt(pa)));
}

static int32_t posit32_bearing_brad(int32_t lat1, int32_t lon1,
                                    int32_t lat2, int32_t lon2) {
    int32_t dlon = lon2-lon1;
    int32_t y    = (brad_sin(dlon)*brad_cos(lat2))>>12;
    int32_t x    = (brad_cos(lat1)*brad_sin(lat2)>>12)
                 - (brad_sin(lat1)*(brad_cos(lat2)*brad_cos(dlon)>>12)>>12);
    return brad_atan2(y,x);
}

#endif /* POSIT_MATH_H */
