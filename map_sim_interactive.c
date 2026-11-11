/*
 * map_sim_interactive.c -- interactive terminal map demo for map_engine.h
 *
 * Renders the 128x64 OLED map as ASCII art in the terminal.
 * Keyboard controls mirror the FE310 button interface.
 *
 * Usage:
 *   n/p      : walk forward/backward along simulated GPS track
 *   m        : cycle modes
 *                TRACK -- view follows GPS dot, zoom persists
 *                PAN   -- free pan with hjkl or wasd, dot stays visible
 *                ROUTE -- auto-scales to show all waypoints + track
 *   +/-      : zoom in/out (persists across mode changes)
 *   wasd     : pan N/S/E/W (gamer style)
 *   hjkl     : pan W/S/N/E (vim style)
 *   W        : plant waypoint at current GPS position
 *   G        : add Golden Gate Bridge waypoint
 *   C        : add Chicago waypoint
 *   g        : goto first waypoint
 *   r        : reset to neighborhood
 *   q        : quit
 *
 * GPS points: San Jose neighborhood walk, 2026-11-08 pds
 *   A: 37.40058, -121.92571  (start)
 *   B: 37.39873, -121.92434  (238.9m SSE)
 *   C: 37.40054, -121.92456  (201.8m N from B)
 *
 * 2026-11-11 pds+claude  initial cut
 * lab.riscvthai.org/posits/
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <termios.h>
#include <unistd.h>
#include "map_engine.h"

/* -- ASCII framebuffer ----------------------------------------- */

#define FB_W  MAP_W
#define FB_H  MAP_H

static uint8_t _fb[FB_H][FB_W];

static void fb_clear(void) {
    memset(_fb, 0, sizeof(_fb));
}

static void fb_plot(uint8_t x, uint8_t y) {
    if (x < FB_W && y < FB_H) _fb[y][x] = 1;
}

static void fb_print(void) {
    int x, y;
    char mode_str[16];
    switch(_mode) {
        case MODE_TRACK: strcpy(mode_str, "TRACK"); break;
        case MODE_PAN:   strcpy(mode_str, "PAN  "); break;
        case MODE_ROUTE: strcpy(mode_str, "ROUTE"); break;
        default:         strcpy(mode_str, "?????"); break;
    }

    /* clear screen and home cursor */
    printf("\033[2J\033[H");

    printf("+-- mode=%-5s zoom=%dm/px center=%d,%d wp=%d --+\n",
           mode_str, map_zoom_meters(),
           _view_lat/10000000, _view_lon/10000000,
           _wp_count);

    /* render two rows per terminal line for aspect ratio */
    for (y = 0; y < FB_H; y += 2) {
        printf("|");
        for (x = 0; x < FB_W; x++) {
            int top = _fb[y][x];
            int bot = (y+1 < FB_H) ? _fb[y+1][x] : 0;
            if      (top && bot) printf("*");
            else if (top)        printf("'");
            else if (bot)        printf(".");
            else                 printf(" ");
        }
        printf("|\n");
    }

    printf("+");
    for (x = 0; x < FB_W; x++) printf("-");
    printf("+\n");
    printf("  +/-:zoom  wasd/hjkl:pan  m:mode  W:wp  G:GoldenGate  C:Chicago  n/p:walk  r:reset  q:quit\n");
    printf("  gps: %d,%d  zoom_ubx=%d  manual=%d\n",
           _gps_lat, _gps_lon, _zoom_ubx, _manual_zoom);
}

/* -- raw terminal mode ----------------------------------------- */

static struct termios _orig_termios;

