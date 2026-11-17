////////////////////////////////////////////////
// serial spi #2
//
// 2020-04-05 pds    initial cut
//

#include <stdint.h>  // for uint32_t
#include <stddef.h>  // for size_t
#include "defs.h"
#include "gpio.h"  // for iof_en,iof_sel
#include "spi1.h"


///////////////////////////////////////////////
//
// low level spi routines
//

#define spi1_base 0x10024000
//#define spi0_base 0x10014000
//#define spi2_base 0x10034000

//
// SCKDIV
//

#define spi1_sckdiv   (*(volatile uint32_t *) (spi1_base + 0x00))

// n = (hfclk / spiclk / 2) - 1
#define spi1_div(x)  ( spi1_sckdiv = (uint32_t) (( spi1_sckdiv & ~(0xFFF << 0)) | (( (x) & 0xFFF) << 0)) )

//
// SCKMODE
//

#define spi1_sckmode   (*(volatile uint32_t *) (spi1_base + 0x04))

// 0=sample-shift, 1=shift-sample
#define spi1_pha(x)  ( spi1_sckmode = (uint32_t) (( spi1_sckmode & ~(0x1 << 0)) | (( (x) & 0x1) << 0)) )

// 0=sck-idle-low, 1=sck-idle-high
#define spi1_pol(x)  ( spi1_sckmode = (uint32_t) (( spi1_sckmode & ~(0x1 << 1)) | (( (x) & 0x1) << 1)) )

//
// CSID
//

#define spi1_csid   (*(volatile uint32_t *) (spi1_base + 0x10))

#define spi1_id(x)  ( spi1_csid = (uint32_t) (( spi1_csid & ~(0x3 << 0)) | (( (x) & 0x3) << 0)) )

//
// CSDEF
//

#define spi1_csdef   (*(volatile uint32_t *) (spi1_base + 0x14))

// 0=cs-pins-idle-low, 1=cs-pins-idle-high
#define spi1_def(x)  ( spi1_csdef = (uint32_t) (( spi1_csdef & ~(0xF << 0)) | (( (x) & 0xF) << 0)) )

//
// CSMODE
//

#define spi1_csmode   (*(volatile uint32_t *) (spi1_base + 0x18))

// 0=auto, 2=hold, 3=off
// 0=cs-deassert-each-frame, 2=cs-assert-continuous, 3=off (disable-hw-cs-control)
#define spi1_mode(x)  ( spi1_csmode = (uint32_t) (( spi1_csmode & ~(0x3 << 0)) | (( (x) & 0x3) << 0)) )

//
// DELAY0
//

#define spi1_delay0   (*(volatile uint32_t *) (spi1_base + 0x28))

// n = num cs-to-sck cycles
#define spi1_cssck(x)  ( spi1_delay0 = (uint32_t) (( spi1_delay0 & ~(0xFF << 0)) | (( (x) & 0xFF) << 0)) )

// n = num sck-to-cs cycles
#define spi1_sckcs(x)  ( spi1_delay0 = (uint32_t) (( spi1_delay0 & ~(0xFF << 16)) | (( (x) & 0xFF) << 16)) )

//
// DELAY1
//

#define spi1_delay1   (*(volatile uint32_t *) (spi1_base + 0x2C))

// n = num cycles cs deassert between frames
#define spi1_intercs(x)  ( spi1_delay1 = (uint32_t) (( spi1_delay1 & ~(0xFF << 0)) | (( (x) & 0xFF) << 0)) )

// n = num cycles between frames without deassert cs (mode=off only)
#define spi1_interxfr(x)  ( spi1_delay1 = (uint32_t) (( spi1_delay1 & ~(0xFF << 16)) | (( (x) & 0xFF) << 16)) )

//
// EXTRADEL (undocd)
//

#define spi1_extradel   (*(volatile uint32_t *) (spi1_base + 0x38))

#define spi1_extra_del(x)  ( spi1_extradel = (uint32_t) (( spi1_extradel & ~(0xFFFFFFFF << 0)) | (( (x) & 0xFFFFFFFF) << 0)) )

//
// SAMPLEDEL (undocd)
//

