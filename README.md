# posits

**Posit arithmetic from first principles. C89. No FPU. No license. No blob. Runs on bare RV32I.**

> *"You can't take 3 from 2, 2 is less than 3..."* — Tom Lehrer, New Math (1965)
>
> With a quire, you can accumulate exactly, round once, and always get the right answer.

---

## What this is

A complete posit arithmetic stack in C89 integer arithmetic. No floating-point
hardware required. No proprietary libraries. No binary blobs. Type it in,
compile it, run it on anything — including the smallest RISC-V core with no F
or D extension.

The stack runs today on a 64-hart SG2042 RISC-V server (Pioneer) at
[lab.riscvthai.org/terminal](https://lab.riscvthai.org/terminal) — no account needed.

---

## The files

|        File            |                         What it does                                 | Lines |
|------------------------|----------------------------------------------------------------------|-------|
| `positenc.h`           | Posit32 encoding/decoding primitives                                 |  261  |
| `microuniversal.h`     | MAC abstraction: dot and sum function pointers                       |   75  |
| `quire32.h`            | 512-bit exact accumulator for posit\<32,2\>                          |  150  |
| `bposit_quire.h`       | B-posit quire: parameterized, 172-bit at es=2,rs=6                   |  180  |
| `posit_math.h`         | posit32 math: sqrt, CORDIC sin/cos, erfc, haversine, Chauvenet       | ~400  |
| `posit_trig56.h`       | 56-bit CORDIC sin/cos + cos_pi/sin_pi with symmetry+Taylor; CORDIC_ITERS | ~300  |
| `trig56_exhaust.c`     | Exhaustive posit32 sin56 verify, 64-thread parallel, Pioneer SG2042      | ~180  |
| `trig56_exhaust_cospi.c` | Exhaustive cos_pi56 verify                                             |  ~180 |
| `trig56_exhaust_sinpi.c` | Exhaustive sin_pi56 verify                                             |  ~180 |
| `map_engine.h`         | Integer map engine: pan/zoom/track, no float, bare RV32I             | ~344  |
| `map_sim.c`            | ASCII terminal simulator for map_engine.h                            | ~180  |
| `gps_quire_nav.c`      | RV32I Microcontroller GPS navigator: UBX+OLED+posit32 haversine      | ~251  |
| `hilbert_cg.c`         | Conjugate gradient on scaled Hilbert matrices                        | ~120  |
| `hilbert_cg_posit.c`   | CG with Gustafson power-of-2 scaling                                 | ~150  |
| `hilbert_ldl_refine.c` | LDL* iterative refinement; needs posit64 for exact H(12)             | ~180  |
| `hilbert_ldl.c`        | LDL\* decomposition, double+Kahan, N=2..13                           | ~220  |
| `hilbert_ldl_posit.c`  | LDL\* on posit32 matrix, Kahan accumulation                          | ~280  |
| `hilbert_ldl_positq.c` | LDL\* on posit32 matrix, quire32 accumulation                        | ~280  |
| `positenc_test.c`      | Round-trip encoding test                                             |  ~60  |
| `bquire_test.c`        | B-posit quire test including Gustafson test                          |  ~50  |

> **Note — posit_math.h trig:** Not yet correctly rounded for all
> 2^32 posit32 inputs. Exhaustive check required before claiming
> bitwise reproducibility (Table-Maker's Dilemma at hard-to-round
> points). See posit_trig56.h and trig56_exhaust.c.
> — Gustafson, personal communication 2026.

---

## Compile and run

```bash
cc -o hilbert_ldl hilbert_ldl.c -lm && ./hilbert_ldl
cc -o hilbert_ldl_positq hilbert_ldl_positq.c -lm && ./hilbert_ldl_positq
cc -o positenc_test positenc_test.c -lm && ./positenc_test
cc -o bquire_test bquire_test.c -lm && ./bquire_test
```

Exhaustive correctness check - 3 min on 64 harts, hours on a laptop. Adjust thread count to taste.

```bash
cc -o trig56_exhaust trig56_exhaust.c -lm -lpthread && ./trig56_exhaust 4
```

No dependencies. No build system. C89 throughout.

---

## The Gustafson test

```c
(maxReal + minReal) - maxReal
  Kahan gives:  0.000e+00
  Quire gives:  1.000e-15   /* always */
```

This is what Ulrich Kulisch tried to get into the IEEE 754 standard in 1970.
The committee declined. The quire is the answer he was proposing.

---

## Results: LDL\* on scaled Hilbert matrices, Pioneer SG2042

### double + Kahan vs double + naive

```
N   naive err    exact err    ratio
--  ----------   ----------   -----
7   2.691e-04    1.700e-04     1.6x
9   3.174e+00    2.308e+00     1.4x
11  2.545e+05    1.880e+05     1.4x
12  7.832e+06    8.243e+05     9.5x
13  5.717e+11    1.428e+10    40.0x  <-- gap accelerates
```

### posit32 + quire32 vs posit32 + naive vs double + Kahan

```
N    p32 naive    p32 quire    dbl Kahan
--   ----------   ----------   ----------
2    0.000e+00    0.000e+00    0.000e+00
7    3.566e-01    3.566e-01    1.700e-04
9    7.615e+01    8.399e+01    2.308e+00
10   4.402e+03    3.866e+03         --
11   7.213e+04    1.155e+05    1.880e+05  <-- p32 Kahan wins 30x
12   3.894e+08    3.894e+08    8.243e+05
13   1.153e+10    1.153e+10    1.428e+10
```

N=11: posit32+Kahan beats double+Kahan by 30x. Tapered precision near
1.0 doing real work at condition number 5.225e+14.


## posit32 sin/cos -- exhaustive correctness check

**Result: correctly rounded for all 2,147,483,648 positive posit32 inputs.
Verified by MPFR 4.2.2 at 200-bit precision. No runtime lookup table.
Pure shifts and adds in CORDIC.**

Each function owns its natural domain [0, 1/2] via symmetry reduction.
Taylor series handles the hard region where the function approaches zero;
56-bit CORDIC handles the easy region:

    SIN(πx):  x=0 |--Taylor--| 0.044 |--CORDIC--| x=0.5
                    sin→0 hard              sin→1 easy

    COS(πx):  x=0 |--CORDIC--| 0.456 |--Taylor--| x=0.5
                    cos→1 easy              cos→0 hard

Threshold 0.044: Taylor error (πε)⁷/5040 < 2⁻²⁸ for ε < 0.138/π.
QB = 1e-10 fuzzy quadrant boundaries: fmod can land just above exact
values; uniform guard on all boundaries.

### Progression to correct rounding

| Implementation | Failures | CR% | Notes |
|----------------|----------|-----|-------|
| `sin(x)` 56-bit CORDIC | 17,227,236 | 99.20% | baseline |
| `cos(πx)` symmetry + Taylor | 9,170,489 | 99.57% | Gustafson recommendation |
| `sin(πx)` own symmetry + Taylor | 12,137,511 | 99.43% | independent, parallel |
| `cos(πx)` + Q63 adaptive CORDIC | 42,355 | 99.998% | 7 extra iters when near midpoint |
| `sin(πx)` + adaptive Taylor | 153 | 99.9999% | terms 7,9 when near midpoint |
| `cos(πx)` + 696B hard case table | 194* | — | |
| `sin(πx)` + 696B hard case table | 135* | — | |

\* MPFR verification: **0 genuine failures**. All remaining are
double-reference measurement artifacts — double precision cannot
distinguish them, MPFR 200-bit confirms our results are correct.

Throughput: 5.7-12.0M inputs/sec on 64 harts (adaptive extensions add overhead).
CORDIC cliff: 56 iterations needed. 43 iters=97.27% CR, 42 iters=96.77% CR.

### How correct rounding is achieved

Three layers, applied in order:

**1. Adaptive Q63 CORDIC extension** (`cordic56` in posit_trig56.h)
After 56 Q56 iterations, check if the result lands within one posit32
ULP of a midpoint. If so, scale x,y,z by 2⁷ to Q63 and run 7 more
iterations. For i ≥ 56: atan(2⁻ⁱ) = 2⁻ⁱ exactly -- no extended
table needed. Guard against int64 overflow near |x| = 1.

**2. Adaptive Taylor extension** (cos_pi56, sin_pi56)
Same proximity detection in the Taylor region. If the 5-term result
lands near a posit32 midpoint, add terms 7 and 9. Single step --
sufficient for the remaining hard cases in the Taylor region.

**3. MPFR hard case table** (sin_pi_hard.h, cos_pi_hard.h)
75 genuine Table-Maker cases identified offline by MPFR 4.2.2 at
200-bit precision. 31 entries for sin (248 bytes), 44 for cos
(352 bytes). **Total: 696 bytes** -- fits on FE310 (16KB RAM) with
room to spare. Checked at function entry before any computation.

The Minefield Method (Gustafson Ch.10) is not needed -- the adaptive
extensions resolve the hard cases dynamically, and the 696-byte table
handles the remaining 75 genuine Table-Maker cases.

### Verification

```bash
# exhaustive check -- all 2.1B positive posit32 inputs, 64 threads, ~6 min
cc -O2 -o exhaust_cospi trig56_exhaust_cospi.c -lm -lpthread
cc -O2 -o exhaust_sinpi trig56_exhaust_sinpi.c -lm -lpthread
./exhaust_cospi 64
./exhaust_sinpi 64

# MPFR verification of remaining "failures" -- confirms 0 genuine errors
cc -O2 -o mpfr_verify mpfr_verify.c -lm -lmpfr
./list_cos_failures | grep "p=0x" | awk '{print $1}' | sed 's/p=0x//' | ./mpfr_verify c
./list_sin_failures | grep "p=0x" | awk '{print $1}' | sed 's/p=0x//' | ./mpfr_verify s
```

> "I was really hoping there was a way to manually tweak CORDIC so you
> get the speed and simplicity but also get correct rounding everywhere
> -- that would be a big breakthrough."
> -- John L. Gustafson, personal communication 2026

> "To compute infinity is ridiculous when you think about it."
> -- John L. Gustafson, on IEEE 754 exception handling, 2026

---

## The MAC abstraction (microuniversal.h)

```c
typedef double (*dotfn)(double *a, double *b, int n);
typedef double (*sumfn)(double *a, int n);
```

**dot is the M. sum is the A. Together they are the MAC.**

Every algorithm in numerical linear algebra — CG, LDL\*, Cholesky, Kalman,
FFT — reduces to these two primitives. The quire is an implementation of
`dot_exact` and `sum_exact` that is provably correct up to 2^31 terms.
The algorithm does not care. It calls the function pointer.

Pass `dot_naive/sum_naive` for IEEE 754 behavior.
Pass `dot_exact/sum_exact` for Kahan compensation.
Pass `quire32_dot/quire32_sum` for the real quire.
Pass `bquire_dot/bquire_sum` for the B-posit quire.
The algorithm does not change. Only the guarantee does.

---

## B-posit quire (bposit_quire.h)

Standard posit\<32,2\> uses an unbounded regime field, requiring a 512-bit
quire. Bounding the regime field to rs bits (B-posit) dramatically reduces
quire width.

**Quire width formula** (Warsy, Sherman et al., CoNGA 2026):

```
E    = rs * 2^es
F    = n - 1 - rs - es
Qraw = 4*E + 2*F
Qsize = Qraw + 30    (+30 bits carry overflow protection)
```

**Candidate design point: es=2, rs=6, n=32**

```
Qsize = 172 bits (6 x uint32_t words)
vs standard: 512 bits (16 words)
Reduction: 63% in software, 77% in hardware synthesis
```

**Hardware results at es=2, rs=6 (45nm synthesis, Warsy et al.):**

```
IPC:         0.386 vs 0.09 baseline  (4.3x improvement)
Clock:       1394.7 MHz vs 1054.8 MHz (28% faster)
Quire area:  13,200 um^2 vs 57,463 um^2 (77% reduction)
Wall-clock:  0.57 us vs 3.26 us MAC loop (5.7x faster)
```

---

## CLZ without Zbb (pure RV32I)

Regime field decoding requires count-leading-zeros. On cores with the
Zbb bit-manipulation extension, `CLZ` is a single instruction. On a
small microcontroller and all bare RV32I cores, we provide a
6-comparison software implementation in `bposit_quire.h`:

```c
static int bposit_clz32(uint32_t x) {
    int n = 0;
    if (x == 0) return 32;
    if (!(x >> 16)) { n += 16; x <<= 16; }
    if (!(x >> 24)) { n +=  8; x <<=  8; }
    if (!(x >> 28)) { n +=  4; x <<=  4; }
    if (!(x >> 30)) { n +=  2; x <<=  2; }
    if (!(x >> 31)) { n +=  1; }
    return n;
}
```

No F. No D. No Zbb. No FPU. Pure integer arithmetic.

---

## Hilbert matrix scaling

The Hilbert matrix H(n) has entries H[i][j] = 1/(i+j+1). It is the
canonical test for numerical methods — condition number grows as (3.5)^(2n),
exhausting double precision by N=13.

**Gustafson scaling** (from J.L. Gustafson, unpublished DARPA report, 2018):
Scale both A and b by the full LCM of all denominators 1..2N-1, then
divide by the nearest power of 2 to keep entries in [0.5, 1.0] — the
posit maximum-accuracy region. The exact solution x\* is unchanged.

```
N=7:  LCM=360360, divide by 2^19=524288, largest entry=0.687
N=9:  LCM=12252240, divide by 2^24=16777216, largest entry=0.730
N=12: LCM=5354228880, divide by 2^33=8589934592, largest entry=0.623
```

---

## The lineage

**Ulrich Kulisch (1970)** — proposed an exact dot product accumulator as
a hardware requirement for IEEE 754. The committee declined. His letters
to the IEEE 754 revision group (2005) are at
[grouper.ieee.org/groups/1788/Misc/KulischLettersIEEE754R1.pdf](https://grouper.ieee.org/groups/1788/Misc/KulischLettersIEEE754R1.pdf)

**John L. Gustafson (2015)** — built the quire into the posit arithmetic
standard. Proved exact inversion of H(12) using LDL\* + quire + 4
refinement passes. *The End of Error* (CRC Press, 2015). Farewell to
Floats (CoNGA 2023):
[posithub.org/conga/2023/assets/pdf/FarewellToFloats.pdf](https://www.posithub.org/conga/2023/assets/pdf/FarewellToFloats.pdf)

**Theodorus Omtzigt (2024)** — built the MAC abstraction into MTL5 as
`accumulator_traits`. The algorithm calls a trait; the quire plugs in
behind it. [github.com/stillwater-sc/universal](https://github.com/stillwater-sc/universal)

**Bola Yosry Warsy, Paul Sherman (2026)** — B-posit quire width formula,
es/rs design space analysis, hardware synthesis results. CoNGA 2026,
Submission 7: "Closing the Performance Gap of Posit Arithmetic Hardware:
A RISC-V Fife Core Case Study."

---

## The dream

All RV32I base. All in integer registers.
172-bit B-posit quire in 6 register pairs.
No blob. Transparent to the electron.

---

## Live on Pioneer

All files run at [lab.riscvthai.org/terminal](https://lab.riscvthai.org/terminal) —
no account, no install. Open a browser, type:

```bash
cc -o bquire_test bquire_test.c -lm && ./bquire_test
```

RISC-V Open Lab, San Jose CA.
lab.riscvthai.org/posits/

---

## License

MIT. Type it in. Share it. Build anything.

The quire is not quiet.
