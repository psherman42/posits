#ifndef PWM_H__INCLUDED
#define PWM_H__INCLUDED

// pwm.h -- FE310 PWM driver
//
// PWM1 generates WS (word select / LRCLK) for I2S mic
// PWM2 generates BCLK for I2S mic
//
// Pin assignments (LoFive R1):
//   PWM2.1   GPIO11  pin 17  BCLK  (also MIC_BCLK)
//   PWM1.1   GPIO19  pin 23  LRCLK (also MIC_LRCLK / MIC_WS)
//
// BCLK rates (HFCLK = 16 MHz):
//   300, 1000, 1340, 2000, 3000, 4000, 4800 kHz
//
// 2021-09-12  pds  initial cut for FE310
// 2026-11-17  pds/claude  split from fixfft.c

#include <stdint.h>

// PWM base addresses
#define pwm0_base 0x10015000
#define pwm1_base 0x10025000
#define pwm2_base 0x10035000

// PWM register map (offsets from base)
#define PWM_CFG_CMP3IP    0x80000000  // [31]
#define PWM_CFG_CMP2IP    0x40000000  // [30]
#define PWM_CFG_CMP1IP    0x20000000  // [29]
#define PWM_CFG_CMP0IP    0x10000000  // [28]
#define PWM_CFG_CMP3CTR   0x00080000  // [19]
#define PWM_CFG_CMP2CTR   0x00040000  // [18]
#define PWM_CFG_CMP1CTR   0x00020000  // [17]
#define PWM_CFG_CMP0CTR   0x00010000  // [16]
#define PWM_CFG_ENONESHOT 0x2000      // [13]
#define PWM_CFG_ENALWAYS  0x1000      // [12]
#define PWM_CFG_DEGLITCH  0x400       // [10]
#define PWM_CFG_ZEROCMP   0x200       // [9]
#define PWM_CFG_STICKY    0x100       // [8]

// PWM1 registers (WS / LRCLK)
#define pwm1_cfg    (*(volatile uint32_t *) (pwm1_base + 0x00))
#define pwm1_count  (*(volatile uint32_t *) (pwm1_base + 0x08))
#define pwm1_scount (*(volatile uint32_t *) (pwm1_base + 0x10))
#define pwm1_cmp0   (*(volatile uint32_t *) (pwm1_base + 0x20))
#define pwm1_cmp1   (*(volatile uint32_t *) (pwm1_base + 0x24))
#define pwm1_cmp2   (*(volatile uint32_t *) (pwm1_base + 0x28))
#define pwm1_cmp3   (*(volatile uint32_t *) (pwm1_base + 0x2C))

// PWM2 registers (BCLK)
#define pwm2_cfg    (*(volatile uint32_t *) (pwm2_base + 0x00))
#define pwm2_count  (*(volatile uint32_t *) (pwm2_base + 0x08))
#define pwm2_scount (*(volatile uint32_t *) (pwm2_base + 0x10))
#define pwm2_cmp0   (*(volatile uint32_t *) (pwm2_base + 0x20))
#define pwm2_cmp1   (*(volatile uint32_t *) (pwm2_base + 0x24))
#define pwm2_cmp2   (*(volatile uint32_t *) (pwm2_base + 0x28))
#define pwm2_cmp3   (*(volatile uint32_t *) (pwm2_base + 0x2C))

// GPIO pins used for PWM output
#define PWM2_0  10  // LoFive pin 16, GPIO10/PWM2.0
#define PWM2_1  11  // LoFive pin 17, GPIO11/PWM2.1  (BCLK)
#define PWM2_2  12  // LoFive pin 18, GPIO12/PWM2.2
#define PWM2_3  13  // LoFive pin 19, GPIO13/PWM2.3
#define PWM1_1  19  // LoFive pin 23, GPIO19/PWM1.1  (LRCLK / WS)

void pwm_init (uint16_t bclk_khz);  // 300/1000/1340/2000/3000/4000/4800

#endif//PWM_H__INCLUDED
