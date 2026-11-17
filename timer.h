#ifndef TIMER_H__INCLUDED
#define TIMER_H__INCLUDED

// timer.h -- FE310 CLINT/PLIC interrupt infrastructure
//
// trap handler dispatch table, timer, and interrupt enable/disable
//
// 2021-09-12  pds  initial cut for FE310
// 2026-11-17  pds/claude  split from fixfft.c

#include <stdint.h>

// CLINT -- Core Local Interruptor
#define clint_base     0x02000000
#define clint_msip     (*(volatile uint32_t *) (clint_base + 0x0000))   // sw interrupt
#define clint_mtimecmp (*(volatile uint64_t *) (clint_base + 0x4000))   // 8 bytes
#define clint_mtime    (*(volatile uint64_t *) (clint_base + 0xbff8))   // 8 bytes

#define CLINT_MSIP_MIE  0x1  // [0]

// PLIC -- Platform Level Interrupt Controller
#define plic_base    0x0c000000

#define plic_prio_pwm1_1  (*(volatile uint32_t *) (plic_base + 0x4 * 45))
#define plic_prio_pwm2_1  (*(volatile uint32_t *) (plic_base + 0x4 * 49))
#define plic_pend2        (*(volatile uint32_t *) (plic_base + 0x1000 + 4))
#define plic_ena2         (*(volatile uint32_t *) (plic_base + 0x2000 + 4))
#define plic_thr          (*(volatile uint32_t *) (plic_base + 0x200000))
#define plic_claim        (*(volatile uint32_t *) (plic_base + 0x200004))

#define PLIC_PRIO_NEVER    0
#define PLIC_PRIO_LOWEST   1
#define PLIC_PRIO_MIDDLE   4
#define PLIC_PRIO_HIGHEST  7

#define PLIC_ENA2_PWM1_1   0x00004000  // [14]
#define PLIC_ENA2_PWM2_1   0x00040000  // [18]

#define PLIC_THR_LOWEST   0
#define PLIC_THR_HIGHEST  7

// MSTATUS / MIE / MIP / MCAUSE
#define MSTATUS_MPP   0x00001800  // [12:11] machine prev priv mode
#define MSTATUS_MIE   0x00000080  // [7]  machine prev int enable
#define MIE_MEIE      0x00000800  // [11] machine external int enable
#define MIE_MTIE      0x00000080  // [7]  machine timer int enable
#define MIE_MSIE      0x00000008  // [3]  machine software enable
#define MCAUSE_TYPE   0x80000000  // [31] 0=exception, 1=interrupt
#define MCAUSE_EC     0x000003FF  // [9:0] exception code

#define MTIME_CLK_FREQ  16000000
#define MAX_INTERRUPTS  16

// CSR accessors (defined in start.s)
uint32_t get_mstatus (void);
uint32_t get_mie     (void);
uint32_t get_mip     (void);
uint32_t get_mcause  (void);
void     set_mstatus (uint32_t x);
void     set_mie     (uint32_t x);
void     set_mip     (uint32_t x);
void     set_mtvec   (uint32_t x);

void handle_trap        (void);
void timer_init         (void);
void enable_interrupts  (void);
void disable_interrupts (void);
void reset_timer        (int count_val);

// interrupt handler table (index by MCAUSE_EC)
extern void (*interrupt_handler[MAX_INTERRUPTS]) (void);
extern void (*exception_handler[MAX_INTERRUPTS]) (void);

#endif//TIMER_H__INCLUDED
