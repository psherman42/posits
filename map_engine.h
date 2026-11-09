/*
 * map_engine.h -- integer map engine for FE310 GPS navigator
 *
 * Type this in. Include it. No float. No double. No FPU.
 *
 * Three view modes:
 *   MODE_TRACK -- follows GPS, auto-scales to recent track
 *   MODE_PAN   -- manual pan/zoom, cursor free from GPS
 *   MODE_ROUTE -- shows all waypoints + track, auto-scales to fit
 *
 * All positions in UBX units (int32_t, 1e-7 degrees).
 * Screen projection: pure integer subtract and divide.
 * No trig. No haversine. No float anywhere.
 *
 * Display: 128 x 64 SSD1306 OLED
 *   x: 0=left   127=right   center=64
 *   y: 0=top     63=bottom  center=32
 *
 * Controls (3 buttons):
 *   BTN_A short: cycle modes    long: set waypoint
 *   BTN_B short: zoom in        long: zoom out
 *   BTN_C short: pan N/E/S/W    long: jump to current position
 *
 * Decimation: a point is only plotted if it differs from the
 * last plotted point by at least 1 pixel at current zoom.
 * Automatically suppresses overlapping dots at any zoom level.
 *
 * 2026-11-08 pds+claude  initial cut
 * lab.riscvthai.org/posits/ -- RISC-V Open Lab, San Jose CA
 */

#ifndef MAP_ENGINE_H
#define MAP_ENGINE_H

#include <stdint.h>

/* -- display geometry ------------------------------------------ */

#define MAP_W    128
#define MAP_H     64
#define MAP_CX    64   /* center x */
#define MAP_CY    32   /* center y */

/* -- data structures ------------------------------------------- */

#define TRACK_MAX  64   /* ring buffer of recent GPS positions   */
#define WP_MAX      8   /* waypoints                             */

typedef struct {
    int32_t lat;        /* UBX 1e-7 degrees */
    int32_t lon;        /* UBX 1e-7 degrees */
} map_point_t;

typedef enum {
    MODE_TRACK = 0,     /* follow GPS, auto-scale to track       */
    MODE_PAN   = 1,     /* manual pan/zoom, free cursor          */
    MODE_ROUTE = 2,     /* all waypoints + track, auto-scale     */
    MODE_COUNT = 3
} map_mode_t;

typedef enum {
    PAN_N = 0, PAN_E = 1, PAN_S = 2, PAN_W = 3
} pan_dir_t;

/* -- map state ------------------------------------------------- */

static map_point_t _track[TRACK_MAX];   /* ring buffer           */
static int         _track_head  = 0;
static int         _track_count = 0;

static map_point_t _wp[WP_MAX];         /* waypoints             */
static int         _wp_count    = 0;
static int         _wp_target   = 0;

static int32_t  _view_lat  = 0;         /* map center, UBX       */
static int32_t  _view_lon  = 0;
static int32_t  _zoom_ubx  = 10000;     /* UBX units per pixel   */
                                        /* 10000 ~ 1m/px         */
static map_mode_t  _mode   = MODE_TRACK;
static pan_dir_t   _pan_dir = PAN_N;

static int32_t  _gps_lat   = 0;         /* current GPS position  */
static int32_t  _gps_lon   = 0;

static int32_t  _last_plotted_lat = 0;  /* decimation state      */
static int32_t  _last_plotted_lon = 0;

static int _manual_zoom = 0;  /* add near other state variables  */

/* -- integer abs ----------------------------------------------- */

static int32_t _iabs(int32_t x) { return x < 0 ? -x : x; }

/* -- screen projection -- pure integer ------------------------- */
/* Convert UBX lat/lon to screen pixel coordinates.
 * Returns 255 if point is off-screen.                           */

static uint8_t map_to_px(int32_t lon) {
    int32_t dx = (lon - _view_lon) / _zoom_ubx;
    int32_t px = MAP_CX + dx;
    if (px < 0 || px >= MAP_W) return 255;
    return (uint8_t)px;
}

static uint8_t map_to_py(int32_t lat) {
    int32_t dy = (lat - _view_lat) / _zoom_ubx;
    int32_t py = MAP_CY - dy;   /* lat increases northward, y increases down */
    if (py < 0 || py >= MAP_H) return 255;
    return (uint8_t)py;
}

/* -- decimation check ------------------------------------------ */
/* Returns 1 if point is worth plotting (far enough from last).
 * Threshold: 1 pixel = zoom_ubx UBX units.
 * Uses Manhattan distance for speed -- no multiply needed.      */

