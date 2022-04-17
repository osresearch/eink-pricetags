# Hanshow E-Ink price tags
![Front of the PCB](images/pcb-front.jpg)

* TI [MSP430G2553](https://www.ti.com/product/MSP430G2553), 16KB flash, 512 bytes RAM
* Amiccom [A7106](http://www.amiccom.com.tw/asp/product_detail.asp?CATG_ID=2&PRODUCT_ID=109), 2.4GHz ISM band wireless transceiver
* Winbond [25X10CLNIG](https://www.winbond.com/resource-files/w25x10cl_revg%20021714.pdf), 1 megabit SPI flash
* E-Ink display, 128? x 256? pixels.
* 2x CR2450 batteries in parallel

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

![SPI flash image](images/spi-bitmap.jpg)

The stock firmware stores the bitmap image in the flash. Extracting it and
plotting it with a 128-bit stride reveals the image.


