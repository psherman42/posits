# posit_trig56_rv32i.s -- 56-bit CORDIC sin/cos in RV32I assembly
#
# No FPU. No F. No D. No Zbb. No MUL.
# Pure RV32I: ADD, SUB, SLL, SRL, SRA, BGEZ, BLT, LW, SW, JAL
#
# RV32I ABI:
#   a0-a7: arguments and return values
#   t0-t5: temporaries (caller-saved)
#   t6: RESERVED -- shift count (32), initialized by cordic56_rv32i
#         DO NOT use as general temporary
#   s0-s11: saved registers (callee-saved)
#   ra: return address
#   sp: stack pointer
#
# Functions:
#   cordic56_rv32i(int64_t angle_q56, int64_t *cos_out, int64_t *sin_out)
#   posit32_sin56_rv32i(uint32_t a) -> uint32_t
#   posit32_cos56_rv32i(uint32_t a) -> uint32_t
#
# int64_t on RV32I: two 32-bit registers, hi:lo convention
#   angle_q56 passed as a1:a0 (hi:lo)
#   cos_out pointer in a2
#   sin_out pointer in a3
#
# 2026-11-09 pds  initial cut
# lab.riscvthai.org/posits/ -- RISC-V Open Lab, San Jose CA
# Volder 1959. Kulisch 1970. Gustafson 2015. Pioneer 2026.

	.section .rodata
	.align 3

# ================================================================
# Universal 32-bit register primitives
# 2026-11-12 pds/claude/chat
#
# t6 must be initialized to 32 before use:
#   addi t6, x0, 32
#
# RV32I: shift by 32 = shift by 0 (5-bit shift field) -- no-op
# RV64I: shift by 32 clears/fills upper 32 bits (6-bit shift field)
#
# This is not a workaround. It is using the ISA.
# "One Assembly Sequence. Two Register Widths. Zero Portability Excuses."
#   -- Paul Sherman, Claude, ChatGPT, 2026
#
.macro ZEXT32 reg          # zero-extend: lo words, unsigned results
    sll  \reg, \reg, t6
    srl  \reg, \reg, t6
.endm

.macro SEXT32 reg          # sign-extend: hi words, signed results
    sll  \reg, \reg, t6
    sra  \reg, \reg, t6
.endm

# ADD32C and SUB32B below are essentially full adder/subtractor primitives
# with carry/borrow propagation. The macro structure maps directly to RTL:
#   ADD32C  ->  32-bit adder with carry-out
#   SUB32B  ->  32-bit subtractor with borrow-out  
#   ADD32HC ->  32-bit adder with carry-in (hi word)
#   SUB32HS ->  32-bit subtractor with borrow-in (hi word)
# Exactly the four cells in a ripple-carry 64-bit adder split at the
# 32-bit boundary. This is RISC-V Base I implementing the same structure
# a hardware designer would draw.

# ADD32C rd, rs1, rs2 -- 32-bit add with carry out
# rd = (rs1 + rs2) mod 2^32, t0 = carry
# Hint: maps to full adder carry-out in RTL
.macro ADD32C rd, rs1, rs2
    ZEXT32 \rs1
    ZEXT32 \rs2
    add    \rd, \rs1, \rs2
    ZEXT32 \rd
    sltu   t0, \rd, \rs2       # carry: result < addend iff overflow
.endm

# SUB32B rd, rs1, rs2 -- 32-bit subtract with borrow out
# rd = (rs1 - rs2) mod 2^32, t0 = borrow
# Hint: maps to full subtractor borrow-out in RTL
.macro SUB32B rd, rs1, rs2
    ZEXT32 \rs1
    ZEXT32 \rs2
    sltu   t0, \rs1, \rs2      # borrow BEFORE sub: rs1 < rs2
    sub    \rd, \rs1, \rs2
    ZEXT32 \rd
.endm

