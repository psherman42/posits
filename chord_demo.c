/*
 * chord_demo.c -- Am7b5#13 in G Mixolydian: FFT with posit confidence scoring
 *
 * Synthesizes the chord + adjustable noise, runs fix_fft each frame,
 * displays spectrum with posit-inspired confidence scores, and shows
 * which bins survive into reconstruction (posit graceful falloff vs
 * naive hard threshold).
 *
 * This is the signal-path / inference-layer boundary made visible.
 *
 * Compile:
 *   gcc -O2 -o chord_demo chord_demo.c fixfft.c -lm -lncurses
 *
 * Run:
 *   ./chord_demo
 *
 * Keys:
 *   n / N   noise level down / up
 *   t / T   posit threshold down / up
 *   q       quit
 *
 * Am7b5#13 voicing (A3 root, G Mixolydian):
 *   A3   220 Hz  root
 *   C4   262 Hz  b3
 *   Eb4  311 Hz  b5
 *   G4   392 Hz  b7  (also the tonic of G Mixolydian)
 *   F#5  740 Hz  #13
 *
 * 2026-11-17  pds/claude
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ncurses.h>
#include <unistd.h>
#include "fixfft.h"

#define N        1024
#define LOG2_N   10
#define FS       16000      /* Hz -- matches FE310 AON clock */
#define HZ_BIN   (FS / N)   /* 15.625 Hz per bin */

/* chord tones -- bin = round(hz * N / FS) */
#define N_TONES  5
static const int   TONE_HZ  [N_TONES] = { 220, 262, 311, 392, 740 };
static const int   TONE_BIN [N_TONES] = {  14,  16,  19,  25,  47 };
static const char *TONE_NAME[N_TONES] = {
    "A3  root",
    "C4  b3  ",
    "Eb4 b5  ",
    "G4  b7  ",
    "F#5 #13 "
};

#define BIN_LO   10
#define BIN_HI   52     /* display this range: ~156..812 Hz */

static int16_t fr[N], fi[N];
static double  phase[N_TONES];
static int32_t ema[N/2];        /* integer EMA, proxy for posit quire */
static int     paused = 0;

/*
 * posit_conf() -- posit-inspired confidence: 256/(1+d^2)
 *
 * d = distance in bins from nearest chord tone.
 * Tapers smoothly toward zero as d grows.
 * Compare naive_pass() below: hard cliff at d > 1.
 *
 * This is the whole point of the demo:
 *   posit keeps "near misses" with graded confidence;
 *   naive either passes or slams to zero.
 */
static int posit_conf(int bin)
{
    int min_d = 9999;
    for (int i = 0; i < N_TONES; i++) {
        int d = abs(bin - TONE_BIN[i]);
        if (d < min_d) min_d = d;
    }
    int s = 256 / (1 + min_d * min_d);
    return s > 255 ? 255 : s;
}

static int naive_pass(int bin)
{
    for (int i = 0; i < N_TONES; i++)
        if (abs(bin - TONE_BIN[i]) <= 1) return 1;
    return 0;
}

static const char *tone_name(int bin)
{
    for (int i = 0; i < N_TONES; i++)
        if (abs(bin - TONE_BIN[i]) <= 1) return TONE_NAME[i];
    return NULL;
}