#define spi1_sampledel   (*(volatile uint32_t *) (spi1_base + 0x3C))

#define spi1_sample_del(x)  ( spi1_sampledel = (uint32_t) (( spi1_sampledel & ~(0xFFFFFFFF << 0)) | (( (x) & 0xFFFFFFFF) << 0)) )

//
// FMT
//

#define spi1_fmt   (*(volatile uint32_t *) (spi1_base + 0x40))

// 0=single (DQ0=MOSI, DQ1=MISO), 1=dual, 2=quad
#define spi1_proto(x)  ( spi1_fmt = (uint32_t) (( spi1_fmt & ~(0x3 << 0)) | (( (x) & 0x3) << 0)) )

// 0=msb-first, 1=lsb-first
#define spi1_endian(x)  ( spi1_fmt = (uint32_t) (( spi1_fmt & ~(0x1 << 2)) | (( (x) & 0x1) << 2)) )

// 0=rx: dq-tristate (DQ0=MOSI), 1=tx: receive-fifo-not-populated
#define spi1_dir(x)  ( spi1_fmt = (uint32_t) (( spi1_fmt & ~(0x1 << 3)) | (( (x) & 0x1) << 3)) )

// n = bits-per-frame
#define spi1_len(x)  ( spi1_fmt = (uint32_t) (( spi1_fmt & ~(0xF << 16)) | (( (x) & 0xF) << 16)) )

//
// TXDATA
//

#define spi1_txdata   (*(volatile uint32_t *) (spi1_base + 0x48))

#define spi1_tx_data(x)  ( spi1_txdata = (uint32_t) (( spi1_txdata & ~(0xFF << 0)) | (( (x) & 0xFF) << 0)) )

// 0=ready, 1=not-ready
#define spi1_tx_full()  ( ((spi1_txdata >> 31) & 0x1) )

//
// RXDATA
//

#define spi1_rxdata   (*(volatile uint32_t *) (spi1_base + 0x4C))

#define spi1_rx_data()  ( ((spi1_rxdata >> 0) & 0xFF) )

// 0=ready, 1=not-ready
#define spi1_rx_empty()  ( ((spi1_rxdata >> 31) & 0x1) )

//
// TXMARK
//

#define spi1_txmark   (*(volatile uint32_t *) (spi1_base + 0x50))

#define spi1_tx_mark(x)  ( spi1_txmark = (uint32_t) (( spi1_txmark & ~(0x7 << 0)) | (( (x) & 0x7) << 0)) )

//
// RXMARK
//

#define spi1_rxmark   (*(volatile uint32_t *) (spi1_base + 0x54))

#define spi1_rx_mark(x)  ( spi1_rxmark = (uint32_t) (( spi1_rxmark & ~(0x7 << 0)) | (( (x) & 0x7) << 0)) )

//
// FCTRL
//

#define spi1_fctrl   (*(volatile uint32_t *) (spi1_base + 0x60))

// 0=dis, 1=ena
#define spi1_fctrl_en(x)  ( spi1_fctrl = (uint32_t) (( spi1_fctrl & ~(0x1 << 0)) | (( (x) & 0x1) << 0)) )

//
// FFMT
//

#define spi1_ffmt   (*(volatile uint32_t *) (spi1_base + 0x64))

// 0=dis, 1=ena
#define spi1_cmd_en(x)  ( spi1_ffmt = (uint32_t) (( spi1_ffmt & ~(0x1 << 0)) | (( (x) & 0x1) << 0)) )

// n = num-address-bytes
#define spi1_addr_len(x)  ( spi1_ffmt = (uint32_t) (( spi1_ffmt & ~(0x7 << 1)) | (( (x) & 0x7) << 1)) )

// n = num-dummy-cycles
#define spi1_pad_cnt(x)  ( spi1_ffmt = (uint32_t) (( spi1_ffmt & ~(0xF << 4)) | (( (x) & 0xF) << 4)) )

// command transmit protocol
#define spi1_cmd_proto(x)  ( spi1_ffmt = (uint32_t) (( spi1_ffmt & ~(0x3 << 8)) | (( (x) & 0x3) << 8)) )

