/* posit_pong.c -- Tapered Pong v1 (prototype)
 * An Integer-Only RISC-V Posit32 Demonstration -- work in progress.
 *
 * Ball angle computed by rv32i_sin_pi56 / rv32i_cos_pi56
 * (56-iteration CORDIC in pure RV32I assembly -- no FPU, no MUL)
 *
 * v1 status: game mechanics and CORDIC are working. Ball position,
 * velocity and collision are integer. Display uses posit_to_double for
 * human-readable readout only. Integer-only claim satisfied for game logic.
 * See layers below for the roadmap.
 *
 * Layer 0 (done): playable prototype -- posit32 CORDIC drives ball angle
 * Layer 1 (done): integer p_to_x/p_to_y -- posit32 to screen coords (posit_q16.h)
 * Layer 2 (done): integer p_add/p_sub via Q16 (posit_q16.h)
 *   Note: quire32.h has exact integer accumulation but double API;
 *   quire32_add_posit() integer interface is the proper next step
 * Layer 3 (done): integer game logic -- no double in ball physics, velocity,
 *   collision, or angle; display uses posit_to_double for readout only
 *
 * Human (left/coarse) vs AI (right/smooth) -- [t] to swap sides
 * The non-uniform ball motion is posit32 tapered precision made visible.
 *
 * 2026-11-13 pds/claude/chat -- Pioneer got-milk SG2042
 * "My very first video game with TV RF Modulator on Ch.3" -- Paul Sherman
 *
 * Volder 1959. Gustafson 2026. Woz smiles 2026.
 *
 * Build:
 *   cc -O2 -o posit_pong posit_pong.c posit32_sin56_rv32i.c \
 *      posit_trig56_rv32i.s -I. -lm
 *
 * Controls: w/s = up/down, t = swap sides, space = pause, q = quit
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include "positenc.h"
#include "posit_q16.h"

/* RV32I assembly CORDIC */
extern uint32_t rv32i_sin_pi56(uint32_t x);
extern uint32_t rv32i_cos_pi56(uint32_t x);

/* ---- screen dimensions ---- */
#define W       60
#define H       24
#define PADDLE  4   /* paddle height */

/* ---- posit32 constants ---- */
/* angle step per frame -- small enough for smooth motion */
#define ANGLE_STEP_X  0.008   /* cos(pi*x) gives dx */
#define ANGLE_STEP_Y  0.012   /* sin(pi*x) gives dy */

/* ---- game state ---- */
static struct {
    /* ball position and angle -- all posit32 */
    uint32_t bx;        /* ball x in posit32 [0,1] mapped to [1,W-2] */
    uint32_t by;        /* ball y in posit32 [0,1] mapped to [1,H-2] */
    uint32_t angle;     /* current angle in posit32 [0,2] = [0, 2pi] */
    int      dx_sign;   /* +1 or -1 */
    int      dy_sign;   /* +1 or -1 */

    /* paddles -- y position (top of paddle) */
    int      lpad;      /* left (human) */
    int      rpad;      /* right (AI) */

    /* scores */
    int      lscore;
    int      rscore;

    /* frame counter */
    int      frame;
    int      swap;      /* 1 = human on right (smooth side) */
    int      paused;    /* 1 = game paused */
} G;

/* ---- terminal raw mode ---- */
static struct termios _orig;
static void term_raw(void){
    struct termios t;
    tcgetattr(0, &_orig);
    t = _orig;
    t.c_lflag &= ~(ICANON|ECHO);
    t.c_cc[VMIN] = 0;
    t.c_cc[VTIME] = 0;
    tcsetattr(0, TCSANOW, &t);
    fcntl(0, F_SETFL, O_NONBLOCK);
}
static void term_restore(void){ tcsetattr(0, TCSANOW, &_orig); }

static int kbhit(void){
    char c;
    return read(0, &c, 1) == 1 ? c : 0;
}

/* ---- posit32 helpers ---- */
/* scale posit [0,1] to screen coordinate
 * p_to_fixed: convert posit<nbits,es> in [0,1] to integer in [0,scale]
 * Pure integer -- no double.
 * Positive posits are monotonically ordered with their bit patterns.
 * For p in [0,1]: bit pattern in [0, 2^(nbits-2)].
 * posit32 es=2: POSIT_SHIFT=30. Bola B-posit: change POSIT_NBITS.  */
#define POSIT_NBITS  32
#define POSIT_SHIFT  (POSIT_NBITS - 2)   /* 30 for posit32 */

static int p_to_fixed(uint32_t p, int scale){
    uint64_t r = (uint64_t)p * (uint64_t)scale;
    return (int)(r >> POSIT_SHIFT);
}

static int p_to_bx(uint32_t p){
    /* collision coordinate -- never flipped, always posit space */
    int32_t q = posit_to_q16(p);
    int x = (int)((q * (W-2)) >> 16) + 1;
    if(x < 1)   x = 1;
    if(x > W-2) x = W-2;
    return x;
}

