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
