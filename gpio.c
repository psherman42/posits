///////////////////////////////////////////////
// general peripheral i/o
//
// 2020-04-02 pds    initial cut
// 2021-09-22 pds    fix bug gpio_out_xor, add gpio_drv, gpio_xor
//

#include <stdint.h>  // for uint32_t
#include <stddef.h>  // for size_t
#include "defs.h"
#include "gpio.h"


///////////////////////////////////////////////
//
// low level gpio routines
//

#define gpio_base 0x10012000

//
// GPIO_INPUT_VAL
//

#define gpio_input_val    (*(volatile uint32_t *) (gpio_base + 0x00))

#define __gpio_input_val(p)  ( (gpio_input_val >> (p)) & 0x1  )

//
// GPIO_INPUT_EN
//

#define gpio_input_en    (*(volatile uint32_t *) (gpio_base + 0x04))

#define __gpio_input_en(p,x)  ( gpio_input_en = (uint32_t) (( gpio_input_en & ~(0x1 << (p))) | (( (x) & 0x1) << (p))) )

//
// GPIO_OUTPUT_EN
//

#define gpio_output_en    (*(volatile uint32_t *) (gpio_base + 0x08))

#define __gpio_output_en(p,x)  ( gpio_output_en = (uint32_t) (( gpio_output_en & ~(0x1 << (p))) | (( (x) & 0x1) << (p))) )

//
// GPIO_OUTPUT_VAL
//

#define gpio_output_val    (*(volatile uint32_t *) (gpio_base + 0x0C))

// p=0..31; x: 0=low, 1=high
#define __gpio_output_val(p,x)  ( gpio_output_val = (uint32_t) (( gpio_output_val & ~(0x1UL << (p))) | (( (x) & 0x1UL) << (p))) )

//
// GPIO_PUE
//

#define gpio_pue    (*(volatile uint32_t *) (gpio_base + 0x10))

// p=0..31; x: 0=open-drain, 1=push-pull
#define __gpio_pue(p,x)  ( gpio_pue = (uint32_t) (( gpio_pue & ~(0x1 << (p))) | (( (x) & 0x1) << (p))) )

//
// GPIO_DS
//

#define gpio_ds    (*(volatile uint32_t *) (gpio_base + 0x14))

// p=0..31; x: 0=1ma, 1=20ma
#define __gpio_ds(p,x)  ( gpio_ds = (uint32_t) (( gpio_ds & ~(0x1 << (p))) | (( (x) & 0x1) << (p))) )

//
// GPIO_RISE_IE
//

#define gpio_rise_ie    (*(volatile uint32_t *) (gpio_base + 0x18))

#define __gpio_rise_ie(p,x)  ( gpio_rise_ie = (uint32_t) (( gpio_rise_ie & ~(0x1 << (p))) | (( (x) & 0x1) << (p))) )

//
// GPIO_RISE_IP
//

#define gpio_rise_ip    (*(volatile uint32_t *) (gpio_base + 0x1C))

#define __gpio_rise_ip(p,x)  ( gpio_rise_ip = (uint32_t) (( gpio_rise_ip & ~(0x1 << (p))) | (( (x) & 0x1) << (p))) )

//
// GPIO_FALL_IE
//

#define gpio_fall_ie    (*(volatile uint32_t *) (gpio_base + 0x20))

#define __gpio_fall_ie(p,x)  ( gpio_fall_ie = (uint32_t) (( gpio_fall_ie & ~(0x1 << (p))) | (( (x) & 0x1) << (p))) )

//
// GPIO_FALL_IP
//

#define gpio_fall_ip    (*(volatile uint32_t *) (gpio_base + 0x24))

#define __gpio_fall_ip(p,x)  ( gpio_fall_ip = (uint32_t) (( gpio_fall_ip & ~(0x1 << (p))) | (( (x) & 0x1) << (p))) )

//
// GPIO_HIGH_IE
//

#define gpio_high_ie    (*(volatile uint32_t *) (gpio_base + 0x28))

#define __gpio_high_ie(p,x)  ( gpio_high_ie = (uint32_t) (( gpio_high_ie & ~(0x1 << (p))) | (( (x) & 0x1) << (p))) )

//
// GPIO_HIGH_IP
//

#define gpio_high_ip    (*(volatile uint32_t *) (gpio_base + 0x2C))

#define __gpio_high_ip(p,x)  ( gpio_high_ip = (uint32_t) (( gpio_high_ip & ~(0x1 << (p))) | (( (x) & 0x1) << (p))) )

