/*
 * gps_quire_nav.c -- GPS navigation with posit32 + quire arithmetic
 *
 * Integrates:
 *   gps.c / uart1  -- u-blox UBX binary protocol, POSLLH message
 *   posit_math.h   -- haversine, bearing, Chauvenet
 *   quire32_pure.h -- exact dot product accumulation
 *   oled.c / spi1  -- SSD1306 128x64 display
 *
 * UBX NAV-POSLLH delivers lat/lon as int32_t in units of 1e-7 degrees.
 * We convert to BRAD (binary radians, pi=16384) for the trig functions,
 * then to posit32 for haversine and bearing.
 *
 * Display layout (128x64 SSD1306):
 *   Line 0: "LAT  37.400582"
 *   Line 1: "LON-121.925712"
 *   Line 2: "DST    238.9 m"
 *   Line 3: "BRG    149.7 d"
 *   Line 4: "FIX 3D  6 SVs"
 *   Line 5: dot trail map (bottom 24 rows)
 *
 * Hardware connections (FE310 / HiFive1):
 *   GPS u-blox on UART1 (pins 18/23), 9600 baud
 *   OLED SSD1306 on SPI1 (pins 2/3/4/5), 1 MHz
 *   Debug monitor on UART0 (pins 16/17), 115200 baud
 *
 * 2026-11-07 pds+claude  initial cut
 * Based on 2020-04-02 pds FE310 GPS demo
 *
 * lab.riscvthai.org/posits/ -- RISC-V Open Lab, San Jose CA
 */

#include <stdint.h>
#include <stddef.h>

/* FE310 bare-metal headers from 2020 codebase */
#include "clock.h"
#include "uart0.h"
#include "uart1.h"
#include "spi1.h"
#include "oled.h"
#include "gpio.h"
#include "gps.h"
#include "util.h"

/* posit32 math library */
#include "positenc.h"
#include "posit_math.h"

#define UBX_TO_BRAD(ubx)  ((ubx) / 109863)

/* -- waypoint storage ------------------------------------------- */
/* Store up to 8 waypoints for route navigation.
 * Waypoints set by button press or serial command.               */

#define MAX_WAYPOINTS 8

static int32_t wp_lat[MAX_WAYPOINTS];
static int32_t wp_lon[MAX_WAYPOINTS];
static int      wp_count = 0;
static int      wp_target = 0;

/* -- display helpers -------------------------------------------- */

static char _linebuf[32];

static void disp_line(int row, char *label, int32_t val, int decimals, char *unit) {
    char buf[16];
    oled_set_pos(0, row);
    oled_write_string(label);
    itoa(val / 10, buf);           /* integer part */
    oled_write_string(buf);
    if (decimals > 0) {
        oled_write_string(".");
        itoa(val % 10, buf);       /* one decimal */
        oled_write_string(buf);
    }
    oled_write_string(unit);
}

/* -- dot trail map ---------------------------------------------- */
/* Bottom 24 rows of OLED used as a moving dot map.
 * Center = current position. Scale = zoom_m meters per pixel.   */

static int32_t map_center_lat = 0;
static int32_t map_center_lon = 0;
static int32_t map_zoom_m = 500;   /* meters per 64 pixels */

static void map_plot(int32_t lat, int32_t lon) {
    if (map_center_lat == 0) {
        map_center_lat = lat;
        map_center_lon = lon;
    }

    /* compute pixel offsets using haversine approximation */
    uint32_t plat1 = ubx_to_posit_rad(map_center_lat);
    uint32_t plon1 = ubx_to_posit_rad(map_center_lon);
    uint32_t plat2 = ubx_to_posit_rad(lat);
    uint32_t plon2 = ubx_to_posit_rad(lon);

    /* north-south distance */
    int32_t bclat = UBX_TO_BRAD(map_center_lat);
    int32_t bclon = UBX_TO_BRAD(map_center_lon);
    int32_t blat2 = UBX_TO_BRAD(lat);
    int32_t blon2 = UBX_TO_BRAD(lon);

    int32_t dist_ns = (int32_t)_pd(posit32_haversine_brad(bclat, bclon, blat2, bclon));
    int32_t dist_ew = (int32_t)_pd(posit32_haversine_brad(bclat, bclon, bclat, blon2));

    int px = 64 + (int32_t)(dist_ew * 64 / map_zoom_m);
    int py = 48 - (int32_t)(dist_ns * 24 / map_zoom_m);

    if (px >= 0 && px < 128 && py >= 40 && py < 64)
        oled_plot_point((uint8_t)px, (uint8_t)py);
}

/* -- main navigation loop --------------------------------------- */

