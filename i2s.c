// i2s.c -- I2S microphone driver (GPIO bit-bang)
//
// 2021-09-12  pds  initial cut for FE310
// 2026-11-17  pds/claude  split from fixfft.c

#include <stdint.h>
#include "gpio.h"
#include "i2s.h"
#include "pwm.h"  // for pwm1_cfg, PWM_CFG_CMP1IP; pwm2_cfg, PWM_CFG_CMP2IP

#define gpio_base       0x10012000
#define gpio_input_val  (*(volatile uint32_t *) (gpio_base + 0x00))
#define gpio_output_val (*(volatile uint32_t *) (gpio_base + 0x0C))
#define gpio_iof_en     (*(volatile uint32_t *) (gpio_base + 0x38))
#define gpio_iof_sel    (*(volatile uint32_t *) (gpio_base + 0x3C))

void i2s_init (void)
{
  gpio_dir( MIC_STAT,  GPIO_OUT );
  gpio_dir( MIC_LRCLK, GPIO_OUT );
  gpio_dir( MIC_BCLK,  GPIO_OUT );
  gpio_dir( MIC_DOUT,  GPIO_IN  );

  gpio_iof( MIC_STAT,  GPIO_GPIO );
  gpio_iof( MIC_LRCLK, GPIO_GPIO );
  gpio_iof( MIC_BCLK,  GPIO_GPIO );
  gpio_iof( MIC_DOUT,  GPIO_GPIO );

  gpio_ds_low( MIC_STAT  );
  gpio_ds_low( MIC_LRCLK );
  gpio_ds_low( MIC_BCLK  );

  gpio_xor( MIC_STAT,  GPIO_NORM );
  gpio_xor( MIC_LRCLK, GPIO_NORM );
  gpio_xor( MIC_BCLK,  GPIO_NORM );

  gpio_low ( MIC_STAT  );
  gpio_high( MIC_LRCLK );
  gpio_high( MIC_BCLK  );

  gpio_pu_dis( MIC_DOUT );
}

// none=1us, 5=12us, 10=20us, 2=5.4us, 1=3.6us
static void dly (void) { volatile uint32_t x; for (x=0; x<1; x++) { x++; x--; } }

void i2s_read_sync (int n, int m, int16_t *left, int16_t *right)
{
  uint32_t x;
  int i, k;
  (void) dly;  // available for timing use

  /////////////////////////////////
  // 32bit unsigned to 18bit signed
  //
  // 32      24           14                0
  //  |       |            |                |
  //  Sddd dddd dddd dddd dL00 0000 XXXX XXXX
  /////////////////////////////////
  //
  // startup sequence (sleep to active) is 7 cycles of WS
  // followed by 4 cycles of BCLK
  // thus 8 cycles of WS before data valid

  while ((pwm1_cfg & PWM_CFG_CMP1IP) != 0);  // WS high
  while ((pwm1_cfg & PWM_CFG_CMP1IP) == 0);  // WS low

  for (k = 0; k < n; k++)
  {
    while ((pwm1_cfg & PWM_CFG_CMP1IP) != 0);  // WS high

    x = 0;
    for (i = 32; i > 0; i--)  // 32 bits, MSB first
    {
      while ((pwm2_cfg & PWM_CFG_CMP1IP) != 0);  // BCLK high
      x <<= 1;
      x |= ((gpio_input_val >> MIC_DOUT) & 0x1);
      while ((pwm2_cfg & PWM_CFG_CMP1IP) == 0);  // BCLK low
    }
    x &= 0x81FFCFFF;  // s111 100?  xxxx xxxx  xx00 0000  zzzz zzzz
    left[k] = ((int32_t) x >> (14 + 0)) - m;

    while ((pwm1_cfg & PWM_CFG_CMP1IP) == 0);  // WS low

    x = 0;
    for (i = 32; i > 0; i--)
    {
      while ((pwm2_cfg & PWM_CFG_CMP1IP) != 0);  // BCLK high
      x <<= 1;
      x |= ((gpio_input_val >> MIC_DOUT) & 0x1);
      while ((pwm2_cfg & PWM_CFG_CMP1IP) == 0);  // BCLK low
    }
    x &= 0x81FFCFFF;
    right[k] = ((int32_t) x >> (14 + 0)) - m;
  }
}
