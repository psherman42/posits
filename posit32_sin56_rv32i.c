/* posit32_sin56_rv32i.c -- posit32 sin(pi*x) and cos(pi*x) with RV32I CORDIC
 * 2026-11-12 pds
 *
 * Path B: C wrapper for argument reduction and posit encode/decode.
 * The CORDIC engine (cordic56_rv32i) is pure RV32I assembly -- no FPU,
 * no MUL, no extensions. Shifts and adds only.
 *
 * Build for FE310 (bare RV32I):
 *   riscv32-unknown-elf-gcc -march=rv32i -mabi=ilp32 \
 *       posit32_sin56_rv32i.c posit_trig56_rv32i.s \
 *       -nostdlib -o sin56_fe310.elf
 *
 * Volder 1959. Gustafson 2026. Pioneer 2026. Woz smiles 2026.
 */

#include <stdint.h>
#include <math.h>
#include "positenc.h"
#include "sin_pi_hard.h"
#include "cos_pi_hard.h"

/* RV32I assembly CORDIC engine -- pure shifts and adds */
extern void cordic56_rv32i(int64_t angle_hi, int64_t angle_lo,
                            int64_t *cos_out, int64_t *sin_out);

/* Q56 scale factor: pi/2 in Q56 */
#define Q56_PI2  113187804032455040LL
#define Q56D     72057594037927936.0
#define QB       1e-10

uint32_t rv32i_sin_pi56(uint32_t x)
{
    int _i;
    positenv_init(32,2);
    int neg = 0;

    /* hard case lookup -- 31 entries, 248 bytes */
    for (_i = 0; _i < N_SIN_PI_HARD; _i++)
        if (_sin_pi_hard[_i][0] == x) return _sin_pi_hard[_i][1];

    /* posit32 decode */
    double xd = posit_to_double(x);

    /* mod-2 reduction: sin(pi*(x+2)) = sin(pi*x) */
    xd = fmod(xd, 2.0);
    if (xd < 0.0) xd += 2.0;

    /* symmetry reduction to [0, 1/2]
     * sin(pi*x) for x in [0,2]:
     *   [0,   1/2]: compute directly          neg=0
     *   [1/2,   1]: sin(pi*x) = sin(pi*(1-x)) neg=0
     *   [1,   3/2]: sin(pi*x) = -sin(pi*(x-1))neg=1
     *   [3/2,   2]: sin(pi*x) = -sin(pi*(2-x))neg=1 */
    if      (xd < 0.5 + QB) { ;              neg = 0; }
    else if (xd < 1.0 + QB) { xd = 1.0 - xd; neg = 0; }
    else if (xd < 1.5 + QB) { xd = xd - 1.0; neg = 1; }
    else                    { xd = 2.0 - xd;  neg = 1; }

    /* near x=0: Taylor series
     * sin(pi*x) ~= pi*x*(1 - (pi*x)^2/6 + (pi*x)^4/120)
     * accurate to posit32 ULP for x < 0.044
     * adaptive extension: add terms 7,9 if near posit32 midpoint */
    if (xd < 0.044) {
        double pix  = 3.14159265358979323846 * xd;
        double pix2 = pix * pix;
        double r = pix * (1.0 - pix2/6.0 + pix2*pix2/120.0);

        double rn  = neg ? -r : r;
        double near = posit_to_double(posit_from_double(rn));
        double dist = rn - near;
        if (dist < 0.0) dist = -dist;
        uint32_t pn = posit_from_double(near < 0 ? -near : near);
        double half_ulp = (posit_to_double(pn+1) - posit_to_double(pn)) / 2.0;

        if (dist > 0.0 && dist < half_ulp)
            r += pix * pix2*pix2*pix2 * (-1.0/5040.0 + pix2/362880.0);

        return posit_from_double(neg ? -r : r);
    }

    /* main range: RV32I assembly CORDIC
     * scale xd to Q56 angle, split into hi:lo for RV32I convention */
    int64_t aq = (int64_t)(xd * 2.0 * (double)Q56_PI2);
    int64_t aq_hi = aq >> 32;
    int64_t aq_lo = aq & 0xffffffff;

    int64_t c56, s56;
    cordic56_rv32i(aq_hi, aq_lo, &c56, &s56);

    double r = (double)s56 / Q56D;
    return posit_from_double(neg ? -r : r);
}

uint32_t rv32i_cos_pi56(uint32_t x)
{
    int _i;
    positenv_init(32,2);
    int neg = 0;

    /* hard case lookup -- 250 entries, 2KB */
    for (_i = 0; _i < N_COS_PI_HARD; _i++)
        if (_cos_pi_hard[_i][0] == x) return _cos_pi_hard[_i][1];

    /* posit32 decode */
    double xd = posit_to_double(x);

    /* mod-2 reduction: cos(pi*(x+2)) = cos(pi*x) */
    xd = fmod(xd, 2.0);
    if (xd < 0.0) xd += 2.0;

    /* symmetry reduction to [0, 1/2]
     * cos(pi*x) for x in [0,2]:
     *   [0,   1/2]: compute directly           neg=0
     *   [1/2,   1]: cos(pi*x) = -cos(pi*(1-x)) neg=1
     *   [1,   3/2]: cos(pi*x) = -cos(pi*(x-1)) neg=1
     *   [3/2,   2]: cos(pi*x) =  cos(pi*(2-x)) neg=0 */
    if      (xd < 0.5 + QB) { ;               neg = 0; }
    else if (xd < 1.0 + QB) { xd = 1.0 - xd;  neg = 1; }
    else if (xd < 1.5 + QB) { xd = xd - 1.0;  neg = 1; }
    else                    { xd = 2.0 - xd;   neg = 0; }

    /* near x=0.5: Taylor series for cos near zero
     * eps = 0.5 - xd, cos(pi*x) = cos(pi*(0.5-eps)) = sin(pi*eps)
     * accurate to posit32 ULP for eps < 0.044 (xd > 0.456)
     * adaptive extension: add terms 7,9 if near posit32 midpoint */
    if (xd > 0.456) {
        double eps  = 0.5 - xd;
        double pie  = 3.14159265358979323846 * eps;
        double pie2 = pie * pie;
        double r = pie * (1.0 - pie2/6.0 + pie2*pie2/120.0);

        double rn   = neg ? -r : r;
        double near = posit_to_double(posit_from_double(rn));
        double dist = rn - near;
        if (dist < 0.0) dist = -dist;
        uint32_t pn = posit_from_double(near < 0 ? -near : near);
        double half_ulp = (posit_to_double(pn+1) - posit_to_double(pn)) / 2.0;

        if (dist > 0.0 && dist < half_ulp)
            r += pie * pie2*pie2*pie2 * (-1.0/5040.0 + pie2/362880.0);

        return posit_from_double(neg ? -r : r);
    }

    /* main range: RV32I assembly CORDIC */
    int64_t aq = (int64_t)(xd * 2.0 * (double)Q56_PI2);
    int64_t aq_hi = aq >> 32;
    int64_t aq_lo = aq & 0xffffffff;

    int64_t c56, s56;
    cordic56_rv32i(aq_hi, aq_lo, &c56, &s56);

    double r = (double)c56 / Q56D;
    return posit_from_double(neg ? -r : r);
}
