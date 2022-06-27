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
 * ID code is matched based on 
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


static void radio_write(uint8_t byte)
{
	pin_ddr(RADIO_SDIO, 1);
	spi_write(RADIO_SCK, RADIO_SDIO, 0, byte);
}

static uint8_t radio_read(void)
{
	pin_ddr(RADIO_SDIO, 0);
	return spi_write(RADIO_SCK, 0, RADIO_SDIO, 0);
}

static void radio_cs(uint8_t selected)
{
	pin_write(RADIO_SCS, !selected);
}

static void radio_strobe(const uint8_t cmd)
{
	radio_cs(1);
	radio_write(cmd);
	radio_cs(0);
}

static void radio_reg_write_buf(const uint8_t cmd, const uint8_t * data, const unsigned len)
{
	// should confirm that cmd doesn't have bit 7 set
	radio_cs(1);
	radio_write(cmd & ~RADIO_READ_BIT);
	for(unsigned i = 0 ; i < len ; i++)
		radio_write(data[i]);
	radio_cs(0);
}

static void radio_reg_read_buf(const uint8_t cmd, uint8_t * data, const unsigned len)
{
	// should confirm that cmd doesn't have bit 7 set
	radio_cs(1);
	radio_write(cmd | RADIO_READ_BIT);
	for(unsigned i = 0 ; i < len ; i++)
		data[i] = radio_read();
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

void radio_calibrate(void)
{
 	// 3. Set A7106 in PLL mode.
	radio_strobe(RADIO_CMD_PLL);

	// 4. Enable IF Filter Bank (set FBC, RSSC=1), VCO Current (VCC=1), and VCO Bank (VBC=1).
	radio_reg_write(A7106_REG_CALC, 0
		| A7106_REG_CALC_RSSC
		| A7106_REG_CALC_VCC
		| A7106_REG_CALC_VBC
		| A7106_REG_CALC_FBC
	);

	// 5. After calibration done, FBC, VCC and VBC is auto clear.
	// todo: timeout if this fails
	while(radio_reg_read(A7106_REG_CALC) != 0)
		spins++;

	// 6. Check pass or fail by reading calibration flag (FBCF) and (VCCF, VBCF).
	if (radio_reg_read(A7106_REG_IF_CALIBRATION1) & A7106_REG_IF_CALIBRATION1_FBCF)
	{
		spins = 0x999;
		//while(1)
			//;
	}

	if (radio_reg_read(A7106_REG_VCO_CURRENT_CALIBRATION) & A7106_REG_VCO_CURRENT_CALIBRATION_VCCF)
	{
		spins = 0x888;
		while(1)
			;
	}

	if (radio_reg_read(A7106_REG_VCO_BAND_CALIBRATION) & A7106_REG_VCO_BAND_CALIBRATION_VBCF)
	{
		spins = 0x777;
		while(1)
			;
	}
}

uint8_t id[9];

void radio_tx_buf(const uint8_t *buf, const unsigned len)
{
	radio_reg_write_buf(A7106_REG_FIFO, buf, len);
	radio_strobe(RADIO_CMD_TX);

	delay(10);
}

void radio_init(void)
{
	pin_ddr(RADIO_SDIO, 1);
	pin_ddr(RADIO_SCK, 1);
	pin_ddr(RADIO_SCS, 1);

	pin_write(RADIO_SCS, 1);

	radio_strobe(RADIO_CMD_IDLE);

	memset(id, 0xAF, sizeof(id));
	id[7] = radio_reg_read(0x0A);

	static const uint8_t buf[] = { 0x55, 0xAB, 0x12, 0x13 };
	radio_reg_write_buf(A7106_REG_ID, buf, sizeof(buf));
	radio_reg_read_buf(A7106_REG_ID, id, 4);

	//return;

	radio_calibrate();

	// try transmitting?
	uint8_t msg[12];
	msg[0] = 0xAA;
	msg[1] = 0xAA;
	msg[2] = 0xAA;
	msg[3] = 0xAA;
	msg[4] = 0xA0;
	msg[5] = 0x0A;
	msg[6] = 0x5A;
	msg[7] = 0xA5;
	msg[8] = 0x00;
	msg[9] = 0x00;
	msg[10] = 0x00;
	msg[11] = 0x00;

	while(1)
	{
		radio_strobe(RADIO_CMD_IDLE);

		radio_tx_buf(msg, 64);
		msg[11]++;
		radio_tx_buf(msg, 64);
		msg[11]++;
		radio_tx_buf(msg, 64);
		msg[11]++;

		delay(20);
		radio_strobe(RADIO_CMD_SLEEP);
		delay(50);
	}
}
