// fix_fft - fixed point in-place Fast Fourier Transform
//
// All data are fixed-point short integers, in which -32768
// to +32768 represent -1.0 to +1.0, respectively. Integer
// arithmetic is used for speed, instead of the more natural
// floating point.
//
// For the forward FFT (time -> freq), fixed scaling is
// performed to prevent arithmetic overflow, and to map a 0dB
// sine/cosine wave (i.e., amplitude = 32768) to two -6dB freq
// coefficients. The return value is always zero.
//
// For the inverse FFT (freq -> time), fixed scaling cannot be
// done, as two 0dB coefficients would sum to a peak amplitude
// of 64K, overflowing the 32k range of the fixed-point integers.
// Thus, the fix_fft() routine performs variable scaling, and
// returns a value which is the number of bits LEFT by which
// the output must be shifted to get the actual amplitude
// (i.e., if fix_fft() returns 3, each value of fr[] and fi[]
// must be multiplied by 8 (2**3) for proper scaling).
//
// 1.0  1989-08-11  Tom Roberts          initial cut
// 1.1  1994-12-15  Malcolm Stanley      made portable
// 1.2  2006-06-14  Dimitrios P. Bouras  enhanced dbouras@ieee.org
// 1.3  2010-10-10  David Keller         modified for 8bit values
// 1.4  2021-09-12  Paul D. Sherman      port to riscv SiFive FE310
//
// split 2026-11-17  pds/claude  i2s->i2s.c  pwm->pwm.c  irq->timer.c  main->fft_demo.c

#include <stdint.h>
#include "fixfft.h"
#include "sinetab.h"

//inline
int16_t FIX_MPY (int16_t a, int16_t b)
{
  int32_t c;

  c = ((int32_t) a * (int32_t) b) >> 14;  // shift right one less bit (15-1)
  b = c & 0x01;  // rounding bit
  a = (c >> 1) + b;  // accum

  return a;
}

int32_t fix_fft (int16_t fr[], int16_t fi[], int16_t m, int16_t inv)  // inv: 0=fwd, 1=rev
{
  int32_t mr, nn, i, j, l, k, istep, n, scale, shift;
  int16_t qr, qi, tr, ti, wr, wi;

  n = 1 << m;

  if (n > N_WAVE) return -1;

  mr = 0;
  nn = n - 1;
  scale = 0;

  // decimation in time, reorder data
  for (m = 1; m <= nn; ++m)
  {
    l = n;
    do
    {
      l >>= 1;
    }
    while (mr+l > nn);
    mr = (mr & (l-1)) + l;

    if (mr <= m) continue;

    tr = fr[m]; fr[m] = fr[mr]; fr[mr] = tr;
    ti = fi[m]; fi[m] = fi[mr]; fi[mr] = ti;
  }

  l = 1;
  k = LOG2_N_WAVE - 1;
  while (l < n)
  {
    if (inv)  // variable scaling, depending upon data
    {
      shift = 0;
      for (i=0; i<n; i++)
      {
        j = fr[i]; if (j < 0) j = -j;
        m = fi[i]; if (m < 0) m = -m;
        if ((j > 16383) || (m > 16383))
        {
          shift = 1;
          break;
        }
      }
      if (shift) ++scale;
    }
    else  // fixed scaling, for proper normalization
    {
      shift = 1;
    }
    istep = l << 1;
    for (m = 0; m < l; ++m)
    {
      j  = m << k;  // 0 <= j < N_WAVE/2
      wr = sintab( j + N_WAVE/4 ); // cos
      wi = -sintab( j );           // -sin

      if (inv) wi = -wi;

      if (shift)
      {
        wr >>= 1;
        wi >>= 1;
      }

      for (i=m; i<n; i+=istep)
      {
        j = i + l;
        tr = FIX_MPY(wr, fr[j]) - FIX_MPY(wi, fi[j]);
        ti = FIX_MPY(wr, fi[j]) + FIX_MPY(wi, fr[j]);
        qr = fr[i];
        qi = fi[i];

        if (shift)
        {
          qr >>= 1;
          qi >>= 1;
        }
        fr[j] = qr - tr;
        fi[j] = qi - ti;
        fr[i] = qr + tr;
        fi[i] = qi + ti;
      }
    }
    --k;
    l = istep;
  }
  return scale;
}

int32_t fix_fftr (int16_t f[], int32_t m, int32_t inv)  // inv: 0=fwd, 1=rev
{
  int32_t i, N, scale;
  int16_t tt, *fr, *fi;

  N = 1 << (m-1);
  scale = 0;
  fr = f;
  fi = &f[N];

  if (inv) scale = fix_fft( fi, fr, m-1, inv );

  for (i=1; i<N; i+=2)
  {
    tt = f[ N + i - 1 ];
    f[ N + i - 1 ] = f[i];
    f[i] = tt;
  }

  if (!inv) scale = fix_fft( fi, fr, m-1, inv );

  return scale;
}