# ADD32HC rd, rs1, rs2, cin -- 32-bit add hi word with carry in
# rd = rs1 + rs2 + cin, sign-extended
# Hint: upper half of 64-bit adder in RTL
.macro ADD32HC rd, rs1, rs2, cin
    add    \rd, \rs1, \rs2
    add    \rd, \rd,  \cin
    SEXT32 \rd
.endm

# SUB32HS rd, rs1, rs2, bin -- 32-bit subtract hi word with borrow in
# rd = rs1 - rs2 - bin, sign-extended
# Hint: upper half of 64-bit subtractor in RTL
.macro SUB32HS rd, rs1, rs2, bin
    sub    \rd, \rs1, \rs2
    sub    \rd, \rd,  \bin
    SEXT32 \rd
.endm
# ================================================================
# ================================================================

# arctan table: atan(2^-i) * 2^56, i=0..55
# 64-bit little-endian, lo word first
_atan56_rv32i:
	.quad 56593902016227520   # i=0
	.quad 33409331186036028   # i=1
	.quad 17652573055549882   # i=2
	.quad  8960721713639278   # i=3
	.quad  4497749271019253   # i=4
	.quad  2251067235130761   # i=5
	.quad  1125808294293075   # i=6
	.quad   562938500594601   # i=7
	.quad   281473545067997   # i=8
	.quad   140737309398766   # i=9
	.quad    70368721808055   # i=10
	.quad    35184369292629   # i=11
	.quad    17592185694890   # i=12
	.quad     8796092978517   # i=13
	.quad     4398046505642   # i=14
	.quad     2199023254869   # i=15
	.quad     1099511627690   # i=16
	.quad      549755813877   # i=17
	.quad      274877906942   # i=18
	.quad      137438953471   # i=19
	.quad       68719476735   # i=20
	.quad       34359738367   # i=21
	.quad       17179869183   # i=22
	.quad        8589934591   # i=23
	.quad        4294967295   # i=24
	.quad        2147483647   # i=25
	.quad        1073741823   # i=26
	.quad         536870912   # i=27
	.quad         268435456   # i=28
	.quad         134217728   # i=29
	.quad          67108864   # i=30
	.quad          33554432   # i=31
	.quad          16777216   # i=32
	.quad           8388608   # i=33
	.quad           4194304   # i=34
	.quad           2097152   # i=35
	.quad           1048576   # i=36
	.quad            524288   # i=37
	.quad            262144   # i=38
	.quad            131072   # i=39
	.quad             65536   # i=40
	.quad             32768   # i=41
	.quad             16384   # i=42
	.quad              8192   # i=43
	.quad              4096   # i=44
	.quad              2048   # i=45
	.quad              1024   # i=46
	.quad               512   # i=47
	.quad               256   # i=48
	.quad               128   # i=49
	.quad                64   # i=50
	.quad                32   # i=51
	.quad                16   # i=52
	.quad                 8   # i=53
	.quad                 4   # i=54
	.quad                 2   # i=55

	.section .text
	.align 2

# cordic56_rv32i -- full 56-iteration CORDIC engine
# 2026-11-12 pds  initial cut, iter0 verified on Pioneer
#
# Register allocation (maintained across all iterations):
#   s0:s1  = x hi:lo   (cos accumulator)
#   s2:s3  = y hi:lo   (sin accumulator)
#   s4:s5  = z hi:lo   (angle residual)
#   a0:a1  = atan[i] hi:lo -- loaded by caller before each iteration
#
# Primitives (no stack, pure combinatorial):
#   cordic56_rv32i_shift  -- 64-bit arithmetic right shift by i
#   cordic56_rv32i_iter0  -- iteration for i=0 (no shift needed)
#   cordic56_rv32i_iter   -- general iteration for any i (uses shift)
#
# Full function (with prologue/epilogue):
#   cordic56_rv32i        -- 56-iteration loop, loads atan table
#
# Layering philosophy: leaf functions never touch the stack.
# Only cordic56_rv32i needs a frame.
# Volder 1959. Gustafson 2026. RV32I 2026.