// address and padding transmit protocol
#define spi1_addr_proto(x)  ( spi1_ffmt = (uint32_t) (( spi1_ffmt & ~(0x3 << 10)) | (( (x) & 0x3) << 10)) )

// data byte receive protocol
#define spi1_data_proto(x)  ( spi1_ffmt = (uint32_t) (( spi1_ffmt & ~(0x3 << 12)) | (( (x) & 0x3) << 12)) )

// command byte
#define spi1_cmd_code(x)  ( spi1_ffmt = (uint32_t) (( spi1_ffmt & ~(0xFF << 16)) | (( (x) & 0xFF) << 16)) )

// dummy byte value
#define spi1_pad_code(x)  ( spi1_ffmt = (uint32_t) (( spi1_ffmt & ~(0xFF << 24)) | (( (x) & 0xFF) << 24)) )

//
// IE
//

#define spi1_ie   (*(volatile uint32_t *) (spi1_base + 0x70))

// 0=dis, 1=ena
#define spi1_ie_txwm(x)  ( spi1_ie = (uint32_t) (( spi1_ie & ~(0x1 << 0)) | (( (x) & 0x1) << 0)) )

// 0=dis, 1=ena
#define spi1_ie_rxwm(x)  ( spi1_ie = (uint32_t) (( spi1_ie & ~(0x1 << 1)) | (( (x) & 0x1) << 1)) )

//
// IP
//

#define spi1_ip   (*(volatile uint32_t *) (spi1_base + 0x74))

// 0=none, 1=pending
#define spi1_ip_txwm()  ( ((spi1_ip >> 0) & 0x1) )

// 0=none, 1=pending
#define spi1_ip_rxwm()  ( ((spi1_ip >> 1) & 0x1) )


///////////////////////////////////////////////
//
// high level spi routines
//

#define SPI1_SCK  5
#define SPI1_MOSI 3
#define SPI1_MISO 4
#define SPI1_SS0  2
#define SPI1_SS1  8
#define SPI1_SS2  9
#define SPI1_SS3  10
uint8_t gpio_ss_[4] = { SPI1_SS0, SPI1_SS1, SPI1_SS2, SPI1_SS3 };

void spi1_init (uint32_t hfclk_hz, uint32_t spiclk_hz)
{
  gpio_iof( 5, GPIO_SPI );  // spi1, sck, 48-QFN pin _, output from SoC
  gpio_iof( 3, GPIO_SPI );  // spi1, mosi, 48-QFN pin _, output from SoC
  gpio_iof( 4, GPIO_SPI );  // spi1, miso, 48-QFN pin _, input to SoC
  gpio_iof( 2, GPIO_SPI );  // spi1, ss0, 48-QFN pin _, output from SoC
  gpio_iof( 8, GPIO_SPI );  // spi1, ss1, 48-QFN pin _, output from SoC ***not on 64QFN***
  gpio_iof( 9, GPIO_SPI );  // spi1, ss2, 48-QFN pin _, output from SoC
  gpio_iof( 10, GPIO_SPI );  // spi1, ss3, 48-QFN pin _, output from SoC

  /////////////////
  // after copy/paste, for spi2()
  //gpio_iof( x, GPIO_SPI );  // spi2, sck, 48-QFN pin _, output from SoC
  //gpio_iof( x, GPIO_SPI );  // spi2, mosi, 48-QFN pin _, output from SoC
  //gpio_iof( x, GPIO_SPI );  // spi2, miso, 48-QFN pin _, input to SoC
  //gpio_iof( x, GPIO_SPI );  // spi2, ss0, 48-QFN pin _, output from SoC
  //gpio_iof( x, GPIO_SPI );  // spi2, ss1, 48-QFN pin _, output from SoC
  //gpio_iof( x, GPIO_SPI );  // spi2, ss2, 48-QFN pin _, output from SoC
  //gpio_iof( x, GPIO_SPI );  // spi2, ss3, 48-QFN pin _, output from SoC
  // after copy/paste, for spi2()
  /////////////////

  spi1_div( (hfclk_hz / spiclk_hz / 2) - 1 );

  spi1_fctrl_en( 0 );  // disable flash mode

  spi1_proto( 0 );  // single channel
  spi1_endian( 0 );  // msb first
  spi1_dir( 0 );  // rx
  spi1_len( 8 );  // bits per frame

  spi1_pha( 0 );  // sck idles low
//  spi1_pha( 1 );  // sck idles high
  spi1_pol( 0 );  // sample (sck rising edge) then shift (sck falling edge)
//  spi1_pol( 1 );  // shift (sck rising edge) then sample (sck falling edge)
  spi1_cssck( 31 );  // def 1
  spi1_sckcs( 31 );  // def 1
  spi1_intercs( 15 );  // def 1
  spi1_interxfr( 31 );  // def 0
//  spi1_cssck( 1 );  // def 1
//  spi1_sckcs( 1 );  // def 1
//  spi1_intercs( 1 );  // def 1
//  spi1_interxfr( 0 );  // def 0

  ////////////////// UNDOCd ///////////////////////
  // CRITICAL for long-wire hookup and noisy environ
  //
  spi1_extra_del( 0x00000fff );  // extradel.coarse<11:0> [11:0]
  spi1_sample_del( 0x0000001f );  // sampledel.sd<5:0> [5:0]
//  spi1_extra_del( 0x00000000 );  // extradel.coarse<11:0> [11:0]
//  spi1_sample_del( 0x00000000 );  // sampledel.sd<5:0> [5:0]
  //
  // CRITICAL
  /////////////////////////////////////////////////

  spi1_tx_mark( 1 );
  spi1_rx_mark( 0 );
  spi1_ie_txwm( 0 );  // disable int
  spi1_ie_rxwm( 0 );  // disable int

  spi1_def( 0xF );  // SS3,SS2,SS1,SS0 idle High
  spi1_id( 0 );  // 0=SS0, 1=SS1, 2=SS2, 3=SS3
  spi1_mode( 0 );  // auto, assert-deassert cs each frame
}