static void raw_mode_on(void) {
    struct termios raw;
    tcgetattr(STDIN_FILENO, &_orig_termios);
    raw = _orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

static void raw_mode_off(void) {
    tcsetattr(STDIN_FILENO, TCSANOW, &_orig_termios);
}

/* -- GPS test data -- San Jose neighborhood walk --------------- */

static int _gps_idx = 0;  /* current simulated GPS position index into track */

#define A_LAT  374005821
#define A_LON -1219257115
#define B_LAT  373987283
#define B_LON -1219243436
#define C_LAT  374005352
#define C_LON -1219245582

/* simulate GPS track along A->B->C with intermediate points */
static int32_t _track_lats[] = {
    A_LAT,
    A_LAT - 3000,  A_LON + 2000,
    A_LAT - 6000,
    A_LAT - 9000,
    A_LAT - 12000,
    B_LAT + 4000,
    B_LAT + 2000,
    B_LAT,
    B_LAT + 3000,
    B_LAT + 6000,
    B_LAT + 9000,
    B_LAT + 12000,
    C_LAT
};
static int32_t _track_lons[] = {
    A_LON,
    A_LON + 2000,
    A_LON + 4000,
    A_LON + 6000,
    A_LON + 8000,
    A_LON + 10000,
    B_LON + 2000,
    B_LON + 1000,
    B_LON,
    B_LON + 2000,
    B_LON + 4000,
    B_LON + 6000,
    B_LON + 8000,
    C_LON
};
#define N_TRACK 14

static void load_track(void) {
    int i;
    _track_count = 0; _track_head = 0;
    for (i = 0; i < N_TRACK; i++)
        map_update_gps(_track_lats[i], _track_lons[i]);
    /* current position at C */
    _gps_lat = C_LAT;
    _gps_lon = C_LON;
}

/* -- main ------------------------------------------------------ */

int main(void) {
    int ch;

    /* load neighborhood track */
    load_track();

    /* set initial GPS dot */
    _gps_idx = 0;

    /* set initial view */
    _zoom_ubx = 500;
    _view_lat = A_LAT;
    _view_lon = A_LON + (C_LON - A_LON)/2;

    raw_mode_on();
    atexit(raw_mode_off);

    /* initial draw */
    map_draw(fb_plot, fb_clear);
    fb_print();

    while (1) {
        ch = getchar();
        switch (ch) {
            case 'q': case 'Q':
                printf("\033[2J\033[H");
                printf("Goodbye.\n");
                return 0;

            case '+': case '=':
                map_btn_b_short();
                break;

            case '-':
                map_btn_b_long();
                break;

            /* gamer keyboard style */
            case 'w': _pan_dir = PAN_N; map_btn_c_short(); break;
            case 's': _pan_dir = PAN_S; map_btn_c_short(); break;
            case 'd': _pan_dir = PAN_E; map_btn_c_short(); break;
            case 'a': _pan_dir = PAN_W; map_btn_c_short(); break;

            /* programmer vi(m) keyboard style */
            case 'h': _pan_dir = PAN_E; map_btn_c_short(); break;
            case 'j': _pan_dir = PAN_N; map_btn_c_short(); break;
            case 'k': _pan_dir = PAN_S; map_btn_c_short(); break;
            case 'l': _pan_dir = PAN_W; map_btn_c_short(); break;

            case 'm':
                map_btn_a_short();
                break;

            case 'W':
                map_btn_a_long();
                break;

            case 'g':
                if (_wp_count > 0)
                    map_goto(_wp[0].lat, _wp[0].lon, 50000);
                break;

            case 'r':
                /* reset to neighborhood */
                _manual_zoom = 0;
                _mode = MODE_TRACK;
                _view_lat = A_LAT;
                _view_lon = A_LON + (C_LON - A_LON)/2;
                _zoom_ubx = 500;
                break;

            case 'G':
                /* add Golden Gate as waypoint */
                _wp[_wp_count].lat = 377108600;
                _wp[_wp_count].lon = -1225001200;
                _wp_count++;
                break;

            case 'C':
                /* add Chicago as waypoint */
                _wp[_wp_count].lat = 418808700;
                _wp[_wp_count].lon = -876278100;
                _wp_count++;
                break;

            case 'n':  /* next GPS position -- simulate walking forward */
                if (_gps_idx < N_TRACK - 1) _gps_idx++;
                map_update_gps(_track_lats[_gps_idx], _track_lons[_gps_idx]);
                break;
            case 'p':  /* prev GPS position -- simulate walking backward */
                if (_gps_idx > 0) _gps_idx--;
                map_update_gps(_track_lats[_gps_idx], _track_lons[_gps_idx]);
                break;

            default:
                continue;
        }

        map_draw(fb_plot, fb_clear);
        fb_print();
    }

    return 0;
}
