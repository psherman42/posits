// fft_demo.c -- FFT spectrum display demo for LoFive R1
//
// I2S mic -> fixed-point FFT -> OLED spectrum display
// Cycling sample rate FSM: L -> M1 -> M2 -> H -> M2 -> M1 -> L
//
// 2021-09-12  pds  initial cut for FE310
// 2026-11-17  pds/claude  split from fixfft.c; main() extracted

#include <stdint.h>
#include "fixfft.h"
#include "sinetab.h"
#include "i2s.h"
#include "pwm.h"
#include "timer.h"
#include "gpio.h"
#include "oled.h"
#include "util.h"

uint32_t clock_init_ext_dir (void);  // prci.s

void main (void)
{
  uint32_t hfclk_hz;
  int16_t im[1024], left[1024], right[1024];
  int16_t *re;
  int32_t dat, avg[2];
  int i, j, k;
  volatile uint32_t x;

  gpio_init();
  hfclk_hz = clock_init_ext_dir();
  oled_init( hfclk_hz, 1000000 );  // SPI clock = 1 MHz

  i2s_init();
  pwm_init( 1000 );  // 1000 kHz BCLK
  k = 1;

  timer_init();

  x = 0; while( x < 160000 ) { x++; }  // ~10ms settle

  oled_clear_display();
  oled_write_string( "welcome to FFT test" );

  j = 0;
  while(1)
  {
    oled_set_pos( 0, 0 );

    // Cycling sample rate FSM: 0=L 1=M1 2=M2 3=H 4=M2 5=M1 -> 0
    switch (k)
    {
      case 0: oled_write_string( "LOW"             ); break;
      case 1: oled_write_string( "    MID 1"       ); break;
      case 2: oled_write_string( "          MID 2" ); break;
      case 3: oled_write_string( "                 HIGH" ); break;
      case 4: oled_write_string( "          MID 2" ); break;
      case 5: oled_write_string( "    MID 1"       ); break;
      default: oled_write_string( "???" );            break;
    }

    j++;

    clint_msip = CLINT_MSIP_MIE;

    // Acquisition
    i2s_read_sync( 1024, 0, left, right );

    // DC baseline removal
    avg[0] = 0;  avg[1] = 0;
    for (i=0; i<1024; i++) { avg[0] += left[i];  avg[1] += right[i]; }
    avg[0] /= 1024;  avg[1] /= 1024;
    for (i=0; i<1024; i++) { left[i] -= avg[0];  right[i] -= avg[1]; }

    // Left channel FFT -> OLED
    re = left;
    for (i=0; i<1024; i++) im[i] = 0;
    fix_fft( re, im, 10, 0 );  // 2^10 = 1024, forward

    dat = 0;
    for (i=1; i<512; i++)  // skip DC
    {
      dat += (re[i] * re[i] + im[i] * im[i]);  // magnitude squared

      if ((i % 4) == 0)
      {
        dat /= 4;
        dat /= 2;
        if (dat > 63) dat = 63;

        if (dat > 16)
        {
          oled_clear_display();
          oled_plot_point( i/4, 63 - dat );
        }
        dat = 0;
      }
    }

    // Right channel FFT -> OLED
    re = right;
    for (i=0; i<1024; i++) im[i] = 0;
    fix_fft( re, im, 10, 0 );

    dat = 0;
    for (i=1; i<512; i++)
    {
      dat += (re[i] * re[i] + im[i] * im[i]);

      if ((i % 4) == 0)
      {
        dat /= 4;
        dat /= 2;
        if (dat > 63) dat = 63;

        if (dat > 16)
        {
          oled_clear_display();
          oled_plot_point( i/4, 63 - dat );
        }
        dat = 0;
      }
    }
  }
}