//
// GPIO_LOW_IE
//

#define gpio_low_ie    (*(volatile uint32_t *) (gpio_base + 0x30))

#define __gpio_low_ie(p,x)  ( gpio_low_ie = (uint32_t) (( gpio_low_ie & ~(0x1 << (p))) | (( (x) & 0x1) << (p))) )

//
// GPIO_LOW_IP
//

#define gpio_low_ip    (*(volatile uint32_t *) (gpio_base + 0x34))

#define __gpio_low_ip(p,x)  ( gpio_low_ip = (uint32_t) (( gpio_low_ip & ~(0x1 << (p))) | (( (x) & 0x1) << (p))) )

//
// GPIO_IOF_EN
//

#define gpio_iof_en    (*(volatile uint32_t *) (gpio_base + 0x38))

// p=0..31; x: 0=gpio, 1=uart/spi/i2c/pwm
#define __gpio_iof_en(p,x)  ( gpio_iof_en = (uint32_t) (( gpio_iof_en & ~(0x1 << (p))) | (( (x) & 0x1) << (p))) )

//
// GPIO_IOF_SEL
//

#define gpio_iof_sel   (*(volatile uint32_t *) (gpio_base + 0x3C))

// p=0..31; x: 0=uart/spi/i2c, 1=pwm
#define __gpio_iof_sel(p,x)  ( gpio_iof_sel = (uint32_t) (( gpio_iof_sel & ~(0x1 << (p))) | (( (x) & 0x1) << (p))) )

//
// GPIO_OUT_XOR
//

#define gpio_out_xor   (*(volatile uint32_t *) (gpio_base + 0x40))

// p=0..31; x: 0=normal, 1=invert
#define __gpio_out_xor(p,x)  ( gpio_out_xor = (uint32_t) (( gpio_out_xor & ~(0x1 << (p))) | (( (x) & 0x1) << (p))) )


///////////////////////////////////////////////
//
// high level gpio routines
//

void gpio_init (void)
{
  gpio_ds = 0;
  gpio_pue = 0;
  gpio_out_xor = 0;
  gpio_output_en = 0;
  gpio_input_en = 0;
}

uint8_t gpio_read (uint8_t g)
{
  return __gpio_input_val( g );
}

void gpio_high (uint8_t g)
{
  __gpio_output_val( g, 1 );  // High
}

void gpio_low (uint8_t g)
{
  __gpio_output_val( g, 0 );  // Low
}

void gpio_dir (uint8_t g, enum gpio_dir_t x)
{
  __gpio_ds( g, 0 );  // 1mA
//  __gpio_ds( g, 1 );  // 20mA
  __gpio_pue( g, 0 );  // open-drain
//  __gpio_pue( g, 1 );  // push-pull
//  __gpio_out_xor( g, 0 );  // Normal

  switch (x)
  {
    case GPIO_IN:
      __gpio_output_en( g, 0 );  // Disable
      __gpio_input_en( g, 1 );  // Enable
      break;

    case GPIO_OUT:
      __gpio_input_en( g, 0 );  // Disable
      __gpio_output_en( g, 1 );  // Enable
      break;

    default:
      break;
  }
}

void gpio_iof (uint8_t g, enum gpio_iof_t x)
{
  switch (x)
  {
    case GPIO_GPIO:
      __gpio_iof_en( g, 0 );  // Disable
      break;

    case GPIO_UART:
    case GPIO_SPI:
    case GPIO_I2C:
      __gpio_iof_sel( g, 0 );  // Uart,Spi,I2c
      __gpio_iof_en( g, 1 );  // Enable
      break;

    case GPIO_PWM:
      __gpio_iof_sel( g, 1 );  // PWM
      __gpio_iof_en( g, 1 );  // Enable
      break;

    default:
      break;
  }
}

void gpio_pu_dis (uint8_t g)
{
  __gpio_pue( g, 0 );  // disable
}

void gpio_pu_ena (uint8_t g)
{
  __gpio_pue( g, 1 );  // enable
}

void gpio_ds_low (uint8_t g)
{
  __gpio_ds( g, 0 );  // 1mA
}

void gpio_ds_high (uint8_t g)
{
  __gpio_ds( g, 1 );  // 20mA
}

void gpio_xor (uint8_t g, enum gpio_xor_t x)
{
  switch (x)
  {
    case GPIO_NORM:
      __gpio_out_xor( g, 0 );  // Normal
      break;
    case GPIO_INV:
      __gpio_out_xor( g, 1 );  // Invert
      break;
    default:
      break;
  }
}

