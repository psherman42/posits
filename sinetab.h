// sinetab.h -- sinewave table and quadrant-unfolding lookup
//
// N_WAVE/4 = 64 entries covering first quadrant (0 to pi/2)
// sintab(angle) and costab(angle): full-range, uint8_t angle = 0..255 = 0..2*pi
//
// SINTAB_8  : int8_t,  SCALE=7,  range -128..127
// SINTAB_16 : int32_t, SCALE=15, range -32768..32767  (default)
// SINTAB_18 : int32_t, SCALE=17, range -131072..131071
//
// 2021-09-12  pds  initial cut for FE310

#ifndef SINETAB_H__INCLUDED
#define SINETAB_H__INCLUDED

#include <stdint.h>

#define N_WAVE       256  // full length of sinewave

#undef SINTAB_8
#define SINTAB_16
#undef SINTAB_18

#ifdef SINTAB_16
#define SCALE        15  // 32768   // 16-bit, signed: 1 << 15
const int32_t Sinewave[N_WAVE/4] = {
      0,   804,  1607,  2410,  3211,  4011,  4807,  5601,
   6392,  7179,  7961,  8739,  9511, 10278, 11038, 11792,
  12539, 13278, 14009, 14732, 15446, 16150, 16845, 17530,
  18204, 18867, 19519, 20159, 20787, 21402, 22004, 22594,
  23169, 23731, 24278, 24811, 25329, 25831, 26318, 26789,
  27244, 27683, 28105, 28510, 28897, 29268, 29621, 29955,
  30272, 30571, 30851, 31113, 31356, 31580, 31785, 31970,
  32137, 32284, 32412, 32520, 32609, 32670, 32727, 32757,
};
#endif//SINTAB_16

int32_t sintab (uint8_t angle)
{
  uint8_t idx;
  int32_t sgn;

  idx = angle;

       if (idx <  64) { idx += 0;               sgn =  1; }  // a: fwd
  else if (idx < 128) { idx = 128 - idx; idx--; sgn =  1; }  // b: rev
  else if (idx < 192) { idx = idx - 128;        sgn = -1; }  // c: fwd
  else                { idx = 256 - idx; idx--; sgn = -1; }  // d: rev

  return sgn * Sinewave[ idx ];
}

int32_t costab (uint8_t angle)
{
  uint8_t idx;
  int32_t sgn;

  idx = angle;

       if (idx <  64) { idx = 64 - idx; idx--;  sgn =  1; }  // b: rev
  else if (idx < 128) { idx = idx - 64;         sgn = -1; }  // c: fwd
  else if (idx < 192) { idx = 192 - idx; idx--; sgn = -1; }  // d: rev
  else                { idx = idx - 192;         sgn =  1; }  // a: fwd

  return sgn * Sinewave[ idx ];
}

#endif//SINETAB_H__INCLUDED
