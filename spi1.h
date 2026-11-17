#ifndef SPI1_H__INCLUDED
#define SPI1_H__INCLUDED

#include <stdint.h>
#include <stddef.h>

void spi1_init (uint32_t hfclk_hz, uint32_t spiclk_hz);
void spi1_start (uint8_t ch);
void spi1_stop (uint8_t ch);
uint8_t spi1_transfer (uint8_t x);
void spi1_transblk (uint16_t n, uint8_t *in, uint8_t *out);

#endif//SPI1_H__INCLUDED