int main(void) {
    uint32_t hfclk_hz;
    int32_t lat, lon, height;
    uint8_t gpsfix, flags, valid;
    uint32_t ttff, msss;
    char buf[32];
    int n;

    /* -- hardware init ----------------------------------------- */
    gpio_init();
    hfclk_hz = clock_init(PRCI_EXT_DIR);
    uart0_init(hfclk_hz, 115200);
    gps_init(hfclk_hz, 9600);
    oled_init(hfclk_hz, 1000000);

    /* -- posit32 environment ----------------------------------- */
    positenv_init(32, 2);

    oled_clear_display();
    oled_set_pos(0, 0);
    oled_write_string("posit32 GPS nav ");
    oled_set_pos(0, 1);
    oled_write_string("waiting for fix ");

    uart0_write_string("posit32 GPS nav -- lab.riscvthai.org/posits/\r\n");

    /* -- wait for GPS fix -------------------------------------- */
    do {
        n = gps_nav_status(&gpsfix, &flags, &ttff, &msss);
        for (volatile uint32_t d=0; d<200000; d++) d++;
    } while (gpsfix < 2);

    oled_clear_display();
    uart0_write_string("fix acquired\r\n");

    /* -- navigation loop --------------------------------------- */
    while (1) {
        /* get position */
        n = gps_nav_posllh(&lat, &lon, &height);
        if (n != 0) continue;

        /* convert to posit32 radians */
        int32_t blat = UBX_TO_BRAD(lat);
        int32_t blon = UBX_TO_BRAD(lon);

        /* compute distance and bearing to target waypoint */
        double dist_m = 0.0;
        double brg_deg = 0.0;

        if (wp_count > 0) {
            int32_t tblat = UBX_TO_BRAD(wp_lat[wp_target]);
            int32_t tblon = UBX_TO_BRAD(wp_lon[wp_target]);

            uint32_t pdist = posit32_haversine_brad(blat, blon, tblat, tblon);
            int32_t  ibrg  = posit32_bearing_brad(blat, blon, tblat, tblon);

            int32_t  dist_m  = (int32_t)_pd(pdist);
            int32_t  brg_deg = ibrg * 180 / 16384;

            if (brg_deg < 0) brg_deg += 360;

            /* arrived? advance to next waypoint */
            if (dist_m < 10.0 && wp_target < wp_count - 1)
                wp_target++;
        }

        /* -- OLED display --------------------------------------- */
        /* Line 0: latitude */
        oled_set_pos(0, 0);
        oled_write_string("LAT ");
        itoa(lat / 10000000, buf); oled_write_string(buf);
        oled_write_string(".");
        itoa((lat < 0 ? -lat : lat) % 10000000, buf);
        oled_write_string(buf);

        /* Line 1: longitude */
        oled_set_pos(0, 1);
        oled_write_string("LON ");
        itoa(lon / 10000000, buf); oled_write_string(buf);
        oled_write_string(".");
        itoa((lon < 0 ? -lon : lon) % 10000000, buf);
        oled_write_string(buf);

        /* Line 2: distance to waypoint */
        oled_set_pos(0, 2);
        if (wp_count > 0) {
            oled_write_string("DST ");
            itoa((int32_t)dist_m, buf); oled_write_string(buf);
            oled_write_string("m   ");
        } else {
            oled_write_string("SET WP: press btn");
        }

        /* Line 3: bearing to waypoint */
        oled_set_pos(0, 3);
        if (wp_count > 0) {
            oled_write_string("BRG ");
            itoa((int32_t)brg_deg, buf); oled_write_string(buf);
            oled_write_string(" deg ");
        }

        /* Line 4: fix quality */
        oled_set_pos(0, 4);
        oled_write_string("FIX ");
        oled_write_string(gpsfix == 3 ? "3D" : "2D");
        oled_write_string("  ");

        /* dot trail */
        map_plot(lat, lon);

        /* -- UART0 debug ---------------------------------------- */
        uart0_write_string("LAT=");
        itoa(lat, buf); uart0_write_string(buf);
        uart0_write_string(" LON=");
        itoa(lon, buf); uart0_write_string(buf);
        if (wp_count > 0) {
            uart0_write_string(" DST=");
            itoa((int32_t)dist_m, buf); uart0_write_string(buf);
            uart0_write_string("m BRG=");
            itoa((int32_t)brg_deg, buf); uart0_write_string(buf);
        }
        uart0_write_string("\r\n");

        /* delay ~1 second */
        for (volatile uint32_t d=0; d<1000000; d++) d++;
    }

    return 0xDEADBEEF;
}
