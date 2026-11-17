################################################
## PMU.S
## 
## 2023-05-10 pds    initial cut
## pds  BUG FIX: rtc_loop: dont use x0 for amoxor[rd] because pipeline hazard irregularity
##

.include "pmu.e"

.include "gpio.e"  # for EPD_DC,EPD_BUSY
.include "asic-fe310-48qfn.e"  # for EPD_CS, EPD_DC, EPD_BUSY

.section .text

#
# wakeup events (maskable with PMUIE)
#
pmu_trap_handler: .global pmu_trap_handler
  li t0, AON_BASE
  lw a0, AON_PMUCAUSE(t0)  # [11:8]=rstcause, [3:0]=wakeupcause
  andi t1, a0, 0x3         # decode 0,1,2,3 only
  slli t1, t1, 2           # four-byte table entries
  la t2, pmu_wake_vectbl
  add t2, t2, t1
  jr t2

pmu_wake_vectbl:
  j pmu_wake_rst_vec  # pmucause[3:0]=0
  j pmu_wake_rtc_vec  # pmucause[3:0]=1
  j pmu_wake_dig_vec  # pmucause[3:0]=2
  j pmu_wake_ana_vec  # pmucause[3:0]=3

pmu_wake_rst_vec:
  srli t1, a0, 8           # examine [11:8]
  andi t1, a0, 0x3         # decode 0,1,2,3 only
  slli t1, t1, 2           # four-byte table entries
  la t2, pmu_rst_vectbl
  add t2, t2, t1
  jr t2

pmu_wake_rtc_vec:  .weak pmu_wake_rtc_vec   # AON_RTCCFG[28]=rtcip0 (0=rtcs<rtccmp0, 1=rtcs>=rtccmp0)
  ret

pmu_wake_dig_vec:  .weak pmu_wake_dig_vec  # QFN48-18, HiFive/LoFive btn S1
  ret

pmu_wake_ana_vec:  .weak pmu_wake_ana_vec
  ret

#
# reset events
#
pmu_rst_vectbl:
  j pmu_rst_por_vec   # pmucause[11:8]=0
  j pmu_rst_erst_vec  # pmucause[11:8]=1
  j pmu_rst_wdt_vec   # pmucause[11:8]=2
  j pmu_rst_xxx_vec   # pmucause[11:8]=3

pmu_rst_por_vec:  .weak pmu_rst_por_vec
  ret

pmu_rst_erst_vec:  .weak pmu_rst_erst_vec  # QFN48-24, HiFive btn S2
  ret

pmu_rst_wdt_vec:  .weak pmu_rst_wdt_vec   # AON_WDOGCFG[28]=wdogip0 (0=wdogcounts<wdogcmp0, 1=wdogcounts>=wdogcmp0)
  ret

pmu_rst_xxx_vec:  .weak pmu_rst_xxx_vec
  ret




rtc_toggle:
  # a0 = num-loops, a1 = timer (# 100=10ms, 200=18ms, 1000=84ms)
  #-------- marker pulse init --------
  lui t6, 0x10012        # GPIO_BASE
  addi t6, t6, 0x0C      # GPIO_BASE + GPIO_OUTPUT_VAL
  #-------- marker pulse init --------
  li t5, (1<<22)
rtc_loop:
  beqz a0, rtc_toggle_done
  addi a0, a0, -1

  # BUG FIX: dont use x0 for [rd] to avoid pipeline hazard irregularity
  amoxor.w t0, t5, (t6)      # MARKER PULSE: gpio_output_val ^= (1<<21)
  #
  li t0, AON_BASE
  li t2, (1UL << 28)         # [28]=rtcip0
rtc_inner_loop:
  lw t1, AON_RTCCFG(t0)
  and t1, t1, t2
  beqz t1, rtc_inner_loop
  #
  li t2, 0
  sw t2, AON_RTCCOUNT_LO(t0)
  li t2, 0
  sw t2, AON_RTCCOUNT_HI(t0)
  sw a1, AON_RTCCMP0(t0)     # clears ip when write cmp0 > count_lo
  j  rtc_loop

rtc_toggle_done:
  ret

.equ GPIO22, (1UL << 22)

rtc_init: .global rtc_init
  lui t0, GPIO_BASE
  #gpio_init( GPIO22 );
  addi t2, t0, GPIO_IOF_EN      # 0=gpio, 1=uart,spi,i2c,pwm
  li t1, ~(GPIO22)
  amoand.w zero, t1, (t2)       # GPIO22 = GPIO
  #gpio_out( GPIO22 );
  addi t2, t0, GPIO_OUTPUT_EN   # 0=dis, 1=ena
  li t1, (GPIO22)
  amoor.w zero, t1, (t2)        # GPIO22 = out

  li t0, AON_BASE
  addi t1, t0, AON_RTCCFG
  li t2, (1UL << 12)        # [12]=rtcenalways (0=once, 1=continuous)
  amoor.w zero, t2, (t1)
  li t2, ~(0b1111)          # [3:0]=rtcscale
  amoand.w zero, t2, (t1)
  li t2, 0
  sw t2, AON_RTCCOUNT_LO(t0)
  li t2, 0
  sw t2, AON_RTCCOUNT_HI(t0)
  li t2, 150                 # 100=10ms, 200=18ms, 300=XXms, 1000=84ms (2023-06-03 chg 300->150)
  sw t2, AON_RTCCMP0(t0)     # clesrs ip when write cmp0 > count_lo
  ret

rtc_sweep: .global rtc_sweep
  addi sp, sp, -4
  sw ra, 0(sp)
  #
rtc_sweep_loop_in_exp_init:
  addi t3, zero, 200  # was 400
  addi a1, zero, (1UL << 0)  # lowest rate
rtc_sweep_loop_in_exp:
  blez t3, rtc_sweep_loop_out_exp_init
  addi t3, t3, -50
  #
  mv a0, t3
  slli a1, a1, 1   # exponential increase
  #addi a1, a1, 5  # linear increase
  jal rtc_toggle
  j rtc_sweep_loop_in_exp

rtc_sweep_loop_out_exp_init:
  addi t3, zero, 200  # was 400
  addi a1, zero, (1UL << 8)  # highest rate
rtc_sweep_loop_out_exp:
  blez t3, rtc_sweep_loop_in_lin_init
  addi t3, t3, -50
  #
  mv a0, t3
  srli a1, a1, 1   # exponential decrease
  #addi a1, a1, -5  # linear decrease
  jal rtc_toggle
  j rtc_sweep_loop_out_exp

rtc_sweep_loop_in_lin_init:
  addi t3, zero, 200  # was 400
  addi a1, zero, (1UL << 0)  # lowest rate
rtc_sweep_loop_in_lin:
  blez t3, rtc_sweep_loop_out_lin_init
  addi t3, t3, -50
  #
  mv a0, t3
  #slli a1, a1, 1   # exponential increase
  addi a1, a1, 5  # linear increase
  jal rtc_toggle
  j rtc_sweep_loop_in_lin

rtc_sweep_loop_out_lin_init:
  addi t3, zero, 200  # was 400
  addi a1, zero, (1UL << 8)  # highest rate
rtc_sweep_loop_out_lin:
  blez t3, rtc_sweep_done
  addi t3, t3, -50
  #
  mv a0, t3
  #srli a1, a1, 1   # exponential decrease
  addi a1, a1, -5  # linear decrease
  jal rtc_toggle
  j rtc_sweep_loop_out_lin

rtc_sweep_done:
  lw ra, 0(sp)
  addi sp, sp, 4
  ret

