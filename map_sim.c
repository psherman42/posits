/*
 * map_sim.c -- ASCII terminal simulator for map_engine.h
 *
 * Simulates the SSD1306 128x64 display in terminal.
 * Tests all three modes, pan, zoom, waypoints, decimation.
 * Uses the three neighborhood walk points as test data.
 *
 *   cc -o map_sim map_sim.c && ./map_sim
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "map_engine.h"

/* -- ASCII framebuffer ----------------------------------------- */

static uint8_t _fb[MAP_H][MAP_W];

static void fb_clear(void) {
    memset(_fb, 0, sizeof(_fb));
}

static void fb_plot(uint8_t x, uint8_t y) {
    if (x < MAP_W && y < MAP_H) _fb[y][x] = 1;
}

static void fb_print(const char *title) {
    int x, y;
    printf("\n+--- %s (zoom=%dm/px mode=%s) ---\n",
           title, map_zoom_meters(),
           _mode==MODE_TRACK?"TRACK":_mode==MODE_PAN?"PAN":"ROUTE");
    /* print every other row and 2 chars per pixel for aspect ratio */
    for (y = 0; y < MAP_H; y += 2) {
        printf("|");
        for (x = 0; x < MAP_W; x += 1) {
            /* check this row and next row */
            int top = _fb[y][x];
            int bot = (y+1 < MAP_H) ? _fb[y+1][x] : 0;
            if (top && bot)      printf("*");
            else if (top)        printf("'");
            else if (bot)        printf(".");
            else                 printf(" ");
        }
        printf("|\n");
    }
    printf("+");
    for (x = 0; x < MAP_W; x++) printf("-");
    printf("+\n");
    printf("  center: lat=%d lon=%d  zoom_ubx=%d\n",
           _view_lat, _view_lon, _zoom_ubx);
}

/* -- test data -- neighborhood walk --------------------------- */
/* From San Jose walk, 2026-11-08:
 * A: 37.40058, -121.92571  (start)
 * B: 37.39873, -121.92434  (238.9m SSE)
 * C: 37.40054, -121.92456  (201.8m N from B)
 * D: far waypoint -- Golden Gate Bridge
 * E: even farther -- downtown Chicago                           */

#define A_LAT  374005821
#define A_LON -1219257115
#define B_LAT  373987283
#define B_LON -1219243436
#define C_LAT  374005352
#define C_LON -1219245582

/* far waypoints for big-picture test */
#define GG_LAT  377108600   /* Golden Gate Bridge */
#define GG_LON -1225001200
#define CH_LAT  418808700   /* Chicago */
#define CH_LON  -876278100

int main(void) {
    int i;

    printf("map_engine.h simulator\n");
    printf("No float. No double. No FPU. Pure integer.\n");
    printf("128x64 OLED rendered as ASCII art.\n");

    /* -- test 1: empty map ------------------------------------- */
    _zoom_ubx = 50000;  /* ~500m/px to start */
    _view_lat = A_LAT;
    _view_lon = A_LON;
    map_draw(fb_plot, fb_clear);
    fb_print("EMPTY MAP");

    /* -- test 2: add neighborhood points one by one ----------- */
    printf("\nAdding neighborhood points A, B, C...\n");
    map_update_gps(A_LAT, A_LON);
    map_update_gps(B_LAT, B_LON);
    map_update_gps(C_LAT, C_LON);
    /* add intermediate track points */
    map_update_gps(A_LAT - 5000,  A_LON + 3000);
    map_update_gps(A_LAT - 10000, A_LON + 6000);
    map_update_gps(A_LAT - 15000, A_LON + 9000);
    map_update_gps(B_LAT + 5000,  B_LON + 2000);
    map_update_gps(B_LAT + 10000, B_LON + 4000);
    map_update_gps(C_LAT, C_LON);
    map_draw(fb_plot, fb_clear);
    fb_print("TRACK MODE -- neighborhood");

    /* -- test 3: zoom in --------------------------------------- */
    printf("\nZoom in (BTN_B short x3)...\n");
    map_btn_b_short();
    map_btn_b_short();
    map_btn_b_short();
    map_draw(fb_plot, fb_clear);
    fb_print("ZOOMED IN");

    /* -- test 4: zoom out -------------------------------------- */
    printf("\nZoom out (BTN_B long x2)...\n");
    map_btn_b_long();
    map_btn_b_long();
    map_draw(fb_plot, fb_clear);
    fb_print("ZOOMED OUT");

    /* -- test 5: set waypoints --------------------------------- */
    printf("\nSet waypoints at B and Golden Gate...\n");
    _gps_lat = B_LAT; _gps_lon = B_LON;
    map_btn_a_long();   /* waypoint at B */
    _wp[1].lat = GG_LAT; _wp[1].lon = GG_LON; _wp_count=2;

    /* -- test 6: ROUTE mode -- shows all ------------------------*/
    printf("\nSwitch to ROUTE mode (BTN_A short x2)...\n");
    map_btn_a_short();  /* -> PAN */
    map_btn_a_short();  /* -> ROUTE */
    map_draw(fb_plot, fb_clear);
    fb_print("ROUTE MODE -- neighborhood + GG Bridge");

    /* -- test 7: add Chicago, really zoom out ------------------ */
    printf("\nAdd Chicago waypoint...\n");
    _wp[2].lat = CH_LAT; _wp[2].lon = CH_LON; _wp_count=3;
    map_draw(fb_plot, fb_clear);
    fb_print("ROUTE MODE -- SF to Chicago");

    /* -- test 8: goto -- zoom into Golden Gate ----------------- */
    printf("\nmap_goto Golden Gate Bridge (1km radius)...\n");
    map_goto(GG_LAT, GG_LON, 100000);  /* 100000 UBX ~ 1km */
    map_draw(fb_plot, fb_clear);
    fb_print("PAN MODE -- Golden Gate area");

    /* -- test 9: decimation test ------------------------------- */
    printf("\nDecimation test: 64 overlapping points...\n");
    map_btn_a_short();  /* -> TRACK */
    _view_lat = A_LAT; _view_lon = A_LON;
    _zoom_ubx = 50000;
    _track_count = 0; _track_head = 0;
    /* add 64 points all within 1 pixel of each other */
    for (i = 0; i < 64; i++)
        map_update_gps(A_LAT + i*10, A_LON + i*10);  /* ~0.1m steps */
    map_draw(fb_plot, fb_clear);
    fb_print("DECIMATION -- 64 points, should show 1-2 dots");

    printf("\nAll tests complete.\n");
    printf("No float. No double. No FPU.\n");
    printf("Integer map engine ready for FE310.\n");
    return 0;
}