enum spi_ch_t { SPI_SS0, SPI_SS1, SPI_SS2, SPI_SS3 };
enum spi_mode_t { SPI_MODE_AUTO, SPI_MODE_X, SPI_MODE_HOLD, SPI_MODE_OFF };

void spi1_start (uint8_t ch)
{
  if (ch > 3) return;

//  ra = GET32( GPIO_IOF_EN );
//  ra |= (1UL << SPI1_SCK);  // SCLK
//  ra |= (1UL << SPI1_MOSI);  // SDA
//  PUT32( GPIO_IOF_EN, ra );

  spi1_id( ch );  // 0=SS0, 1=SS1, 2=SS2, 3=SS3
  spi1_mode( SPI_MODE_HOLD );  // 2 HOLD
}

void spi1_stop (uint8_t ch)
{
  if (ch > 3) return;

  spi1_id( ch );  // 0=SS0, 1=SS1, 2=SS2, 3=SS3
  spi1_mode( SPI_MODE_AUTO );  // 0 AUTO
}

uint8_t spi1_transfer (uint8_t x)
{
  while( spi1_ip_txwm() == 0 );  // wait for tx flush

  spi1_tx_data( x );

  while( spi1_ip_txwm() == 0 );  // wait for tx flush

  while( spi1_ip_rxwm() == 0 );  // wait for rx char

  return spi1_rx_data();

/*
  do
  {
    PUT32( SPI1_TXDATA, x );
  }
  while( (GET32(SPI1_TXDATA) & 0x80000000) == 0x80000000 );  // full

  //while( (GET32(SPI1_IP) & 0x00000001) == 0x00000001 );  // txwm: 1=n<txmark
  //while( (GET32(SPI1_IP) & 0x00000002) == 0x00000002 );  // rxwm: 1=n>rxmark

  for(ra=0; ra<100; ra++);

  do
  {
    ra = GET32( SPI1_RXDATA );
  }
  while( (ra & 0x80000000) == 0x80000000 );  // empty

  return ( ra & 0xFF );
*/
}

void spi1_transblk (uint16_t n, uint8_t *in, uint8_t *out)
{
  uint16_t i;
  uint8_t c;

  // should use wm and fifo n % depth here
  for (i=0; i<n; i++)
  {
    c = spi1_transfer( in[i] );
    if (out != NULL) out[i] = c;
  }
}

