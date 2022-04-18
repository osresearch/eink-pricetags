# Hanshow E-Ink price tags
![Front of the PCB](images/pcb-front.jpg)

These tags are available very cheaply from second-hand sources,
sometimes as low as $1 each.  They don't have any documentation and
this particaular model are programmed over a 2.4 GHz radio, while others
are programmed via IR photodiodes.

## BOM

* TI [MSP430G2553](https://www.ti.com/product/MSP430G2553), 16KB flash, 512 bytes RAM
* Amiccom [A7106](http://www.amiccom.com.tw/asp/product_detail.asp?CATG_ID=2&PRODUCT_ID=109), 2.4GHz ISM band wireless transceiver
* Winbond [25X10CLNIG](https://www.winbond.com/resource-files/w25x10cl_revg%20021714.pdf), 1 megabit SPI flash
* E-Ink display "E0213A04N175I17", likely 122x250 pixels? Might be compatible with [waveshare 2.13" display](https://www.waveshare.com/wiki/2.13inch_e-Paper_HAT)
* 2x CR2450 batteries in parallel

## msp430 JTAG 

![Back of the PCB](images/pcb-rear.jpg)

MSP430 "JTAG" is using the bottom four pins:

* `VCC` - 3.3V
* `GND` - `GND`
* `RST` - `TDIO`
* `TEST` - `TCK`

Connect to the explorer board and probe it with:

```
% mspdebug tilib
MSP430_Initialize: ttyACM1
Firmware version is 31300601
MSP430_VCC: 3000 mV
MSP430_OpenDevice
MSP430_GetFoundDevice
Device: MSP430G2xx3 (id = 0x0067)
2 breakpoints available
MSP430_EEM_Init
Chip ID data:
  ver_id:         5325
  ver_sub_id:     0000
  revision:       00
  fab:            60
  self:           0000
  config:         00
  fuses:          00
Device: MSP430G2xx3
```

Memory layout:

* `0x0000 - 0x000F`: Special function registers
* `0x0010 - 0x00FF`: 8-bit peripheral modules
* `0x0100 - 0x01FF`: 16-bit peripheral modules
* `0x0200 - 0x09FF`: SRAM
* `0x0C00 - 0x0FFF`: Boot flash (not sure)
* `0x1000 - 0x107F`: Information memory
* `0xC000 - 0xFFFF`: Code flash

## spi flash

![SPI flash image](images/spi-bitmap.jpg)

The stock firmware stores the bitmap image in the flash. Extracting it and
plotting it with a 128-bit stride reveals the image.

SPI flash pins:

* `CE` - P3.0 (also pulled high)
* `DO` - P1.6
* `DI` - P1.7
* `CLK` - P1.5
* `!RST` - pulled high
* `!WP` - pulled high

## radio
![A7106 radio through the microscope](images/pcb-radio.jpg)

* `GIO1` - P1.1 (RX data from radio? can be set in mode 0b1000 for direct output)
* `GIO2` - P1.0 (wakeup? ready? can be set to different modes)
* `SDIO` - P1.2 (TX data to radio)
* `SCK` - P1.4 (clock)
* `SCS` - P1.3 (radio chip select)


## e-ink

![e-paper through microscope](images/eink.jpg)

Not 100% positive about model. Typically they are SPI controlled, plus
some extra pins.  The pinout *appears* to match the waveshare design.

* `EPD BUSY` - P2.5? (maybe?)
* `EPD RESET`
* `EPD DC` - P3.5? (data/command selector)
* `EPD CS` - P3.4
* `EPD CLK` - P2.3
* `DI` - P2.4? (data out from mcu into e-ink)
* ??? - P2.2

Commands:

* 0x4E 0x00 - set X address to 0
* 0x4F 0x00 - set Y address to 0
* 0x24 .... - send bitmap data
* 0x12 - sw reset to all parameters (does not clear RAM)
* 0x10 0x01 - deep sleep mode 1, requires hwreset to exit
