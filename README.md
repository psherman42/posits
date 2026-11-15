# posits — js branch

**Tapered Pong: The AI Who Loved Me**
*An Integer-Only RISC-V Posit32 Demonstration*

> Nobody said the future would be correctly rounded.

Play: [lab.riscvthai.org/posits/tapered_pong_play.html](https://lab.riscvthai.org/posits/tapered_pong_play.html)

---

## What this is

Ball trajectory driven by posit32 sin(πx)/cos(πx) on a 56-iteration CORDIC.
No Math.sin(). No IEEE 754 trig. Pure JavaScript BigInt integer arithmetic —
the same CORDIC as the RV32I assembly port, running in any browser.

The non-uniform ball motion is not a bug. It is posit32 tapered precision
made physically observable. Near the edges the ball takes giant steps. Near
the center it moves smoothly. This is the arithmetic.

---

## Game modes

| Key | Mode | Description |
|-----|------|-------------|
| default | Human vs AI | You play. AI is Reactive by default. |
| `M` | Cycle AI mode | Reactive → Predictive → Stealth |
| `A` | AI vs AI | Reactive (gold) vs Predictive (white) |
| `T` | Swap sides | Human moves to smooth/coarse side |
| `Space` | Pause | |
| `R` | Restart | |

### AI strategies

**Reactive AI** — follows ball every frame. Always correct, never wrong.
Cannot handle the coarse zone — ball jumps past before AI can react.

**Predictive AI** — calculates trajectory once per serve, moves paddle once,
then waits. 33× fewer moves than Reactive. Accurate in smooth zone.
Fails in coarse zone when quantization errors corrupt the prediction.

**Stealth AI** — switches silently between Reactive and Predictive.
You never know which you're facing. A bad prediction can be confidently wrong.

### The Precision Trap

Three steps to always beat the Reactive AI:
1. Hit the ball toward the coarse zone (screen edges, posit x < 0.1 or x > 0.9)
2. The ball accelerates — fewer representable posit32 values, larger steps
3. The ball jumps past the AI paddle entirely

To beat the Predictive AI: same approach. The coarse zone corrupts its
trajectory calculation. Aim there.

### AI vs AI observations

In AI-vs-AI mode, Predictive wins in the smooth zone — nearly perfect
trajectory calculation, 33× fewer moves. But a human who forces the ball
into the coarse zone defeats Predictive easily. The real contest is
Human vs Predictive AI, not AI vs AI.

---

## Thresholds (free correct answers)

cos(πx) = 1.0 exactly for p ≤ 0x0445f306 — 71.7M inputs (7.6%)
Gustafson threshold, personal communication Nov 2026

sin(πx) = 1.0 exactly for p ≥ 0x38200001 — 132.1M inputs (6.2%)
Sherman threshold, derived from exhaustive scan Nov 2026

Combined: 203.8M free correct answers (13.8% of positive posit32 [0,1/2])


---

## Files

| File | Description |
|------|-------------|
| `posit_cordic.js` | posit32 CORDIC in BigInt — foundation library |
| `tapered_pong_play.html` | Browser game — self-contained, no server needed |

---

## Attributions

- "Tapered Pong" name: Paul Sherman
- "The AI Who Loved Me" title: Paul Sherman + ChatGPT
- "Nobody said the future would be correctly rounded.": ChatGPT
- Three-stage framework: ChatGPT (Nov 2026)
- Precision Trap: ChatGPT (Nov 2026)
- Reactive vs Predictive AI: Paul Sherman
- Stealth AI mode: Paul Sherman
- AI vs AI mode: Paul Sherman
- Gustafson cos threshold: John L. Gustafson (personal comm, Nov 2026)
- Sherman sin threshold: Paul Sherman (exhaustive scan, Nov 2026)
- CORDIC: Volder 1959
- Posit arithmetic: Gustafson 2015
- Integer CORDIC on RV32I: Paul Sherman, Claude, Nov 2026
- Browser port: Paul Sherman, Claude, Nov 2026

*Volder 1959. Gustafson 2026. Woz smiles 2026.*

---

## Related branches

- `master` — C89 reference implementation
- `rv32i-asm` — RV32I assembly CORDIC, Tapered Pong terminal version
- `js` — this branch, browser version