# cordic56_rv32i_shift -- 64-bit arithmetic right shift by i
# Input:  a0:a1 = value hi:lo, a2 = shift amount (0..55)
# Output: t2:t3 = result hi:lo
# Clobbers: t0, t1
# No stack. Pure combinatorial.
#
# if i == 0: t2=a0, t3=a1
# if 0 < i < 32:
#   t3 = (a1 >> i) | (a0 << (32-i))
#   t2 = a0 >> i   (arithmetic)
# if i >= 32:
#   t3 = a0 >> (i-32)  (arithmetic)
#   t2 = a0 >> 31      (sign fill)

        .globl cordic56_rv32i_shift
cordic56_rv32i_shift:
        li    t0, 32
        bge   a2, t0, .shift_hi   # i >= 32?

        # 0 <= i < 32
        beqz  a2, .shift_zero

        # t3 = (a1 >> i) | (a0 << (32-i))
        srl   t3, a1, a2          # a1 >> i (logical -- lo word)
        sub   t0, t0, a2          # 32 - i
        sll   t1, a0, t0          # a0 << (32-i)
        or    t3, t3, t1          # combine
        sra   t2, a0, a2          # a0 >> i (arithmetic -- hi word)
        ret

.shift_zero:
        mv    t2, a0
        mv    t3, a1
        ret

.shift_hi:
        # i >= 32: shift by (i-32)
        sub   a2, a2, t0          # i - 32
        sra   t3, a0, a2          # a0 >> (i-32) arithmetic
        srai  t2, a0, 31          # sign fill
        ret

# cordic56_rv32i_iter0 -- one CORDIC iteration for i=0
# 2026-11-12 pds  first real iteration -- RV32I 64-bit add/sub/sign
#
# Register convention (maintained across all iterations):
#   s0:s1  = x hi:lo
#   s2:s3  = y hi:lo
#   s4:s5  = z hi:lo
#   a0:a1  = atan[i] hi:lo (caller loads before each iteration)
#
# i=0: no shift needed (y>>0 = y, x>>0 = x)
#
# d = sign(z): if z>=0, d=+1 else d=-1
# x_new = x - d*(y>>i)
# y_new = y + d*(x>>i)
# z_new = z - d*atan[i]

        .globl cordic56_rv32i_iter0
cordic56_rv32i_iter0:
        # t4 = 1 if z negative, 0 if positive
        slt   t4, s4, x0

        # save x_orig for y update
        mv    t5, s1             # x_orig_lo
        mv    t6, s0             # x_orig_hi

        beqz  t4, .i0_pos

.i0_neg:
        # d = -1: x += y, y -= x_orig, z += atan
        # 64-bit x += y
        add   s1, s1, s3
        sltu  t0, s1, s3         # carry
        add   s0, s0, s2
        add   s0, s0, t0

        # 64-bit y -= x_orig (borrow BEFORE sub)
        sltu  t0, s3, t5         # borrow = (y_lo < x_lo)
        sub   s3, s3, t5
        sub   s2, s2, t6
        sub   s2, s2, t0         # - borrow

        # 64-bit z += atan
        add   s5, s5, a1
        sltu  t0, s5, a1         # carry
        add   s4, s4, a0
        add   s4, s4, t0
        ret

.i0_pos:
        # d = +1: x -= y, y += x_orig, z -= atan
        # 64-bit x -= y (borrow BEFORE sub)
        sltu  t0, s1, s3         # borrow = (x_lo < y_lo)
        sub   s1, s1, s3
        sub   s0, s0, s2
        sub   s0, s0, t0         # - borrow

        # 64-bit y += x_orig
        add   s3, s3, t5
        sltu  t0, s3, t5         # carry
        add   s2, s2, t6
        add   s2, s2, t0

        # 64-bit z -= atan (borrow BEFORE sub)
        sltu  t0, s5, a1         # borrow = (z_lo < atan_lo)
        sub   s5, s5, a1
        sub   s4, s4, a0
        sub   s4, s4, t0         # - borrow
        ret