int main(void)
{
    int noise  = 1200;  /* amplitude, 0..8000 */
    int thresh = 20;    /* posit confidence threshold, 0..255 */
    int frame  = 0;

    srand(42);
    memset(phase, 0, sizeof(phase));

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    curs_set(0);

    if (has_colors()) {
        start_color();
        init_pair(1, COLOR_CYAN,    COLOR_BLACK);  /* spectrum bars      */
        init_pair(2, COLOR_YELLOW,  COLOR_BLACK);  /* posit conf bars    */
        init_pair(3, COLOR_GREEN,   COLOR_BLACK);  /* ✓ accepted         */
        init_pair(4, COLOR_RED,     COLOR_BLACK);  /* ✗ rejected / noise */
        init_pair(5, COLOR_WHITE,   COLOR_BLACK);  /* labels / borders   */
        init_pair(6, COLOR_MAGENTA, COLOR_BLACK);  /* chord tone rows    */
    }

    for (;;) {
        int ch = getch();

        if (ch == 'q' || ch == 'Q') break;

        /* The memset forces quire to reconverge from zero
         * and makes the smoothing property visually obvious. */
        if (ch == 'n' && noise >    0) { noise -= 200; memset(ema, 0, sizeof(ema)); }
        if (ch == 'N' && noise < 8000) { noise += 200; memset(ema, 0, sizeof(ema)); }

        if (ch == 't' && thresh >    4) thresh -=   4;
        if (ch == 'T' && thresh <  240) thresh +=   4;

        if (ch == ' ') paused = !paused;

        /* First spacebar: renders one frame (shows PAUSED in header), sets was_paused=1.
         * Every subsequent frame while paused: sleeps and skips.
         * Screen stays completely frozen — safe to copy.
         * Second spacebar: paused=0, was_paused=1, renders immediately,
         * then was_paused resets to 0 next frame. */
        static int was_paused = 0;
        if (paused && was_paused) { usleep(80000); continue; }
        was_paused = paused;

        static int32_t mag[N/2];
        static int32_t mmax = 1;
        if (!paused) {
            /* --- synthesize N samples --- */
            int16_t amp = 32767 / (N_TONES * 2);
            for (int i = 0; i < N; i++) {
                int32_t s = 0;
                for (int t = 0; t < N_TONES; t++)
                    s += (int32_t)(amp * sin(phase[t] + 2.0*M_PI * TONE_HZ[t] * i / FS));
                /* add white noise */
                s += (int32_t)(((rand() & 0xffff) - 32768) * noise / 65536);
                if (s >  32767) s =  32767;
                if (s < -32767) s = -32767;
                fr[i] = (int16_t)s;
                fi[i] = 0;
            }
            /* advance phase accumulators (keeps waveform coherent across frames) */
            for (int t = 0; t < N_TONES; t++)
                phase[t] = fmod(phase[t] + 2.0*M_PI * TONE_HZ[t] * N / FS, 2.0*M_PI);

            /* --- forward FFT --- */
            fix_fft(fr, fi, LOG2_N, 0);

            /* --- magnitudes (magnitude squared, avoid sqrt in inner loop) --- */
            mmax = 1;
            for (int i = 1; i < N/2; i++) {
                mag[i] = (int32_t)fr[i]*fr[i] + (int32_t)fi[i]*fi[i];
                if (mag[i] > mmax) mmax = mag[i];
            }

            /* --- EMA update: alpha=1/16, stays in 0..255 scale --- */
            for (int i = 1; i < N/2; i++) {
                int s = (int)(256.0 * sqrt((double)mag[i] / mmax));
                int c = (posit_conf(i) * s) / 256;
                /* Steady state: x = (15x + c)/16 -> x = c. Stays in 0..255 */
                //ema[i] = (ema[i] * 15 + c) / 16;  /* time constant of ~21 seconds at 12fps */
                ema[i] = (ema[i] * 3 + c) / 4;  /* faster converge */
            }
        }

        /* --- display --- */
        clear();

        /* header */
        attron(COLOR_PAIR(5) | A_BOLD);
        mvprintw(0, 0, "Am7b5#13  G Mixolydian");
        attroff(A_BOLD);
        printw("   noise:");
        attron(COLOR_PAIR(4));
        for (int i = 0; i < noise/400; i++) addch(ACS_BLOCK);
        for (int i = noise/400; i < 20; i++) addch('.');
        attroff(COLOR_PAIR(4));
        attron(COLOR_PAIR(5));
        printw("  thresh:%3d  frame:%5d", thresh, frame);
        if (paused) {
            attron(COLOR_PAIR(4)|A_BOLD);
            printw("  PAUSED");
            attroff(COLOR_PAIR(4)|A_BOLD);
            attron(COLOR_PAIR(5));
        }
        printw("   n/N t/T q");
        attroff(COLOR_PAIR(5));

        /* column headers */
        attron(COLOR_PAIR(5));
        mvprintw(1, 0, "  bin   Hz  note        spectrum (fixfft)      posit conf      quire/16     naive  keep?");
        mvprintw(2, 0, "  ---  ----  ------    [--------------------] [----------]    [--------]     ----  -----");
        attroff(COLOR_PAIR(5));

        int row = 3;
        for (int b = BIN_LO; b < BIN_HI && row < 21; b++) {  /* row<23 needs selective display for F#5 bin 47 */
            int hz     = b * FS / N;
            int sbar   = (int)(20.0 * sqrt((double)mag[b] / mmax));
            if (sbar > 20) sbar = 20;
            int sig    = (int)(256.0 * sqrt((double)mag[b] / mmax));  /* signal strength 0..255 */
            int pc     = (posit_conf(b) * sig) / 256;   /* prior × evidence → combined confidence */
            int nc     = naive_pass(b);                 /* hard threshold: d<=1 or nothing */
            int pbar   = pc * 10 / 256;                 /* scale to bar width */
            const char *cname = tone_name(b);
            int chord  = (cname != NULL);

            /* bin / hz */
            attron(COLOR_PAIR(5));
            mvprintw(row, 0, "  %3d  %4d  ", b, hz);
            attroff(COLOR_PAIR(5));

            /* note name */
            if (chord) {
                attron(COLOR_PAIR(6) | A_BOLD);
                printw("%-8s", cname);
                attroff(COLOR_PAIR(6) | A_BOLD);
            } else {
                attron(COLOR_PAIR(5));
                printw("        ");
                attroff(COLOR_PAIR(5));
            }

            /* spectrum bar */
            attron(chord ? COLOR_PAIR(1) | A_BOLD : COLOR_PAIR(5));
            printw("  [");
            for (int i = 0; i < 20; i++) addch(i < sbar ? ACS_BLOCK : ' ');
            printw("]");
            attroff(COLOR_PAIR(1) | A_BOLD | COLOR_PAIR(5));

            /* posit confidence bar */
            attron(pc >= thresh ? COLOR_PAIR(2) : COLOR_PAIR(4));
            printw(" [");
            for (int i = 0; i < 10; i++) addch(i < pbar ? ACS_BLOCK : ' ');
            printw("]%3d", pc);
            attroff(COLOR_PAIR(2) | COLOR_PAIR(4));

            /* quire/EMA column */
            int qc   = ema[b];
            int qbar = qc * 8 / 256;
            attron(COLOR_PAIR(qc >= thresh ? 3 : 5));
            printw(" [");
            for (int i = 0; i < 8; i++) addch(i < qbar ? ACS_BLOCK : ' ');
            printw("]%3d", qc);
            attroff(COLOR_PAIR(3) | COLOR_PAIR(5));

            /* naive pass/fail */
            attron(COLOR_PAIR(5));
            printw("  %-4s", nc ? "pass" : "----");
            attroff(COLOR_PAIR(5));

            /* posit keep/drop */
            if (pc >= thresh) {
                attron(COLOR_PAIR(3) | A_BOLD);
                printw("  keep");
                attroff(COLOR_PAIR(3) | A_BOLD);
            } else {
                attron(COLOR_PAIR(4));
                printw("  drop");
                attroff(COLOR_PAIR(4));
            }

            row++;
        }

        /* footer legend */
        attron(COLOR_PAIR(5));
        mvprintw(21, 0, "--------------------------------------------------------------------------------");
        mvprintw(22, 0, "posit: conf=256/(1+d\xc2\xb2) graceful falloff as d=bins from chord tone grows");
        mvprintw(23, 0, "naive: pass if d<=1, drop otherwise -- hard cliff, no gradient");
        attroff(COLOR_PAIR(5));

        refresh();
        frame++;
        usleep(80000);  /* ~12 fps */
    }

    endwin();
    return 0;
}
