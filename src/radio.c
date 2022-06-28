/*
 * Amiccom A7106 radio interface.
 *
 * SPI data format:
 * A7 A6 A5 A4 A3 A2 A1 A0
 * |  |  Address of control register
 * |  R/!W: 1 = Read, 0 = Write
 * Strobe: 0 = Normal, 1 = Strobe command (only four bits count)
 *
 * Most of the time it is in the the idle mode (1.9uA).
 * Note that there needs to be a calibration step (section 15 of datasheet)
 *
 * TX @ 0dBm = 19 mA
 * RX = 16 mA
 *
 */

#include "pins.h"

#define RADIO_SDIO	0x12
#define RADIO_SCK	0x14
#define RADIO_SCS	0x13
#define RADIO_POWER	0x27

#define RADIO_CMD_SLEEP	0x80
#define RADIO_CMD_IDLE	0x90
#define RADIO_CMD_STBY	0xA0
#define RADIO_CMD_PLL	0xB0
#define RADIO_CMD_RX	0xC0
#define RADIO_CMD_TX	0xD0
#define RADIO_CMD_WRITE_FIFO_RESET	0xE0
#define RADIO_CMD_READ_FIFO_RESET	0xF0

#define BIT(x) ((const uint8_t)(1 << (x)))

// when reading a register, this bit is set in the command byte
#define RADIO_READ_BIT BIT(6)

// calibration control register
#define A7106_REG_CALC 0x02
#define A7106_REG_CALC_RSSC BIT(3)
#define A7106_REG_CALC_VCC BIT(2)
#define A7106_REG_CALC_VBC BIT(1)
#define A7106_REG_CALC_FBC BIT(0)

// RX and TX fifos
#define A7106_REG_FIFO 0x05

#define A7106_REG_ID 0x06

#define A7106_REG_RCOSC1 0x07
#define A7106_REG_RCOSC2 0x08
#define A7106_REG_RCOSC3 0x09
#define A7106_REG_RCOSC3_CALW BIT(3)
#define A7106_REG_RCOSC3_EN BIT(2)
#define A7106_REG_RCOSC3_TSEL BIT(1)
#define A7106_REG_RCOSC3_WORMS BIT(0)

#define A7106_REG_IF_CALIBRATION1 0x22
#define A7106_REG_IF_CALIBRATION1_MFBS	BIT(4) // write
#define A7106_REG_IF_CALIBRATION1_FBCF	BIT(4) // read

#define A7106_REG_IF_CALIBRATION2 0x23

#define A7106_REG_VCO_CURRENT_CALIBRATION 0x24
#define A7106_REG_VCO_CURRENT_CALIBRATION_MVCS BIT(4) // write
#define A7106_REG_VCO_CURRENT_CALIBRATION_VCCF BIT(4) // read

#define A7106_REG_VCO_BAND_CALIBRATION 0x25
#define A7106_REG_VCO_BAND_CALIBRATION_MVBS BIT(3) // write
#define A7106_REG_VCO_BAND_CALIBRATION_VBCF BIT(3) // read

#define A7106_REG_CHARGE_PUMP 0x2b
#define A7106_REG_CHARGE_PUMP_CPC0 BIT(0)
#define A7106_REG_CHARGE_PUMP_CPC1 BIT(1)
#define A7106_REG_CHARGE_PUMP_LVR BIT(2)
#define A7106_REG_CHARGE_PUMP_STS BIT(3)
#define A7106_REG_CHARGE_PUMP_RGC0 BIT(4)
#define A7106_REG_CHARGE_PUMP_RGC1 BIT(5)
#define A7106_REG_CHARGE_PUMP_CELS BIT(6)
#define A7106_REG_CHARGE_PUMP_ROSCS BIT(7)

volatile uint8_t radio_status;

/*
 * busy loop delay, with their constant.
 */
static void delay(uint16_t loops)
{
	for(uint16_t i = 0 ; i < loops ; i++)
		for(uint16_t j = 0 ; j < 0x1D1 ; j++)
			__asm__("");
}

/*
 * The radio is used in a three-wire mode, where the SDIO pin
 * switches directions for a read command.
 * This means that simultaneous bi-directional data is not supported,
 * although none of the protocol requires it.
 */
static void radio_write_byte(uint8_t byte)
{
	spi_write(RADIO_SCK, RADIO_SDIO, 0, byte);
}

static uint8_t radio_read_byte(void)
{
	return spi_write(RADIO_SCK, 0, RADIO_SDIO, 0);
}

static void radio_cs(uint8_t selected)
{
	pin_write(RADIO_SCS, !selected);
}

static void radio_strobe(const uint8_t cmd)
{
	radio_cs(1);
	radio_write_byte(cmd);
	radio_cs(0);
}