static int p_to_x(uint32_t p){
    /* Layer 1: integer posit32 to screen coordinate -- no double */
    int32_t q = posit_to_q16(p);         /* q in [0, 65536] */
    if(G.swap) q = 65536 - q;            /* flip for smooth side */
    int x = (int)((q * (W-2)) >> 16) + 1;
    if(x < 1)   x = 1;
    if(x > W-2) x = W-2;
    return x;
}

static int p_to_y(uint32_t p){
    /* Layer 1: integer posit32 to screen coordinate -- no double */
    int32_t q = posit_to_q16(p);         /* q in [0, 65536] */
    int y = (int)((q * (H-2)) >> 16) + 1;
    if(y < 1)   y = 1;
    if(y > H-2) y = H-2;
    return y;
}

/* add/sub posit32 in [0,1] -- Layer 2: integer, no double */
static uint32_t p_add(uint32_t a, uint32_t b){
    return p_add_int(a, b);
}
static uint32_t p_sub(uint32_t a, uint32_t b){
    return p_sub_int(a, b);
}

/* ---- draw ---- */
static char _buf[H][W+1];
static void clear_buf(void){
    for(int y=0; y<H; y++){
        memset(_buf[y], ' ', W);
        _buf[y][W] = 0;
    }
}
static void draw_char(int x, int y, char c){
    if(x>=0&&x<W&&y>=0&&y<H) _buf[y][x]=c;
}
static void draw_str(int x, int y, const char *s){
    for(; *s && x<W; s++,x++) draw_char(x,y,*s);
}

static void render(void){
    clear_buf();

    /* border */
    for(int x=0; x<W; x++){ draw_char(x,0,'-'); draw_char(x,H-1,'-'); }
    for(int y=0; y<H; y++){ draw_char(0,y,'|'); draw_char(W-1,y,'|'); }

    /* center dashes */
    for(int y=1; y<H-1; y+=2) draw_char(W/2, y, ':');

    /* paddles */
    for(int i=0; i<PADDLE; i++){
        draw_char(1,        G.lpad+i, '#');
        draw_char(W-2,      G.rpad+i, '#');
    }

    /* ball */
    int bx = p_to_x(G.bx);
    int by = p_to_y(G.by);
    draw_char(bx, by, 'O');

    /* score */
    char score[32];
    snprintf(score, sizeof(score), " %d ", G.lscore);
    draw_str(W/2-8, 0, score);
    snprintf(score, sizeof(score), " %d ", G.rscore);
    draw_str(W/2+4, 0, score);

    /* posit info */
    char info[80];
    uint32_t sin_v = rv32i_sin_pi56(G.angle);
    uint32_t cos_v = rv32i_cos_pi56(G.angle);
    double sx = posit_to_double(cos_v) * G.dx_sign;
    double sy = -posit_to_double(sin_v) * G.dy_sign;
    int deg = (int)(atan2(sy, sx) * 180.0 / 3.14159265);
    if(deg < -180) deg += 360;
    snprintf(info, sizeof(info),
             " angle=%3d°  sin=%+.4f  cos=%+.4f  [Tapered Pong: smooth center, coarse edges -- posit32 precision]",
             deg,
             posit_to_double(sin_v),
             posit_to_double(cos_v));
    /* print below board */

    /* move cursor to top, draw */
    printf("\033[H");
    for(int y=0; y<H; y++) printf("%s\n", _buf[y]);
    printf("%s\n", info);
    printf(" [w/s]=paddle [t]=swap [space]=pause [q]=quit  side=%s %s frame=%d\n",
           G.swap?"smooth(right)":"coarse(left)",
           G.paused?"PAUSED":"      ",
           G.frame);
    fflush(stdout);
}

