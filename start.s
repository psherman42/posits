################################################
##
## x
## 2020-04-02 pds    initial cut
## 2023-05-17 pds    add pmu stuff
##

.include "csr.e"

.section .text

###############################################
##
## entry point main reset vector
##

.globl _start
_start:
  lui a1, 0x80004
  addi sp, a1, -4

  li a0, 0x98989898 # unlock and disable addr
  csrrc x0, pmpcfg0, a0
  csrrc x0, pmpcfg1, a0
  csrrc x0, pmpcfg2, a0
  csrrc x0, pmpcfg3, a0
  li a0, 07070707 # enable R, W, X
  csrrs x0, pmpcfg0, a0
  csrrs x0, pmpcfg1, a0
  csrrs x0, pmpcfg2, a0
  csrrs x0, pmpcfg3, a0

  li a0, 0x00001aaa # disable MPP, SPP. MPIE, SPIE, MIE, SIE
  csrrc x0, mstatus, a0

  #li a0, 0x1800 # MPP=3
  #csrw mstatus, a0

  csrw mip, zero
  csrw mie, zero

  .option push
pcrel_addr:
  auipc gp, %hi(pcrel_addr)
  addi gp, gp, %lo(pcrel_addr)
  .option pop

  # set trap handler
  la t0, clint_trap_handler  # lives in clint.s
  #srl t0, t0, 2  # mtvec.MODE [1:0] = 0
  #sll t0, t0, 2
  #andi t0, t0, 0xFFFFFFFC  # mtvec.BASE [1:0] 0=direct, 1=vector
  csrrw zero, mtvec, t0

#  jal pmu_trap_handler  # lives in pmu.s

  jal main
  j .

.balign 8  # required 64-bit alignment for mtvec in vectored (non-direct) mode
clint_trap_handler: .weak clint_trap_handler
  j .

.balign 4  # 32-bit alignment for jr
pmu_trap_handler: .weak pmu_trap_handler
  ret

pmu_rst_por_vec:  .global pmu_rst_por_vec
  lui a1, 0x80004  # top of ram
  addi sp, a1, -4

  csrrci x0, mstatus, (CSR_MSTATUS_MIE | CSR_MSTATUS_SIE)  # disable interrupts

  lui t0, %hi(CSR_MIE_MEIE | CSR_MIE_MTIE | CSR_MIE_MSIE)
  addi t0, t0, %lo(CSR_MIE_MEIE | CSR_MIE_MTIE | CSR_MIE_MSIE)
  csrrc zero, mie, t0

  # BUG FIX: must explicitly clear pending bits
  lui t0, %hi(CSR_MIP_MEIP | CSR_MIP_MTIP | CSR_MIP_MSIP)
  addi t0, t0, %lo(CSR_MIP_MEIP | CSR_MIP_MTIP | CSR_MIP_MSIP)
  csrrc zero, mip, t0

  # set trap handler
  la t0, clint_trap_handler  # lives in clint.s
  #srl t0, t0, 2  # mtvec.MODE [1:0] = 0
  #sll t0, t0, 2
  #andi t0, t0, 0xFFFFFFFC  # mtvec.BASE [1:0] 0=direct, 1=vector
  csrrw zero, mtvec, t0
  ret

pmu_wake_rtc_vec:  .global pmu_wake_rtc_vec
  ret

################################################
# CSR accessors for C (timer.c / timer.h)
#
get_mstatus: .global get_mstatus
  csrrs a0, mstatus, zero
  ret

set_mstatus: .global set_mstatus
  csrrw zero, mstatus, a0
  ret

get_mie: .global get_mie
  csrrs a0, mie, zero
  ret

set_mie: .global set_mie
  csrrw zero, mie, a0
  ret

get_mip: .global get_mip
  csrrs a0, mip, zero
  ret

set_mip: .global set_mip
  csrrw zero, mip, a0
  ret

get_mcause: .global get_mcause
  csrrs a0, mcause, zero
  ret

set_mtvec: .global set_mtvec
  csrrw zero, mtvec, a0
  ret

