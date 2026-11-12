# posit_trig56_rv32i.s -- 56-bit CORDIC sin/cos in RV32I assembly
#
# No FPU. No F. No D. No Zbb. No MUL.
# Pure RV32I: ADD, SUB, SLL, SRL, SRA, BGEZ, BLT, LW, SW, JAL
#
# RV32I ABI:
#   a0-a7: arguments and return values
#   t0-t6: temporaries (caller-saved)
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

# cordic56_rv32i -- one CORDIC iteration (loop body)
# Called 56 times from the unrolled loop
# 
# Register allocation:
#   s0:s1  = x (hi:lo)
#   s2:s3  = y (hi:lo)
#   s4:s5  = z (hi:lo)
#   a0:a1  = atan[i] (hi:lo) -- loaded by caller
#   t0:t1  = y >> i  (hi:lo)
#   t2:t3  = x >> i  (hi:lo)
#   t4     = sign of z (0=pos, 1=neg)

# TODO: full implementation next session
# Sketch of one iteration:
#
#   bgez s5, .pos_N    # branch if z_lo >= 0 (approximate -- need hi too)
#   # z negative: xi=x+(y>>i), yi=y-(x>>i), z+=atan[i]
#   ...
#   .pos_N:
#   # z positive: xi=x-(y>>i), yi=y+(x>>i), z-=atan[i]
#   ...

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

	.globl posit32_sin56_rv32i
posit32_sin56_rv32i:
	# stub
	ret

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