static int map_should_plot(int32_t lat, int32_t lon) {
    int32_t dlat = _iabs(lat - _last_plotted_lat);
    int32_t dlon = _iabs(lon - _last_plotted_lon);
    return (dlat + dlon) >= _zoom_ubx;
}

/* -- store GPS position -------------------------------------- */
/* Add to ring buffer if moved enough to matter.
 * Threshold: 1000 UBX units ~ 0.1m -- filters GPS jitter.      */

static void map_update_gps(int32_t lat, int32_t lon) {
    int32_t dlat = _iabs(lat - _gps_lat);
    int32_t dlon = _iabs(lon - _gps_lon);

    _gps_lat = lat;
    _gps_lon = lon;

    if ((dlat + dlon) < 1000 && _track_count > 0) return;

    /* store in ring buffer */
    _track[_track_head].lat = lat;
    _track[_track_head].lon = lon;
    _track_head = (_track_head + 1) % TRACK_MAX;
    if (_track_count < TRACK_MAX) _track_count++;

    /* in TRACK mode, re-center on current position */
    if (_mode == MODE_TRACK) {
        _view_lat = lat;
        _view_lon = lon;
    }
}

/* -- auto-scale to fit a set of points ------------------------ */
/* Finds bounding box, sets zoom and center to fit all points
 * on screen with 10% margin. Pure integer arithmetic.          */

static void map_auto_scale(map_point_t *pts, int n) {
    int32_t min_lat, max_lat, min_lon, max_lon;
    int32_t span_lat, span_lon, span, margin;
    int i;

    if (n == 0) return;

    min_lat = max_lat = pts[0].lat;
    min_lon = max_lon = pts[0].lon;

    for (i = 1; i < n; i++) {
        if (pts[i].lat < min_lat) min_lat = pts[i].lat;
        if (pts[i].lat > max_lat) max_lat = pts[i].lat;
        if (pts[i].lon < min_lon) min_lon = pts[i].lon;
        if (pts[i].lon > max_lon) max_lon = pts[i].lon;
    }

    _view_lat = (int32_t)(((int64_t)min_lat + max_lat) / 2);
    _view_lon = (int32_t)(((int64_t)min_lon + max_lon) / 2);

    span_lat = (max_lat - min_lat);
    span_lon = (max_lon - min_lon);

    /* use larger span to set zoom */
    span = span_lat > span_lon ? span_lat : span_lon;

    /* add 10% margin */
    margin = span / 10;
    span += margin * 2;

    if (span < 1000) span = 1000;   /* minimum zoom */

    /* zoom: UBX units per pixel, fit span in min(W,H) pixels */
    _zoom_ubx = span / (MAP_H < MAP_W ? MAP_H : MAP_W);
    if (_zoom_ubx < 1) _zoom_ubx = 1;
}

/* -- redraw the map -------------------------------------------- */
/* Caller provides oled_plot_point and oled_clear_display.
 * We draw: track dots, waypoint markers, current position cross.*/

