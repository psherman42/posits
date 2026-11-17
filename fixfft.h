#ifndef FIXFFT_H__INCLUDED
#define FIXFFT_H__INCLUDED

#include <stdint.h>

// fix_fft - fixed point in-place Fast Fourier Transform
//
// 1.0  1989-08-11  Tom Roberts
// 1.1  1994-12-15  Malcolm Stanley
// 1.2  2006-06-14  Dimitrios P. Bouras
// 1.3  2010-10-10  David Keller
// 1.4  2021-09-12  Paul D. Sherman   port to SiFive FE310

#define N_WAVE        256   // full cycle sinewave table size
#define LOG2_N_WAVE   8     // log2(N_WAVE)

int16_t FIX_MPY  (int16_t a, int16_t b);
int32_t fix_fft  (int16_t fr[], int16_t fi[], int16_t m, int16_t inv);
int32_t fix_fftr (int16_t f[], int32_t m, int32_t inv);

#endif//FIXFFT_H__INCLUDED
