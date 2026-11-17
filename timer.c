// timer.c -- FE310 CLINT/PLIC interrupt infrastructure
//
// trap handler dispatch, timer init, interrupt enable/disable
//
// 2021-09-12  pds  initial cut for FE310
// 2026-11-17  pds/claude  split from fixfft.c

#include <stdint.h>
#include "gpio.h"
#include "i2s.h"   // for MIC_STAT
#include "timer.h"

static void foo_handler (void)
{
  gpio_high( MIC_STAT );
  gpio_low ( MIC_STAT );
  gpio_high( MIC_STAT );
  gpio_low ( MIC_STAT );
}

void (*interrupt_handler[MAX_INTERRUPTS]) (void) = {
  foo_handler,  //  0
  foo_handler,  //  1
  foo_handler,  //  2
  foo_handler,  //  3  machine software interrupt
  foo_handler,  //  4
  foo_handler,  //  5
  foo_handler,  //  6
  foo_handler,  //  7  machine timer interrupt
  foo_handler,  //  8
  foo_handler,  //  9
  foo_handler,  // 10
  foo_handler,  // 11  machine external interrupt
  foo_handler,  // 12
  foo_handler,  // 13
  foo_handler,  // 14
  foo_handler,  // 15
};

void (*exception_handler[MAX_INTERRUPTS]) (void) = {
  foo_handler,  //  0  instruction address misaligned
  foo_handler,  //  1  instruction address fault
  foo_handler,  //  2  illegal instruction
  foo_handler,  //  3  breakpoint
  foo_handler,  //  4  load address misaligned
  foo_handler,  //  5  load access fault
  foo_handler,  //  6  store/AMO address misaligned
  foo_handler,  //  7  store/AMO access fault
  foo_handler,  //  8  environment call from U-mode
  foo_handler,  //  9
  foo_handler,  // 10
  foo_handler,  // 11  environment call from M-mode
  foo_handler,  // 12
  foo_handler,  // 13
  foo_handler,  // 14
  foo_handler,  // 15
};

void handle_trap (void)
{
  unsigned long mcause_value;

  mcause_value = get_mcause();

  if (mcause_value & MCAUSE_TYPE)
    interrupt_handler[mcause_value & MCAUSE_EC]();
  else
    exception_handler[mcause_value & MCAUSE_EC]();
}

void reset_timer (int count_val)
{
  clint_mtime    = 0;
  clint_mtimecmp = count_val;
}

#define BLINK_FREQ  4000000

static void timer_handler (void)
{
  uint32_t x;

  x = plic_claim;
  plic_claim = x;

  gpio_high( MIC_STAT );
  gpio_low ( MIC_STAT );

  reset_timer(MTIME_CLK_FREQ / (2 * BLINK_FREQ));
}

void enable_interrupts (void)
{
  uint32_t x;

  x = get_mie();
  x |= (1 << 3);   // machine software interrupt enable
  x |= (1 << 7);   // machine timer interrupt enable
  x |= (1 << 11);  // machine external interrupt enable
  set_mie(x);

  x = get_mstatus();
  x |= (1 << 3);   // MSTATUS_MIE
  set_mstatus(x);
}

void disable_interrupts (void)
{
  uint32_t x;

  x = get_mstatus();
  x &= ~(1 << 3);
  set_mstatus(x);

  x = get_mie();
  x &= ~(1 << 3);
  x &= ~(1 << 7);
  x &= ~(1 << 11);
  set_mie(x);
}

void timer_init (void)
{
  gpio_high( MIC_STAT );
  gpio_low ( MIC_STAT );

  plic_prio_pwm1_1 = PLIC_PRIO_MIDDLE;
  plic_ena2        = PLIC_ENA2_PWM1_1;

  plic_prio_pwm2_1 = PLIC_PRIO_MIDDLE;
  plic_ena2        = PLIC_ENA2_PWM2_1;

  plic_thr = PLIC_THR_LOWEST;

  interrupt_handler[7] = timer_handler;  // machine timer (MCAUSE_EC=7)

  set_mtvec(((uint32_t) handle_trap) & 0xFFFFFFFC);  // direct mode; 64-bit align

  reset_timer(MTIME_CLK_FREQ / (2 * BLINK_FREQ));

  enable_interrupts();
}