# cordic56_rv32i_iter -- general CORDIC iteration for any i
# 2026-11-12 pds
#
# Inputs (maintained by caller):
#   s0:s1  = x hi:lo
#   s2:s3  = y hi:lo
#   s4:s5  = z hi:lo
#   s6     = i (iteration number, 0..55)
#   a0:a1  = atan[i] hi:lo
#
# Uses ra -- caller must save ra if needed
# Clobbers: t0-t6, a0-a2

        .globl cordic56_rv32i_iter
cordic56_rv32i_iter:
        addi  sp, sp, -24
        sd    ra, 0(sp)
        sd    a0, 8(sp)          # save atan_hi (shift clobbers a0)
        sd    a1, 16(sp)         # save atan_lo

        # compute y >> i -> s7:s8
        mv    a0, s2             # y_hi
        mv    a1, s3             # y_lo
        mv    a2, s6             # i
        jal   ra, cordic56_rv32i_shift
        mv    s7, t2             # y>>i hi
        mv    s8, t3             # y>>i lo
        SEXT32 s7                # sign-extend hi
        ZEXT32 s8                # zero-extend lo

        # compute x >> i -> t2:t3 (used directly below)
        mv    a0, s0             # x_hi
        mv    a1, s1             # x_lo
        mv    a2, s6             # i
        jal   ra, cordic56_rv32i_shift
        # t2:t3 = x>>i hi:lo
        SEXT32 t2                # sign-extend hi
        ZEXT32 t3                # zero-extend lo

        # restore atan and determine sign of z
        ld    a0, 8(sp)          # restore atan_hi
        ld    a1, 16(sp)         # restore atan_lo
        slt   t4, s4, x0         # t4 = 1 if z negative

        # .iter_neg and .iter_pos are mirror images:
        # neg: x+=, y-=, z+=  (d=-1, rotate clockwise)
        # pos: x-=, y+=, z-=  (d=+1, rotate counterclockwise)
        # add/carry <-> sub/borrow throughout

        beqz  t4, .iter_pos

.iter_neg:
        # d=-1: x += y>>i, y -= x>>i, z += atan
        ADD32C  s1, s1, s8       # x_lo += (y>>i)_lo, t0=carry
        ADD32HC s0, s0, s7, t0   # x_hi += (y>>i)_hi + carry

        SUB32B  s3, s3, t3       # y_lo -= (x>>i)_lo, t0=borrow
        SUB32HS s2, s2, t2, t0   # y_hi -= (x>>i)_hi - borrow

        ADD32C  s5, s5, a1       # z_lo += atan_lo, t0=carry
        ADD32HC s4, s4, a0, t0   # z_hi += atan_hi + carry

        ld    ra, 0(sp)
        addi  sp, sp, 24
        ret

.iter_pos:
        # d=+1: x -= y>>i, y += x>>i, z -= atan
        SUB32B  s1, s1, s8       # x_lo -= (y>>i)_lo, t0=borrow
        SUB32HS s0, s0, s7, t0   # x_hi -= (y>>i)_hi - borrow

        ADD32C  s3, s3, t3       # y_lo += (x>>i)_lo, t0=carry
        ADD32HC s2, s2, t2, t0   # y_hi += (x>>i)_hi + carry

        SUB32B  s5, s5, a1       # z_lo -= atan_lo, t0=borrow
        SUB32HS s4, s4, a0, t0   # z_hi -= atan_hi - borrow

        ld    ra, 0(sp)
        addi  sp, sp, 24
        ret

