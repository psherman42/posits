// pwm.c -- FE310 PWM driver
//
// PWM1 = WS / LRCLK (word select, 1x bit rate)
// PWM2 = BCLK       (bit clock,  64x bit rate)
//
// HFCLK = 64 MHz (LoFive R1: 16 MHz xtal through PLL via PRCI_EXT_DIR)
//
// BCLK MHz  PWMx  CMP0 (N-1)  CMP1 (N/2)
// --------  ----  ----------  ----------
//   0.300   pwm1   13652       6827
//           pwm2     212        107
//
//   1.000   pwm1    4095       2048
//           pwm2      63         32
//
//   1.340   pwm1    3056       1529
//           pwm2      47         24
//
//   2.000   pwm1    2047       1024
//           pwm2      31         16
//
//   3.000   pwm1    1364        683
//           pwm2      20         11
//
//   4.000   pwm1    1023        512
//           pwm2      15          8
//
//   4.800   pwm1     852        427
//           pwm2      12          7
//
// 2021-09-12  pds  initial cut for FE310
// 2026-11-17  pds/claude  split from fixfft.c

#include <stdint.h>
#include "gpio.h"
#include "pwm.h"

void pwm_init (uint16_t bclk_khz)
{
  pwm1_cfg = 0;  pwm1_count = 0;  pwm1_scount = 0;
  pwm2_cfg = 0;  pwm2_count = 0;  pwm2_scount = 0;

  switch (bclk_khz)
  {
    // HFCLK = 16 MHz
    case  300: pwm1_cmp0 = 13652; pwm1_cmp1 =  6827; pwm2_cmp0 = 212; pwm2_cmp1 = 107; break;
    case 1000: pwm1_cmp0 =  4095; pwm1_cmp1 =  2048; pwm2_cmp0 =  63; pwm2_cmp1 =  32; break;
    case 1340: pwm1_cmp0 =  3056; pwm1_cmp1 =  1529; pwm2_cmp0 =  47; pwm2_cmp1 =  24; break;
    case 2000: pwm1_cmp0 =  2047; pwm1_cmp1 =  1024; pwm2_cmp0 =  31; pwm2_cmp1 =  16; break;
    case 3000: pwm1_cmp0 =  1364; pwm1_cmp1 =   683; pwm2_cmp0 =  20; pwm2_cmp1 =  11; break;
    case 4000: pwm1_cmp0 =  1023; pwm1_cmp1 =   512; pwm2_cmp0 =  15; pwm2_cmp1 =   8; break;
    case 4800: pwm1_cmp0 =   852; pwm1_cmp1 =   427; pwm2_cmp0 =  12; pwm2_cmp1 =   7; break;
    default:   pwm1_cmp0 =  4095; pwm1_cmp1 =  2048; pwm2_cmp0 =  63; pwm2_cmp1 =  32; break; // 1 MHz
  }

  pwm1_cmp2 = pwm1_cmp1;
  pwm1_cmp3 = pwm1_cmp1;
  pwm2_cmp2 = pwm2_cmp1;
  pwm2_cmp3 = pwm2_cmp1;

  gpio_iof( PWM2_1, GPIO_PWM );  // BCLK  -> PWM2.1
  gpio_iof( PWM1_1, GPIO_PWM );  // LRCLK -> PWM1.1

  pwm1_cfg = (PWM_CFG_ENALWAYS | PWM_CFG_DEGLITCH | PWM_CFG_ZEROCMP | 0);
  pwm2_cfg = (PWM_CFG_ENALWAYS | PWM_CFG_DEGLITCH | PWM_CFG_ZEROCMP | 0);
}
