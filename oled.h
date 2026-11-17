#ifndef OLED_H__INCLUDED
#define OLED_H__INCLUDED

#include <stdint.h>
#include <stddef.h>  // for size_t

void oled_init (uint32_t hfclk_hz, uint32_t spiclk_hz);
void oled_clear_display (void);
void oled_set_pos (uint8_t x, uint8_t y);
void oled_write (char c);
void oled_write_string (char *s);
enum color_t { COLOR_BLACK=0, COLOR_WHITE=1 };
void oled_color (enum color_t c);
void oled_origin (int16_t x, int16_t y);
void oled_plot_point (uint8_t x, uint8_t y);
void oled_move_to (int16_t x, int16_t y);
void oled_draw_to (int16_t x, int16_t y);
void oled_draw_circle (int16_t r);

#endif//OLED_H__INCLUDED