static void radio_reg_write_buf(const uint8_t cmd, const uint8_t * data, const unsigned len)
{
	// should confirm that cmd doesn't have bit 7 set
	radio_cs(1);
	pin_ddr(RADIO_SDIO, 1);
	radio_write_byte(cmd & ~RADIO_READ_BIT);

	for(unsigned i = 0 ; i < len ; i++)
		radio_write_byte(data[i]);

	radio_cs(0);
}

static void radio_reg_read_buf(const uint8_t cmd, uint8_t * data, const unsigned len)
{
	radio_cs(1);
	pin_ddr(RADIO_SDIO, 1);
	radio_write_byte(cmd | RADIO_READ_BIT);

	// use the same pin for input
	pin_ddr(RADIO_SDIO, 0);
	for(unsigned i = 0 ; i < len ; i++)
		data[i] = radio_read_byte();

	pin_ddr(RADIO_SDIO, 1);
	radio_cs(0);
}

static void radio_reg_write(const uint8_t cmd, const uint8_t data)
{
	radio_reg_write_buf(cmd, &data, 1);
}

static uint8_t radio_reg_read(const uint8_t cmd)
{
	uint8_t data;
	radio_reg_read_buf(cmd, &data, 1);
	return data;
}



/*
 * 15. Calibration 
 * A7106 needs calibration process after power on reset or software reset by 3 calibration items,
 * they are, VCO Current, VCO Bank, and IF Filter Bank.
 *
 * 1. VCO Current Calibration (Standby or PLL mode) is used to find adequate VCO current.
 * 2. VCO Bank Calibration (PLL mode) is used to select best VCO frequency bank for the calibrated frequency.
 * 3. IF Filter Bank Calibration (Standby or PLL mode) is used to calibrate IF filter bandwidth and center frequency.
 * 4. RSSI Calibration is to find the RSSI value corresponding to -70dBm RF input and RSSI curve.
 *
 * 15.1 Calibration Procedure
 * 1. Initialize all control registers (refer to A7106 reference code).
 * 2. Select calibration mode (set MFBS=0, MVCS=1, MVBS=0).
 * 3. Set A7106 in PLL mode.
 * 4. Enable IF Filter Bank (set FBC, RSSC=1), VCO Current (VCC=1), and VCO Bank (VBC=1).
 * 5. After calibration done, FBC, VCC and VBC is auto clear.
 * 6. Check pass or fail by reading calibration flag (FBCF) and (VCCF, VBCF).
 */
volatile uint32_t spins = 0;

static int radio_calibrate(void)
{
 	// 3. Set A7106 in PLL mode.
	radio_strobe(RADIO_CMD_PLL);
	delay(400);

	// 4. Enable IF Filter Bank (set FBC, RSSC=1), VCO Current (VCC=1), and VCO Bank (VBC=1).
	radio_reg_write(A7106_REG_CALC, 0
		| A7106_REG_CALC_VCC
		| A7106_REG_CALC_VBC
		| A7106_REG_CALC_FBC
	);

	// 5. After calibration done, FBC, VCC and VBC is auto clear.
	for(unsigned count = 200 ; count != 0 ; count--)
	{
		if (radio_reg_read(A7106_REG_CALC) == 0)
			break;
		if (count == 0)
		{
			radio_status = 1;
			return 0;
		}
	}

	// 6. Check pass or fail by reading calibration flag (FBCF) and (VCCF, VBCF).
	if (radio_reg_read(A7106_REG_IF_CALIBRATION1) & A7106_REG_IF_CALIBRATION1_FBCF)
	{
		radio_status = 2;
		return 0;
	}

	if (radio_reg_read(A7106_REG_VCO_CURRENT_CALIBRATION) & A7106_REG_VCO_CURRENT_CALIBRATION_VCCF)
	{
		radio_status = 3;
		return 0;
	}

	if (radio_reg_read(A7106_REG_VCO_BAND_CALIBRATION) & A7106_REG_VCO_BAND_CALIBRATION_VBCF)
	{
		radio_status = 4;
		return 0;
	}

	return 1;
}


/* Chapter 20.1
 * 1. Set A7106 in standby mode.
 * 2. Select either GIO1 or GIO2 pin to wake up MCU.
 * (2-1) Set GIO1S [3:0] = [0100] to let WAK from GIO1.
 * (2-2) Set GIO2S [3:0] = [0100] to let WAK from GIO2.
 * 3. Set period of WOR_AC (08h) and WOR_SL (07h and 08h).
 * 4. Enable RCO Calibration: set RCOT = 1, RCOSC_E = 1, CALW = 1, WOR_MS =0(09h).
 * 5. Check if CALR = 0 (07h), calibration done.
 * 6. Read RCOC[6:0] (07h)
 * (6-1) If RCOC >= 0x14, go to STEP7.
 * (6-2) If RCOC < 0x14, Increase RCOT [1:0] by 1, then, go to STEP4 until RCOC [6:0] >= 0x14.
 * 7. Enable WORE = 1 (01h).
 * 8. Once a packet is received, GIO1 or GIO2 pin outputs WAK to wake up MCU.
 * 9. A7106 will end up WOR function and stay in sleep mode.
 */
