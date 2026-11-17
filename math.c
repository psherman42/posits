////////////////////////////////////////////////
// math functions
//
// 2020-05-12 pds    initial cut
//

#include <stdint.h>  // for uint32_t
#include <stddef.h>  // for size_t

uint32_t clz (uint32_t x)  // bit twiddling hacks
{
  x |= (x >> 1);
  x |= (x >> 2);
  x |= (x >> 4);
  x |= (x >> 8);
  x |= (x >> 16);
  // hamming distance
  x -= (x >> 1) & 0x55555555U;
  x = ((x >> 2) & 0x33333333U) + (x & 0x33333333U);
  x = ((x >> 4) + x) & 0x0F0F0F0FU;
  x += (x >> 8);
  x += (x >> 16);
  return 32U - (x & 0x0000003FU);
}

int32_t sqrt (int32_t x)  // wiki newtons method
{
  uint32_t q;

//  q = 1 << ((32U - clz(x)) >> 1);
//  q = (q + x / q) >> 1;
//  q = (q + x / q) >> 1;
//  q = (q + x / q) >> 1;

  uint32_t t, r, b;

  r = x;
  b = 0x40000000;  // scale=1<<16, decimal betw bit 15 and 16
  q = 0;

  while (b > 0x40)
  {
    t = q + b;
    if (r >= t) { r -= t; q = t + b; }  // q += 2*b
    r <<= 1;
    b >>= 1;
  }
  q >>= 8;

  return q;
}

const uint32_t BRAD_PI = 1 << 14;  // 0x4000
const uint32_t BRAD_HPI = BRAD_PI >> 1;
const uint32_t BRAD_2PI = BRAD_PI << 1;
const uint32_t ATAN_ONE = 0x1000;
const uint32_t ATAN_FP = 12;

int32_t __sin (int32_t x)  // coranac
{
  int32_t qN=13, qA=12, B=19900, C=3516, c, y;

  c = x << (30 - qN);
  x -= (1 << qN);
  x = x << (31 - qN);
  x = x >> (31 - qN);  // signed shift
  x = x * x >> (2 * qN - 14);
  y = B - (x * C >> 14);  // B - C * X^2
  y = (1 << qA) - (x * y >> 16);   // A - (B - C * x^2)

  return (c >= 0) ? y : -y;
}

int32_t _sin (int32_t x)  // nullhardware, andrew steadman
{
  // (signed) input to 0 - 8192 (pi/2)
  uint8_t c;
  x <<= 1;
  c = x < 0;
  if (x == (x | 0x4000)) x = (1 << 15) - x;
  x = (x & 0x7FFF) >> 1;
  ////////////////////////////////////

  enum { A1 = 3370945099UL, B1 = 2746362156UL, C1 = 292421UL };
  enum { n = 13, p = 32, q = 31, r = 3, a = 12 };

  uint32_t y;
  y = (C1 * ((uint32_t) x)) >> n;
  y = B1 - (((uint32_t) x * y) >> r);
  y = (uint32_t) x * (y >> n);
  y = (uint32_t) x * (y >> n);
  y = A1 - (y >> (p-q));
  y = (uint32_t) x * (y >> n);
  y = (y + (1UL << (q-a-1))) >> (q-a);  // round

  return c ? -y : y;
}

int32_t _cos (int32_t x)
{
  return _sin( BRAD_HPI - x );
}

int32_t sin (int32_t x)  // coranac
{
  int32_t quad;
  quad = x / BRAD_HPI;
  switch (quad)
  {
    case 0: return   _sin( x ); break;
    case 1: return   _sin( BRAD_PI - x ); break;
    case 2: return - _sin( x - BRAD_PI ); break;
    case 3: return - _sin( BRAD_2PI - x ); break;
    default: return 0; break;
  }
}

int32_t cos (int32_t x)
{
  int32_t quad;
  quad = x / BRAD_HPI;
  switch (quad)
  {
    case 0: return   _cos( x ); break;
    case 1: return - _cos( BRAD_PI - x ); break;
    case 2: return - _cos( x - BRAD_PI ); break;
    case 3: return   _cos( BRAD_2PI - x ); break;
    default: return 0; break;
  }
}

int32_t atan2 (int32_t x, int32_t y)
{
  int32_t phi, t, t2, dphi, fixshft=15;

  if (y==0) return x >= 0 ? 0 : BRAD_PI;

  ///////////////////////
  // OCTANTIFY(x, y, phi)
  phi = 0;
  if (y <  0) { ;          x = -x; y = -y; phi += 4; }
  if (x <= 0) { t = x;     x = y;  y = t;  phi += 2; }
  if (x <= y) { t = y - x; x += y; y = t;  phi += 1; }
  ///////////////////////

  phi *= BRAD_PI / 4;
  t = (y << fixshft) / x;  // QDIV( y,x,fixshft )
  t2 = -t*t >> fixshft;
  dphi = 0x0470;                           // 7/257
  dphi = 0x1029 + (t2 * dphi >> fixshft);  // 12/121
  dphi = 0x1F0B + (t2 * dphi >> fixshft);  // 4/21
  dphi = 0x364C + (t2 * dphi >> fixshft);  // 1/3
  dphi = 0xA2FC + (t2 * dphi >> fixshft);  // 1/1
  dphi = dphi * t >> fixshft;

  return phi + ((dphi + 4) >> 3);  // round
}

int32_t mul (int32_t a, int32_t b)
{
  int32_t as, bs;
  uint32_t ah, al, bh, bl, r;

  if (a >= 0) { as = 1; a = a; }
  else { as = -1; a = -a; }

  if (b >= 0) { bs = 1; b = b; }
  else { bs = -1; b = -b; }

  ah = (a << 16) >> 16;
  al = (a >> 16);

  bh = (b << 16) >> 16;
  bl = (b >> 16);

  r = (al * bl) \
    + (((ah * bl) + (al * bh)) << 16) \
    + ((ah * bh) << 16);  // should be << 32?

  r *= (as * bs);

  return r;
}