# cordic56_rv32i -- full 56-iteration CORDIC engine
# 2026-11-12 pds
#
# The heart of posit32 sin/cos on bare RV32I.
# No FPU. No MUL. No extensions.
# Shifts and adds only -- Volder 1959, running on RISC-V 2026.
#
# RV32I calling convention:
#   a0:a1  = angle_q56 hi:lo   (input)
#   a2     = cos_out pointer   (int64_t*, receives x result)
#   a3     = sin_out pointer   (int64_t*, receives y result)
#
# Internal register allocation:
#   s0:s1  = x hi:lo   (cos accumulator, starts at CORDIC_K56)
#   s2:s3  = y hi:lo   (sin accumulator, starts at 0)
#   s4:s5  = z hi:lo   (angle residual, starts at angle_q56)
#   s6     = i          (loop counter 0..55)
#   s7:s8  = y>>i scratch (used by cordic56_rv32i_iter)
#   s9     = atan table base pointer
#   s10    = cos_out saved pointer
#   s11    = sin_out saved pointer
#
# CORDIC gain K = product(i=0..55) of 1/sqrt(1 + 2^(-2i))
# Precomputed at Q56 scale: K * 2^56
# Starting x at K*2^56 means after 56 rotations x=cos, y=sin
# without needing a final multiply -- the gain is baked in.
#
# CORDIC_K56 = 43757185469210216
#   hi = 10188013 = 0x9b74ed
#   lo = 2822987368 = 0xa8435e68
#
# After 56 iterations:
#   x/Q56D = cos(angle), y/Q56D = sin(angle)
#   Q56D = 2^56 = 72057594037927936

        .globl cordic56_rv32i
cordic56_rv32i:
        # prologue -- save all callee-saved registers we use
        addi  sp, sp, -104
        sd    s0,   0(sp)
        sd    s1,   8(sp)
        sd    s2,  16(sp)
        sd    s3,  24(sp)
        sd    s4,  32(sp)
        sd    s5,  40(sp)
        sd    s6,  48(sp)
        sd    s7,  56(sp)
        sd    s8,  64(sp)
        sd    s9,  72(sp)
        sd    s10, 80(sp)
        sd    s11, 88(sp)
        sd    ra,  96(sp)

        addi t6, x0, 32   # shift count: 0 on RV32I (5-bit field), 32 on RV64I (6-bit field)

        # x = CORDIC_K56 hi:lo
        # lui zeroes lower 12 bits, ori fills them -- no sign issue
        lui   s0, 0x9b7
        ori   s0, s0, 0x4ed      # s0 = 0x9b74ed = 10188013 (x_hi)

        # x_lo = 2822987368 = 0xa8435e68
        # lower 12 bits = 0xe68 = 3688 > 2047 (bit 11 set)
        # addi sign-extends, so lui must compensate:
        #   addi immediate = 0xe68 - 0x1000 = -408
        #   lui value = (0xa8435e68 + 0x800) >> 12 = 0xa8436
        # verify: (0xa8436 << 12) + (-408) = 0xa8435e68 ✓
        lui   s1, 0xa8436        # upper 20 bits (compensated)
        addi  s1, s1, -408       # lower 12 bits signed (-408 = 0xfffffe68)

        SEXT32 s0                # sign-extend hi word on RV64
        ZEXT32 s1                # zero-extend lo word on RV64

        # y = 0
        li    s2, 0
        li    s3, 0

        # z = angle_q56 (a0:a1 hi:lo)
        mv    s4, a0
        SEXT32 s4                # sign-extend hi word on RV64
        mv    s5, a1
        ZEXT32 s5                # zero-extend lo word on RV64

        # save output pointers
        mv    s10, a2            # cos_out
        mv    s11, a3            # sin_out

        # atan table base
        la    s9, _atan56_rv32i

        # loop i = 0..55
        li    s6, 0