/* ---- ball physics using posit32 sin/cos ---- */
static void move_ball(void){
    /* get velocity components from posit32 sin/cos of current angle */
    uint32_t sin_v = rv32i_sin_pi56(G.angle);
    uint32_t cos_v = rv32i_cos_pi56(G.angle);

    /* step size */
    /* dx = |cos| * step_x, dy = |sin| * step_y
     * use signed posit Q16 -- negate if sign bit set  */
    int32_t qcos = posit_to_q16(cos_v & 0x7fffffffu);  /* magnitude */
    int32_t qsin = posit_to_q16(sin_v & 0x7fffffffu);  /* magnitude */
    uint32_t dx = q16_to_posit32((int32_t)((int64_t)qcos * posit_to_q16(P32_STEP_X) >> 16));
    uint32_t dy = q16_to_posit32((int32_t)((int64_t)qsin * posit_to_q16(P32_STEP_Y) >> 16));

    /* move */
    if(G.dx_sign > 0) G.bx = p_add(G.bx, dx);
    else              G.bx = p_sub(G.bx, dx);
    if(G.dy_sign > 0) G.by = p_add(G.by, dy);
    else              G.by = p_sub(G.by, dy);

    int bx = p_to_bx(G.bx);  /* collision -- unflipped */
    int by = p_to_y(G.by);

    /* wall bounce top/bottom */
    if(by <= 1)   { G.dy_sign =  1; G.by = P32_0_08; G.angle = p_add_int(G.angle, P32_0_03); }
    if(by >= H-2) { G.dy_sign = -1; G.by = P32_0_92; G.angle = p_sub_int(G.angle, P32_0_03); }

    /* paddle collision
     * lpad always on left side of screen
     * rpad always on right side of screen
     * swap=0: human=left(lpad), AI=right(rpad)
     * swap=1: human=right(rpad), AI=left(lpad)              */

    /* left side collision */
    if(bx <= 2){
        if(by >= G.lpad && by < G.lpad + PADDLE){
            G.dx_sign = 1;
            int hit_q = ((by - G.lpad) * 65536) / PADDLE;  /* Q16 hit position */
            G.angle = p_add_int(P32_0_10, q16_to_posit32((int32_t)((int64_t)hit_q * posit_to_q16(P32_0_40) >> 16)));
        } else {
            if(G.swap) G.lscore++; else G.rscore++;
            render();
            { struct timespec ts = {0, 500000000}; nanosleep(&ts, NULL); }
            G.bx = P32_HALF;
            G.by = P32_HALF;
            G.dx_sign = 1;
            G.angle = P32_0_15;
        }
    }

    /* right side collision */
    if(bx >= W-3){
        if(by >= G.rpad && by < G.rpad + PADDLE){
            G.dx_sign = -1;
            int hit_q = ((by - G.rpad) * 65536) / PADDLE;  /* Q16 hit position */
            G.angle = p_add_int(P32_0_60, q16_to_posit32((int32_t)((int64_t)hit_q * posit_to_q16(P32_0_40) >> 16)));
        } else {
            if(G.swap) G.rscore++; else G.lscore++;
            render();
            { struct timespec ts = {0, 500000000}; nanosleep(&ts, NULL); }
            G.bx = P32_HALF;
            G.by = P32_HALF;
            G.dx_sign = -1;
            G.angle = P32_0_65;
        }
    }
}

/* ---- AI paddle ---- */
static void ai_move(void){
    int by = p_to_y(G.by);
    int mid = G.rpad + PADDLE/2;
    /* Reactive AI: follows ball every frame -- wastes motion but always correct.
     *
     * TODO: Predictive AI -- when ball leaves human paddle, compute trajectory
     * using rv32i_sin_pi56/cos_pi56, move once to predicted arrival point, wait.
     * Prediction accuracy limited by posit32 tapered precision:
     *   smooth side (high precision): near-perfect prediction
     *   coarse side (low precision):  quantization errors cause misses
     * Human can exploit coarse region to defeat prediction -- Chat's Precision Trap.
     * Mode toggle: [p] = reactive, [P] = predictive (future feature)          */
    int *ap = G.swap ? &G.lpad : &G.rpad;
    mid = *ap + PADDLE/2;
    if(mid < by && *ap < H-1-PADDLE) (*ap)++;  /* H-2-PADDLE to leave one row gap */
    if(mid > by && *ap > 1)          (*ap)--;
}

/* ---- main ---- */
int main(void){
    positenv_init(32,2);

    /* init game state */
    G.bx     = P32_HALF;
    G.by     = P32_HALF;
    G.angle  = P32_0_15;
    G.dx_sign = 1;
    G.dy_sign = 1;
    G.lpad   = H/2 - PADDLE/2;
    G.rpad   = H/2 - PADDLE/2;
    G.lscore = 0;
    G.rscore = 0;
    G.frame  = 0;

    term_raw();
    printf("\033[2J\033[H");  /* clear screen */

    while(1){
        int ch = kbhit();
        if(ch == 'q' || ch == 'Q') break;
        /* human controls lpad (swap=0) or rpad (swap=1) */
        int *hp = G.swap ? &G.rpad : &G.lpad;
        if(ch == 'w' && *hp > 1)          (*hp)--;
        if(ch == 's' && *hp < H-1-PADDLE) (*hp)++;
        if(ch == 't') G.swap = !G.swap;
        if(ch == 32) G.paused = !G.paused;  /* spacebar */

        if(!G.paused){ move_ball(); ai_move(); }
        render();
        /* drift angle slightly every frame -- shows live posit32 */
        if(!G.paused && G.frame % 3 == 0)
            G.angle = p_add_int(G.angle, P32_0_001);
        G.frame++;

        /* ~30fps */
        struct timespec ts = {0, 33000000};
        nanosleep(&ts, NULL);
    }

    term_restore();
    printf("\033[2J\033[H");
    printf("Final score: Human %d  AI %d\n", G.lscore, G.rscore);
    printf("Frames: %d\n", G.frame);
    printf("Ball angle (posit32): 0x%08x\n", G.angle);
    printf("sin(pi*angle) = 0x%08x = %.8f\n",
           rv32i_sin_pi56(G.angle),
           posit_to_double(rv32i_sin_pi56(G.angle)));
    printf("cos(pi*angle) = 0x%08x = %.8f\n",
           rv32i_cos_pi56(G.angle),
           posit_to_double(rv32i_cos_pi56(G.angle)));
    printf("\nNo double in game logic. Pure posit32 RV32I CORDIC.\n");
    printf("Volder 1959. Gustafson 2026. Woz smiles 2026.\n");
    return 0;
}