static int radio_osc_setup(void)
{
	for(unsigned i = 0 ; i < 4 ; i++)
	{
		radio_reg_write(A7106_REG_RCOSC3, 0);
		radio_reg_write(A7106_REG_RCOSC3, (i << 4) | A7106_REG_RCOSC3_CALW | A7106_REG_RCOSC3_TSEL);
	}

	return 0;
}


uint8_t id[9];

void radio_tx_buf(const uint8_t *buf, const unsigned len)
{
	radio_reg_write_buf(A7106_REG_FIFO, buf, len);
	radio_strobe(RADIO_CMD_TX);

	delay(10);
}

// todo: document these registers and their bits
static const uint8_t radio_init_cmd[] = {
	0x01, 0x62,
	0x03, 0x3f,
	0x04, 0x40,
	0x07, 0xff,
	0x08, 0xcb,
	0x09, 0x00,
	0x0a, 0x00,
	0x0b, 0x01,
	0x0c, 0x13,
	0x0d, 0x05,
	0x0e, 0x00,
	0x0f, 0x64,
	0x10, 0x9e,
	0x11, 0x4b,
	0x12, 0x00,
	0x13, 0x02,
	0x14, 0x16,
	0x15, 0x2b,
	0x16, 0x12,
	0x17, 0x40,
	0x18, 0x62,
	0x19, 0x80,
	0x1a, 0x80,
	0x1b, 0x00,
	0x1c, 0x0a,
	0x1d, 0x32,
	0x1e, 0xc3,
	0x1f, 0x0f,
	0x20, 0x12,
	0x21, 0x00,
	0x22, 0x00,
	0x24, 0x0f,
	0x25, 0x00,
	0x26, 0x23,
	0x27, 0x70,
	0x28, 0x1f,
	0x29, 0x47,
	0x2a, 0x80,
	0x2b, 0x77,
	0x2c, 0x01,
	0x2d, 0x45,
	0x2e, 0x19,
	0x2f, 0x00,
	0x30, 0x01,
	0x31, 0x0f,
	0x33, 0x7f,
};

void radio_init(void)
{
	pin_ddr(RADIO_SDIO, 1);
	pin_ddr(RADIO_SCK, 1);
	pin_ddr(RADIO_SCS, 1);

	pin_write(RADIO_SCS, 1);

	// force a reset
	radio_reg_write(0,0);
	delay(200);

	// try writing to the id
	id[0] = 0x55;
	id[1] = 0xaa;
	id[2] = 0x01;
	id[3] = 0x10;
	radio_reg_write_buf(A7106_REG_ID, id, 4);
	radio_reg_read_buf(A7106_REG_ID, id+4, 4);

	// there is something weird in this value; not sure if docs are right
	radio_reg_write(A7106_REG_CHARGE_PUMP, 0
		| A7106_REG_CHARGE_PUMP_LVR // shall be set to 1
		| A7106_REG_CHARGE_PUMP_CPC1 // 11 == 2.0mA
		| A7106_REG_CHARGE_PUMP_CPC0
		| A7106_REG_CHARGE_PUMP_CELS // shall be set to 1
		| A7106_REG_CHARGE_PUMP_RGC1 // shall be set to 1
		| A7106_REG_CHARGE_PUMP_RGC0 // shall be set to 0, but is set?
	);

	// send all of our initial register states
	for(unsigned i = 0 ; i < sizeof(radio_init_cmd) ; i+=2)
		radio_reg_write(radio_init_cmd[i+0], radio_init_cmd[i+1]);

	// charge pump current register; normal value
	radio_reg_write(A7106_REG_CHARGE_PUMP, 0
		| A7106_REG_CHARGE_PUMP_LVR // shall be set to 1
		| A7106_REG_CHARGE_PUMP_CPC1 // 11 == 2.0mA
		| A7106_REG_CHARGE_PUMP_CPC0
		| A7106_REG_CHARGE_PUMP_CELS // shall be set to 1
		| A7106_REG_CHARGE_PUMP_RGC1 // shall be set to 1
	);

	if (!radio_calibrate()
	&&  !radio_calibrate()
	&&  !radio_calibrate())
	{
		// we tried.  we really tried.
		return;
	}

	delay(1000);

	// wake on radio is not yet configured, so it is not calibrated
	if (0)
	if (!radio_osc_setup())
		return;

	// radio is ready!
	radio_status = 0;

}
