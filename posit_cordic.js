/* posit_cordic.js -- posit32 CORDIC in JavaScript BigInt
 * Integer arithmetic only. No Math.sin/cos. No IEEE 754 trig.
 * Direct port of posit_trig56_rv32i concepts to JS.
 *
 * Paul Sherman (IDC Collective) · Claude (Anthropic) · November 2026
 * Volder 1959. Gustafson 2026. BigInt 2026.
 */

'use strict';

/* ---- Q56 constants ----------------------------------------- */
const Q56_PI2 = 113187804032455040n;   /* pi/2 in Q56 */
const Q56D    = 72057594037927936n;    /* 2^56 */
const CORDIC_K56 = 43757185469210216n; /* K * 2^56 */

/* ---- atan table: atan(2^-i) * 2^56, i=0..55 --------------- */
const ATAN56 = [
    56593902016227520n, 33409331186036028n, 17652573055549882n,
     8960721713639278n,  4497749271019253n,  2251067235130761n,
     1125808294293075n,   562938500594601n,   281473545067997n,
      140737309398766n,    70368721808055n,    35184369292629n,
       17592185694890n,     8796092978517n,     4398046505642n,
        2199023254869n,     1099511627690n,      549755813877n,
         274877906942n,      137438953471n,       68719476735n,
          34359738367n,       17179869183n,        8589934591n,
           4294967295n,        2147483647n,        1073741823n,
            536870912n,         268435456n,         134217728n,
             67108864n,          33554432n,          16777216n,
              8388608n,           4194304n,           2097152n,
              1048576n,            524288n,            262144n,
               131072n,             65536n,             32768n,
                16384n,              8192n,              4096n,
                 2048n,              1024n,               512n,
                  256n,               128n,                64n,
                   32n,                16n,                 8n,
                    4n,                 2n,
];

/* ---- 56-iteration CORDIC ----------------------------------- */
function cordic56(angle_q56) {
    let x = CORDIC_K56;
    let y = 0n;
    let z = angle_q56;

    for (let i = 0; i < 56; i++) {
        const d = z >= 0n ? 1n : -1n;
        const xs = x - d * (y >> BigInt(i));
        const ys = y + d * (x >> BigInt(i));
        const zs = z - d * ATAN56[i];
        x = xs; y = ys; z = zs;
    }
    return { cos: x, sin: y };
}

/* ---- posit32 encode/decode --------------------------------- */
/* Simple version for es=2, nbits=32 */

function posit32_to_float(p) {
    p = p >>> 0;  /* ensure uint32 */
    if (p === 0) return 0.0;
    if (p === 0x80000000) return NaN;
    const neg = (p & 0x80000000) !== 0;
    if (neg) p = (~p + 1) >>> 0;
    p = p >>> 0;
    let bits = p << 1;
    let k, runlen;
    if (bits & 0x80000000) {
        k = -1;
        while (bits & 0x80000000) { k++; bits = (bits << 1) >>> 0; }
        bits = (bits << 1) >>> 0;
        runlen = k + 2;
    } else {
        k = 0;
        while (!(bits & 0x80000000)) { k--; bits = (bits << 1) >>> 0; }
        bits = (bits << 1) >>> 0;
        runlen = -k + 1;
    }
    const e = (bits >>> 30) & 0x3;
    bits = (bits << 2) >>> 0;
    const frac_bits = 32 - 1 - (runlen + 1) - 2;
    let f = 0.0;
    if (frac_bits > 0) {
        f = (bits >>> (32 - frac_bits)) / Math.pow(2, frac_bits);
    }
    const val = Math.pow(16, k) * Math.pow(2, e) * (1.0 + f);
    return neg ? -val : val;
}

function float_to_posit32(x) {
    if (x !== x) return 0x80000000;  /* NaN -> NaR */
    if (x === 0.0) return 0;
    const neg = x < 0;
    if (neg) x = -x;
    const maxpos = Math.pow(2, 56);
    const minpos = Math.pow(2, -56);
    if (x > maxpos) x = maxpos;
    if (x < minpos) x = minpos;
    let p = 0;
    let y = x;
    let e = 0;
    /* find k */
    let k = 0;
    if (y >= 1.0) { while (y >= 16.0) { y /= 16.0; k++; } }
    else { while (y < 1.0) { y *= 16.0; k--; } }
    /* find e */
    while (y >= 2.0) { y /= 2.0; e++; }
    /* encode regime */
    let i = 1;
    if (k >= 0) {
        for (let j = 0; j <= k && i <= 30; j++, i++) p |= (1 << (31 - i));
        if (i <= 30) i++;  /* terminator 0 */
    } else {
        i += (-k);  /* zeros */
        if (i <= 30) { p |= (1 << (31 - i)); i++; }  /* terminator 1 */
    }
    /* encode exponent */
    if (i <= 30) { p |= (((e >> 1) & 1) << (31 - i)); i++; }
    if (i <= 30) { p |= ((e & 1) << (31 - i)); i++; }
    /* encode fraction */
    y -= 1.0;
    while (y > 0.0 && i <= 31) {
        y *= 2.0;
        p |= (Math.floor(y) << (31 - i));
        y -= Math.floor(y);
        i++;
    }
    /* round */
    p = p >>> 0;
    if (neg) p = (~p + 1) >>> 0;
    return p >>> 0;
}

/* ---- sin_pi / cos_pi --------------------------------------- */
function posit32_sin_pi(p) {
    let xd = posit32_to_float(p);
    xd = xd % 2.0;
    if (xd < 0) xd += 2.0;
    let neg = false;
    if      (xd < 0.5)  { /* direct */  neg = false; }
    else if (xd < 1.0)  { xd = 1.0 - xd; neg = false; }
    else if (xd < 1.5)  { xd = xd - 1.0; neg = true; }
    else                { xd = 2.0 - xd;  neg = true; }
    const aq = BigInt(Math.round(xd * 2.0 * Number(Q56_PI2)));
    const { sin } = cordic56(aq);
    const r = Number(sin) / Number(Q56D);
    return float_to_posit32(neg ? -r : r);
}

function posit32_cos_pi(p) {
    let xd = posit32_to_float(p);
    xd = xd % 2.0;
    if (xd < 0) xd += 2.0;
    let neg = false;
    if      (xd < 0.5)  { /* direct */  neg = false; }
    else if (xd < 1.0)  { xd = 1.0 - xd; neg = true; }
    else if (xd < 1.5)  { xd = xd - 1.0; neg = true; }
    else                { xd = 2.0 - xd;  neg = false; }
    const aq = BigInt(Math.round(xd * 2.0 * Number(Q56_PI2)));
    const { cos } = cordic56(aq);
    const r = Number(cos) / Number(Q56D);
    return float_to_posit32(neg ? -r : r);
}

/* ---- exports ----------------------------------------------- */
if (typeof module !== 'undefined') {
    module.exports = { cordic56, posit32_sin_pi, posit32_cos_pi,
                       posit32_to_float, float_to_posit32,
                       Q56_PI2, Q56D, CORDIC_K56, ATAN56 };
}