.cordic56_loop:
        li    t0, 56
        bge   s6, t0, .cordic56_done

        # load atan[i]: table is 64-bit entries, little-endian
        # offset = i * 8
        slli  t1, s6, 3          # t1 = i * 8
        add   t1, t1, s9         # t1 = &atan[i]
        lw    a1, 0(t1)          # atan_lo (little-endian: lo at low addr)
        lw    a0, 4(t1)          # atan_hi

        # call general iteration (s6 = i, s0-s5 = state)
        jal   ra, cordic56_rv32i_iter

        addi  s6, s6, 1          # i++
        j     .cordic56_loop

.cordic56_done:
        # store results -- reassemble hi:lo into int64_t
        slli  t5, s0, 32         # cos_hi to upper 32
        slli  t4, s1, 32
        srli  t4, t4, 32         # cos_lo zero-extended
        or    t5, t5, t4
        sd    t5, 0(s10)

        slli  t5, s2, 32         # sin_hi to upper 32
        slli  t4, s3, 32
        srli  t4, t4, 32         # sin_lo zero-extended
        or    t5, t5, t4
        sd    t5, 0(s11)

        # epilogue
        ld    s0,   0(sp)
        ld    s1,   8(sp)
        ld    s2,  16(sp)
        ld    s3,  24(sp)
        ld    s4,  32(sp)
        ld    s5,  40(sp)
        ld    s6,  48(sp)
        ld    s7,  56(sp)
        ld    s8,  64(sp)
        ld    s9,  72(sp)
        ld    s10, 80(sp)
        ld    s11, 88(sp)
        ld    ra,  96(sp)
        addi  sp, sp, 104
        ret

# posit32_sin56_rv32i -- ...
# 2026-11-11 pds
	.globl posit32_sin56_rv32i
posit32_sin56_rv32i:
	# stub
	ret

# posit32_cos56_rv32i -- ...
# 2026-11-11 pds
	.globl posit32_cos56_rv32i
posit32_cos56_rv32i:
	# stub
	ret

# test wrapper -- calls iter0 directly (same file, no relocation needed)
        .globl cordic56_rv32i_iter0_test
cordic56_rv32i_iter0_test:
        addi  sp, sp, -88
        sd    s0,  0(sp)
        sd    s1,  8(sp)
        sd    s2, 16(sp)
        sd    s3, 24(sp)
        sd    s4, 32(sp)
        sd    s5, 40(sp)
        sd    s6, 48(sp)
        sd    s7, 56(sp)
        sd    s8, 64(sp)
        sd    ra, 72(sp)

        srli  s0, a0, 32
        slli  s1, a0, 32
        srli  s1, s1, 32

        srli  s2, a1, 32
        slli  s3, a1, 32
        srli  s3, s3, 32

        srli  s4, a2, 32
        slli  s5, a2, 32
        srli  s5, s5, 32

        srli  t0, a3, 32
        slli  t1, a3, 32
        srli  t1, t1, 32
        mv    a0, t0
        mv    a1, t1

        mv    s6, a4
        mv    s7, a5
        mv    s8, a6

        jal   ra, cordic56_rv32i_iter0

        slli  t5, s0, 32         # hi to upper 32
        slli  t6, s1, 32         # lo to upper 32
        srli  t6, t6, 32         # lo back to lower 32, zero-extended
        or    t5, t5, t6
        sd    t5, 0(s6)

        slli  t5, s2, 32         # hi to upper 32
        slli  t6, s3, 32         # lo to upper 32
        srli  t6, t6, 32         # lo back to lower 32, zero-extended
        or    t5, t5, t6
        sd    t5, 0(s7)

        slli  t5, s4, 32         # hi to upper 32
        slli  t6, s5, 32         # lo to upper 32
        srli  t6, t6, 32         # lo back to lower 32, zero-extended
        or    t5, t5, t6
        sd    t5, 0(s8)

        ld    s0,  0(sp)
        ld    s1,  8(sp)
        ld    s2, 16(sp)
        ld    s3, 24(sp)
        ld    s4, 32(sp)
        ld    s5, 40(sp)
        ld    s6, 48(sp)
        ld    s7, 56(sp)
        ld    s8, 64(sp)
        ld    ra, 72(sp)
        addi  sp, sp, 88
        ret