static void map_draw(void (*plot)(uint8_t x, uint8_t y),
                     void (*clear)(void)) {
    int i, idx;
    uint8_t px, py;

    clear();

    /* auto-scale in ROUTE mode */
    if (_mode == MODE_ROUTE) {
        /* build combined point list: track + waypoints */
        map_point_t all[TRACK_MAX + WP_MAX];
        int n = 0;
        for (i = 0; i < _track_count; i++)
            all[n++] = _track[i];
        for (i = 0; i < _wp_count; i++)
            all[n++] = _wp[i];
        if (!_manual_zoom) map_auto_scale(all, n);
    }

    /* auto-scale in TRACK mode: fit recent track */
    if (_mode == MODE_TRACK && _track_count > 1) {
        if (!_manual_zoom) map_auto_scale(_track, _track_count);
        /* keep center on current position */
        _view_lat = _gps_lat;
        _view_lon = _gps_lon;
    }

    /* draw track dots with decimation */
    _last_plotted_lat = 0;
    _last_plotted_lon = 0;
    for (i = 0; i < _track_count; i++) {
        idx = (_track_head - _track_count + i + TRACK_MAX) % TRACK_MAX;
        int32_t lat = _track[idx].lat;
        int32_t lon = _track[idx].lon;
        if (!map_should_plot(lat, lon)) continue;
        px = map_to_px(lon);
        py = map_to_py(lat);
        if (px < 255 && py < 255) {
            plot(px, py);
            _last_plotted_lat = lat;
            _last_plotted_lon = lon;
        }
    }

    /* draw waypoints as small cross */
    for (i = 0; i < _wp_count; i++) {
        px = map_to_px(_wp[i].lon);
        py = map_to_py(_wp[i].lat);
        if (px < 255 && py < 255) {
            plot(px, py);
            if (px > 0)   plot(px-1, py);
            if (px < 127) plot(px+1, py);
            if (py > 0)   plot(px, py-1);
            if (py < 63)  plot(px, py+1);
        }
    }

    /* draw current position as larger cross */
    px = map_to_px(_gps_lon);
    py = map_to_py(_gps_lat);
    if (px < 255 && py < 255) {
        plot(px, py);
        if (px > 1)   { plot(px-1,py); plot(px-2,py); }
        if (px < 126) { plot(px+1,py); plot(px+2,py); }
        if (py > 1)   { plot(px,py-1); plot(px,py-2); }
        if (py < 62)  { plot(px,py+1); plot(px,py+2); }
    }

    /* in PAN mode: draw cursor crosshair at center */
    if (_mode == MODE_PAN) {
        plot(MAP_CX,     MAP_CY);
        plot(MAP_CX - 3, MAP_CY);
        plot(MAP_CX + 3, MAP_CY);
        plot(MAP_CX,     MAP_CY - 3);
        plot(MAP_CX,     MAP_CY + 3);
    }
}

/* -- button handlers ------------------------------------------- */

static void map_btn_a_short(void) {
    /* cycle modes */
    _mode = (map_mode_t)(((int)_mode + 1) % (int)MODE_COUNT);
    _manual_zoom = 0;               /* reset on mode change */
    /* entering PAN: freeze view at current center */
    /* entering TRACK: re-center on GPS */
    if (_mode == MODE_TRACK) {
        _view_lat = _gps_lat;
        _view_lon = _gps_lon;
    }
}

static void map_btn_a_long(void) {
    /* set waypoint at current GPS position */
    if (_wp_count < WP_MAX) {
        _wp[_wp_count].lat = _gps_lat;
        _wp[_wp_count].lon = _gps_lon;
        _wp_count++;
    }
}

static void map_btn_b_short(void) {
    _manual_zoom = 1;               /* lock zoom from auto-scale */
    /* zoom in: halve zoom_ubx */
    _zoom_ubx /= 2;
    if (_zoom_ubx < 100) _zoom_ubx = 100;   /* max zoom: ~10m span */
}

static void map_btn_b_long(void) {
    _manual_zoom = 1;               /* lock zoom from auto-scale */
    /* zoom out: double zoom_ubx */
    _zoom_ubx *= 2;
    if (_zoom_ubx > 10000000) _zoom_ubx = 10000000; /* ~1000km span */
}

static void map_btn_c_short(void) {
    /* pan in current direction -- step = 8 pixels at current zoom */
    int32_t step = _zoom_ubx * 8;
    switch (_pan_dir) {
        case PAN_N: _view_lat += step; break;
        case PAN_S: _view_lat -= step; break;
        case PAN_E: _view_lon += step; break;
        case PAN_W: _view_lon -= step; break;
    }
    /* cycle pan direction after each press */
    _pan_dir = (pan_dir_t)(((int)_pan_dir + 1) % 4);
}

static void map_btn_c_long(void) {
    /* jump view center to current GPS position */
    _view_lat = _gps_lat;
    _view_lon = _gps_lon;
}

/* -- zoom to show a specific area ----------------------------- */
/* Call with center UBX and radius in UBX units.
 * Useful for "show me waypoint N and surroundings".             */

static void map_goto(int32_t lat, int32_t lon, int32_t radius_ubx) {
    _view_lat = lat;
    _view_lon = lon;
    _zoom_ubx = (radius_ubx * 2) / (MAP_H < MAP_W ? MAP_H : MAP_W);
    if (_zoom_ubx < 1) _zoom_ubx = 1;
    _mode = MODE_PAN;
}

/* -- query current zoom in meters (approximate) --------------- */
/* 1 UBX unit = 1e-7 degrees ~ 0.011132 meters at equator.
 * Adjust for latitude: scale by cos(lat) in integer.           */

static int32_t map_zoom_meters(void) {
    /* approximate: 1 UBX = 11 mm at equator */
    return (_zoom_ubx * 11) / 1000;  /* meters per pixel */
}

#endif /* MAP_ENGINE_H */
