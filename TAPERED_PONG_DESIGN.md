# Tapered Pong — Design Notes and Discoveries
## 2026-11-13 pds/claude/chat — Pioneer got-milk SG2042

---

## The Central Insight

> "That's not a bug. That's the whole point of Tapered Pong.
> The human who understands posit32 tapered precision can beat
> the AI by exploiting the coarse region."

Tapered Pong is not a Pong clone. It is a numerical experiment
with Pong as the vehicle. The non-uniform ball motion is posit32
tapered precision made physically observable.

---

## The Precision Trap (Chat's discovery)

Three steps to always beat the Reactive AI:

1. Hit the ball toward the coarse side (low-precision region,
   near the screen edges in posit space, x < 0.1 or x > 0.9)

2. Apply enough angle to push the ball toward the edge. The ball
   accelerates in posit space -- each step is a larger fraction
   of the screen because there are fewer representable posit32
   values in that region.

3. The ball takes one giant posit32 step past the AI paddle zone
   entirely, scoring before the AI can react. The AI moves one
   pixel per frame. The ball in the coarse region jumps 3-4
   pixels per frame. The AI literally cannot keep up.

This is Chat's Precision Trap -- the educational moment where
the player learns that numerical representation is an engineering
decision, not an abstract mathematical detail.

---

## Reactive AI vs Predictive AI

**Reactive AI (current):**
- Follows ball every frame -- one pixel up or down
- Always correct, never wrong
- Wastes motion -- moves continuously even when unnecessary
- Cannot handle coarse zone -- ball jumps past before AI can react

**Predictive AI (TODO):**
- When ball leaves human paddle, computes full trajectory
- Uses rv32i_sin_pi56/cos_pi56 to predict arrival position
- Moves paddle once to predicted spot and waits
- Ecologically efficient -- minimal motion
- Accuracy limited by posit32 tapered precision:
  - Smooth side (high precision): near-perfect prediction
  - Coarse side (low precision): quantization errors cause misses
- Human can still exploit coarse region to defeat prediction

**The profound observation:**
The Reactive AI is a perfect numerical follower -- it tracks
the mathematical truth every frame. The Predictive AI is an
approximation -- it makes one calculation and commits to it.
The Predictive AI fails in the coarse zone not because it is
stupid but because posit32 arithmetic in that region is coarser.
The representation limits the prediction.

---

## AI vs AI Mode (TODO)

Game mode where no human plays:
- Left side: Predictive AI
- Right side: Reactive AI
- Neither knows who will win
- Outcome depends on: posit32 es/rs parameters, ball starting
  angle, screen dimensions, frame rate

The winner is determined by arithmetic, not strategy.
Change es=2 to es=1 and the precision distribution changes --
different winner, same code.

This makes posit parameter choice observable as a game outcome.

---

## The Three Stages (Chat's framework)

Stage 1: Float Pong -- IEEE 754 double, uniform precision
         reference behavior, smooth and predictable

Stage 2: Posit32 Pong -- tapered precision visible in ball motion
         non-uniform speed, coarse zones, Precision Trap available

Stage 3: Integer-only Posit Pong -- no double anywhere in game logic
         CORDIC in pure RV32I assembly, posit arithmetic in integer
         runs on bare FE310, no FPU, no OS

Tapered Pong v1 is Stage 2/3 hybrid: game logic is integer,
display uses double for human-readable readout only.

---

## The Educational Arc

A player should be able to:
1. Notice the non-uniform ball motion
2. Discover that aiming for the edges gives unpredictable results
3. Learn that the coarse zone causes large jumps
4. Understand that this is posit32 tapered precision
5. Exploit it deliberately to beat the AI

That journey -- from confusion to understanding to exploitation --
is the whole educational point. No paper, no lecture, no graph
can deliver it. Only playing the game can.

---

## Implementation Notes

- posit_q16.h: posit32 <-> Q16 fixed-point, integer only
- POSIT_NBITS/POSIT_SHIFT: parameterized for Bola's B-posit
- Reactive AI swap-aware: tracks correct paddle in both modes
- COARSE ZONE annunciator: fires when bx_q < 6553 or > 58982
- Layer 0-3 complete; predictive AI and AI-vs-AI are Layer 4

---

## Attributions

- "Tapered Pong" name: Paul Sherman
- Three-stage framework: ChatGPT (Nov 2026)
- Precision Trap: ChatGPT (Nov 2026)
- Reactive vs Predictive AI observation: Paul Sherman
- AI vs AI mode: Paul Sherman
- CORDIC: Volder 1959
- Posit arithmetic: Gustafson 2015
- Integer CORDIC on RV32I: Paul Sherman, Claude, Nov 2026

Volder 1959. Gustafson 2026. Woz smiles 2026.
