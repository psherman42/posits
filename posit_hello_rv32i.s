	# posit_hello_rv32i.s -- posit4 hello world in pure RV32I
	#
	# Same output as posit_hello.c.
	# No FPU. No posit library. The quire is three ADDs.
	#
	# Assemble and link:
	#   gcc -o posit_hello posit_hello_rv32i.s
	# Run:
	#   ./posit_hello
	#
	# 2026-11-18  pds/claude

	.section .rodata

str_hdr:  .string "posit4  es=0  useed=2\n"
str_row:  .string "%2d   %s    %s\n"
str_sep:  .string "\n1/2 + 3/4 + 3/4:\n"
str_bad:  .string "  without quire: 3/2  (wrong -- should be 2)\n"
str_ok:   .string "  with quire:    %d/16 = %d  (exact)\n"

	# 16 bit-pattern strings
bits_0:  .string "0000"
bits_1:  .string "0001"
bits_2:  .string "0010"
bits_3:  .string "0011"
bits_4:  .string "0100"
bits_5:  .string "0101"
bits_6:  .string "0110"
bits_7:  .string "0111"
bits_8:  .string "1000"
bits_9:  .string "1001"
bits_10: .string "1010"
bits_11: .string "1011"
bits_12: .string "1100"
bits_13: .string "1101"
bits_14: .string "1110"
bits_15: .string "1111"

	# 16 value strings
val_0:  .string "0"
val_1:  .string "1/4"
val_2:  .string "1/2"
val_3:  .string "3/4"
val_4:  .string "1"
val_5:  .string "3/2"
val_6:  .string "2"
val_7:  .string "4"
val_8:  .string "NaR"
val_9:  .string "-4"
val_10: .string "-2"
val_11: .string "-3/2"
val_12: .string "-1"
val_13: .string "-3/4"
val_14: .string "-1/2"
val_15: .string "-1/4"

	.section .data
	.align 2

bits_tbl:
	.word bits_0,  bits_1,  bits_2,  bits_3
	.word bits_4,  bits_5,  bits_6,  bits_7
	.word bits_8,  bits_9,  bits_10, bits_11
	.word bits_12, bits_13, bits_14, bits_15

vals_tbl:
	.word val_0,  val_1,  val_2,  val_3
	.word val_4,  val_5,  val_6,  val_7
	.word val_8,  val_9,  val_10, val_11
	.word val_12, val_13, val_14, val_15

	.section .text
	.globl main
	.type  main, @function

main:
	addi  sp, sp, -32
	sw    ra, 28(sp)
	sw    s0, 24(sp)        # s0 = loop counter i (callee-saved)

	# --- header ---
	la    a0, str_hdr
	call  printf

	# --- table: i = 0 .. 15 ---
	li    s0, 0
.Lloop:
	li    t0, 16
	bge   s0, t0, .Ldone

	slli  t1, s0, 2         # byte offset = i * 4

	la    t2, bits_tbl
	add   t2, t2, t1
	lw    a2, 0(t2)         # bits string

	la    t2, vals_tbl
	add   t2, t2, t1
	lw    a3, 0(t2)         # value string

	la    a0, str_row
	mv    a1, s0            # index i
	call  printf

	addi  s0, s0, 1
	j     .Lloop

.Ldone:
	# --- separator ---
	la    a0, str_sep
	call  printf

	# --- without quire (narrative only) ---
	la    a0, str_bad
	call  printf

	# --- with quire: THIS IS IT ---
	# quire is a 16-bit integer, units of 1/16
	# posit4 value x maps to quire integer x * 16
	#
	#   1/2  ->  8   (0010 in posit4)
	#   3/4  -> 12   (0011 in posit4)
	#   3/4  -> 12   (0011 in posit4)
	#
	li    t0, 0             # quire = 0
	addi  t0, t0,  8        # quire += 1/2
	addi  t0, t0, 12        # quire += 3/4
	addi  t0, t0, 12        # quire += 3/4
	# t0 = 32 = 2 * 16  ->  posit4 value 2  (0110)
	srli  a2, t0, 4         # a2 = quire / 16 = posit result

	la    a0, str_ok
	mv    a1, t0            # 32 (numerator, for "32/16")
	call  printf

	# --- done ---
	lw    ra, 28(sp)
	lw    s0, 24(sp)
	addi  sp, sp, 32
	li    a0, 0
	ret

	.size main, .-main