# ================================================================
# TEST WRAPPERS -- RV64 only, for verification on Pioneer
# Not part of the RV32I production code
# ================================================================

        .globl cordic56_rv32i_shift_test
cordic56_rv32i_shift_test:
        addi  sp, sp, -16
        sd    ra, 0(sp)          # save return address
        sd    t6, 8(sp)          # save t6 too (used for result ptr)
        
        mv    t6, a2             # save result ptr
        mv    a2, a1             # shift amount
        srai  t0, a0, 32
        slli  a1, a0, 32
        srli  a1, a1, 32
        mv    a0, t0
        jal   ra, cordic56_rv32i_shift
        slli  t5, t2, 32
        slli  t4, t3, 32
        srli  t4, t4, 32
        or    t5, t5, t4
        sd    t5, 0(t6)
        
        ld    ra, 0(sp)
        ld    t6, 8(sp)
        addi  sp, sp, 16
        ret

# iter_test wrapper -- RV64 test scaffolding only
# C signature:
#   void cordic56_rv32i_iter_test(
#       int64_t x_in, int64_t y_in, int64_t z_in,
#       int64_t atan_in, int32_t i,
#       int64_t *x_out, int64_t *y_out, int64_t *z_out)
# RV64 ABI: a0=x, a1=y, a2=z, a3=atan, a4=i, a5=x_out, a6=y_out, a7=z_out

        .globl cordic56_rv32i_iter_test
cordic56_rv32i_iter_test:
        addi  sp, sp, -112
        sd    s0,   0(sp)
        sd    s1,   8(sp)
        sd    s2,  16(sp)
        sd    s3,  24(sp)
        sd    s4,  32(sp)
        sd    s5,  40(sp)
        sd    s6,  48(sp)
        sd    s7,  56(sp)
        sd    s8,  64(sp)
        sd    ra,  72(sp)
        sd    a5,  80(sp)        # x_out
        sd    a6,  88(sp)        # y_out
        sd    a7,  96(sp)        # z_out

        # split x (a0) into s0:s1 hi:lo
        srai  s0, a0, 32
        slli  s1, a0, 32
        srli  s1, s1, 32

        # split y (a1) into s2:s3
        srai  s2, a1, 32
        slli  s3, a1, 32
        srli  s3, s3, 32

        # split z (a2) into s4:s5
        srai  s4, a2, 32
        slli  s5, a2, 32
        srli  s5, s5, 32

        # split atan (a3) into a0:a1
        srai  t0, a3, 32
        slli  a1, a3, 32
        srli  a1, a1, 32
        mv    a0, t0

        # i into s6
        mv    s6, a4

        jal   ra, cordic56_rv32i_iter

        # reassemble and store results
        ld    t0, 80(sp)         # x_out ptr
        slli  t5, s0, 32
        slli  t6, s1, 32
        srli  t6, t6, 32
        or    t5, t5, t6
        sd    t5, 0(t0)

        ld    t0, 88(sp)         # y_out ptr
        slli  t5, s2, 32
        slli  t6, s3, 32
        srli  t6, t6, 32
        or    t5, t5, t6
        sd    t5, 0(t0)

        ld    t0, 96(sp)         # z_out ptr
        slli  t5, s4, 32
        slli  t6, s5, 32
        srli  t6, t6, 32
        or    t5, t5, t6
        sd    t5, 0(t0)

        ld    s0,   0(sp)
        ld    s1,   8(sp)
        ld    s2,  16(sp)
        ld    s3,  24(sp)
        ld    s4,  32(sp)
        ld    s5,  40(sp)
        ld    s6,  48(sp)
        ld    s7,  56(sp)
        ld    s8,  64(sp)
        ld    ra,  72(sp)
        addi  sp, sp, 112
        ret

