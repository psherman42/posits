#!/bin/sh
# setup_fe310_branch.sh
#
# Run this on Pioneer from ~/posits to create the fe310 branch
# and populate it with the FE310/LoFive bare-metal HAL.
#
# Usage:
#   cd ~/posits
#   sh setup_fe310_branch.sh
#
# 2026-11-17  pds/claude

set -e

echo "=== fe310 branch setup ==="

# 1. Create branch off rv32i-asm
git checkout rv32i-asm
git checkout -b fe310
echo "--- branch fe310 created off rv32i-asm"

# 2. The HAL files you attached are already in this working tree
#    from rv32i-asm -- if not, copy from your FE310 source tree.
#    New split files need to be copied in from wherever scp put them.
#
#    Expected flat layout (all in ~/posits/):
#
#    Platform:
#      start.s  csr.e  gpio.e  pcba-lofive-r1.e
#      pmu.s    pmu.e
#      gpio.c   gpio.h
#      spi1.c   spi1.h
#      oled.c
#      math.c   util.c
#      clock.c  clock.h       (from your existing FE310 tree)
#      fixfft.c fixfft.h      (new: pure FFT lib, no main)
#      sinetab.h              (renamed from sinetab.txt)
#      i2s.c    i2s.h         (new: split from fixfft.c)
#      pwm.c    pwm.h         (new: split from fixfft.c)
#      timer.c  timer.h       (new: split from fixfft.c)
#      fe310-g002-ram.lds
#      fe310-g002-rom.lds
#      fft_demo.c             (new: main() from fixfft.c)
#      fft_demo.mk            (see below)
#
#    Posit headers (symlinks from rv32i-asm):
#      quire32.h posit_trig56.h bposit_quire.h bposit_quire_double.h
#      (these are already here from rv32i-asm)
#
#    Apps (stub .mk files, .c to be written):
#      pitch.mk  flicker.mk  bandleader.mk
#      wumpus.mk  maze.mk  stego.mk

# 3. Fix vi accident in fft.mk line 6
#    Original line 6 contains: Jjjjjj:q  (vi keystrokes pasted in)
#    Safe to remove; it was in the AOPS context but is not a valid flag
sed -i '/^Jjjjjj:q/d' fft.mk && echo "--- fft.mk vi accident removed"

# 4. Create fft_demo.mk (the first working app)
cat > fft_demo.mk << 'MKEOF'
##############################
# fft_demo.mk -- FFT spectrum display demo, LoFive R1
#
# 2021-09-12  pds  original fft.mk (jcsse demo)
# 2026-11-17  pds/claude  renamed; i2s/pwm/timer split out
#

RVGNU = riscv64-unknown-elf
AOPS  = -march=rv32imac -mabi=ilp32
COPS  = -march=rv32imac -mabi=ilp32 -Wall -O2 -nostdlib -nostartfiles -ffreestanding
LKOPS = -b elf32-littleriscv

PGM = fft_demo

OBJECTS = start.o pmu.o prci.o gpio.o spi1.o oled.o util.o \
          math.o fixfft.o i2s.o pwm.o timer.o fft_demo.o

start.o   : start.s   ; $(RVGNU)-as  $(AOPS) $< -o $@
pmu.o     : pmu.s     ; $(RVGNU)-as  $(AOPS) $< -o $@
prci.o    : prci.s    ; $(RVGNU)-as  $(AOPS) $< -o $@
gpio.o    : gpio.c    ; $(RVGNU)-gcc $(COPS) -c $< -o $@
spi1.o    : spi1.c    ; $(RVGNU)-gcc $(COPS) -c $< -o $@
oled.o    : oled.c    ; $(RVGNU)-gcc $(COPS) -c $< -o $@
util.o    : util.c    ; $(RVGNU)-gcc $(COPS) -c $< -o $@
math.o    : math.c    ; $(RVGNU)-gcc $(COPS) -c $< -o $@
fixfft.o  : fixfft.c  ; $(RVGNU)-gcc $(COPS) -c $< -o $@
i2s.o     : i2s.c     ; $(RVGNU)-gcc $(COPS) -c $< -o $@
pwm.o     : pwm.c     ; $(RVGNU)-gcc $(COPS) -c $< -o $@
timer.o   : timer.c   ; $(RVGNU)-gcc $(COPS) -c $< -o $@
fft_demo.o: fft_demo.c; $(RVGNU)-gcc $(COPS) -c $< -o $@

$(PGM)-ram.elf: fe310-g002-ram.lds $(OBJECTS)
	$(RVGNU)-ld $(OBJECTS) -T fe310-g002-ram.lds -o $@ -Map $(PGM)-ram.map $(LKOPS)
	$(RVGNU)-objdump -D $@ > $(PGM)-ram.lst

$(PGM)-rom.elf: fe310-g002-rom.lds $(OBJECTS)
	$(RVGNU)-ld $(OBJECTS) -T fe310-g002-rom.lds -o $@ -Map $(PGM)-rom.map $(LKOPS)
	$(RVGNU)-objdump -D $@ > $(PGM)-rom.lst

$(PGM)-ram.bin: $(PGM)-ram.elf
	$(RVGNU)-objcopy $< -O binary $@

$(PGM)-rom.bin: $(PGM)-rom.elf
	$(RVGNU)-objcopy $< -O binary $@

clean:
	rm -f *.o $(PGM)-ram.* $(PGM)-rom.*
MKEOF
echo "--- fft_demo.mk created"

# 5. Stub .mk files for future apps
for app in pitch flicker bandleader wumpus maze stego; do
  cat > ${app}.mk << APPEOF
##############################
# ${app}.mk -- TODO
#
# OBJECTS = start.o pmu.o prci.o gpio.o spi1.o oled.o util.o math.o \\
#           fixfft.o i2s.o pwm.o timer.o ${app}.o

PGM = ${app}
APPEOF
  echo "--- ${app}.mk stub created"
done

# 6. Initial commit
git add -A
git commit -m "fe310: initial HAL + I2S/PWM/timer split from fixfft.c

- Split fixfft.c: pure FFT lib + fixfft.h
- New i2s.c / i2s.h: GPIO bit-bang I2S mic driver
- New pwm.c / pwm.h: PWM1/PWM2 clock gen for I2S
- New timer.c / timer.h: CLINT/PLIC trap dispatch
- New fft_demo.c: original main() extracted
- New fft_demo.mk: clean per-app flat makefile
- Stub .mk files for pitch/flicker/bandleader/wumpus/maze/stego
- sinetab.h: renamed from sinetab.txt (header guard added)
- fft.mk: removed vi accident (Jjjjjj:q on line 6)

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_017gB9G1RXGgVXjYnmjEMhjc"

echo "=== done. fe310 branch ready ==="
echo ""
echo "NOTE: clock.c / clock.h not found in attachments."
echo "Copy from your existing FE310 source tree before building."
echo "clock_init(PRCI_EXT_DIR) must return uint32_t hfclk_hz."
