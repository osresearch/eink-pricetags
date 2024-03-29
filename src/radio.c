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

#define RADIO_IO2	0x10
#define RADIO_IO1	0x11 // busy?
#define RADIO_SDIO	0x12
#define RADIO_SCS	0x13
#define RADIO_SCK	0x14
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

#define A7106_REG_MODE 0x00
#define A7106_REG_MODE_FECF BIT(6)
#define A7106_REG_MODE_CRCF BIT(5)
#define A7106_REG_MODE_CER BIT(4)
#define A7106_REG_MODE_XER BIT(3)
#define A7106_REG_MODE_PLLER BIT(2)
#define A7106_REG_MODE_TRSR BIT(1)
#define A7106_REG_MODE_TRER BIT(0)

#define A7106_REG_MODE_CONTROL 0x01
#define A7106_REG_MODE_CONTROL_ADCM BIT(0)
#define A7106_REG_MODE_CONTROL_FMS BIT(1)
#define A7106_REG_MODE_CONTROL_FMT BIT(2)
#define A7106_REG_MODE_CONTROL_WORE BIT(3)
#define A7106_REG_MODE_CONTROL_CD BIT(4)
#define A7106_REG_MODE_CONTROL_AIF BIT(5)
#define A7106_REG_MODE_CONTROL_ARSSI BIT(7)
#define A7106_REG_MODE_CONTROL_DDPC BIT(7)

// calibration control register
#define A7106_REG_CALC 0x02
#define A7106_REG_CALC_RSSC BIT(3)
#define A7106_REG_CALC_VCC BIT(2)
#define A7106_REG_CALC_VBC BIT(1)
#define A7106_REG_CALC_FBC BIT(0)

// RX and TX fifos
#define A7106_REG_FIFO_END 0x03
#define A7106_REG_FIFO2 0x04
#define A7106_REG_FIFO_DATA 0x05

#define A7106_REG_ID 0x06

#define A7106_REG_RCOSC1 0x07
#define A7106_REG_RCOSC2 0x08
#define A7106_REG_RCOSC3 0x09
#define A7106_REG_RCOSC3_CALW BIT(3)
#define A7106_REG_RCOSC3_EN BIT(2)
#define A7106_REG_RCOSC3_TSEL BIT(1)
#define A7106_REG_RCOSC3_WORMS BIT(0)

#define A7106_REG_DATA_RATE 0x0e
#define A7106_REG_PLL1 0x0F
#define A7106_REG_PLL2 0x10
#define A7106_REG_PLL3 0x11
#define A7106_REG_PLL4 0x12
#define A7106_REG_PLL5 0x13

#define A7106_REG_CODE1 0x1f
#define A7106_REG_CODE1_MCS BIT(6)
#define A7106_REG_CODE1_WHTS BIT(5)
#define A7106_REG_CODE1_FECS BIT(4)
#define A7106_REG_CODE1_CRCS BIT(3)
#define A7106_REG_CODE1_IDL BIT(2)
#define A7106_REG_CODE1_PML (BIT(1) | BIT(0))

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
struct {
	volatile uint16_t rx_count;
	volatile uint16_t rx_error;
	volatile uint16_t tx_count;
	volatile uint16_t tx_error;
} radio_stats;

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
	return spi_write(RADIO_SCK, 0, RADIO_IO1, 0);
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
	//pin_ddr(RADIO_SDIO, 1);
	radio_write_byte(cmd & ~RADIO_READ_BIT);

	for(unsigned i = 0 ; i < len ; i++)
		radio_write_byte(data[i]);

	radio_cs(0);
}

static void radio_reg_read_buf(const uint8_t cmd, uint8_t * data, const unsigned len)
{
	radio_cs(1);
	//pin_ddr(RADIO_SDIO, 1);
	radio_write_byte(cmd | RADIO_READ_BIT);

	// use the same pin for input
	//pin_ddr(RADIO_SDIO, 0);
	for(unsigned i = 0 ; i < len ; i++)
		data[i] = radio_read_byte();

	//pin_ddr(RADIO_SDIO, 1);
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

static uint8_t radio_sleeping;

void radio_sleep(void)
{
	radio_strobe(RADIO_CMD_SLEEP);
	radio_sleeping = 1;
}

void radio_wakeup(void)
{
	if (!radio_sleeping)
		return;

	radio_sleeping = 0;
	radio_strobe(RADIO_CMD_STBY);
	delay(1);
}

void radio_set_id(uint32_t id)
{
	uint8_t id_buf[] = {
		(id >> 24) & 0xFF,
		(id >> 16) & 0xFF,
		(id >>  8) & 0xFF,
		(id >>  0) & 0xFF,
	};

	radio_reg_write_buf(A7106_REG_ID, id_buf, sizeof(id_buf));

	// todo: check that it worked?
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
static int radio_calibrate(void)
{
 	// 3. Set A7106 in PLL mode.
	// radio_strobe(RADIO_CMD_PLL);

        // Calibration 
        radio_reg_write(0x22, 0x00); // Set MFBS = 0
        radio_reg_write(0x24, 0x00); // Set MCVS = 0
        radio_reg_write(0x25, 0x00); // Set MVBS = 0
	radio_strobe(RADIO_CMD_PLL);

	delay(400);

	// 4. Enable IF Filter Bank (set FBC, RSSC=1), VCO Current (VCC=1), and VCO Bank (VBC=1).
	radio_reg_write(A7106_REG_CALC, 0
		| A7106_REG_CALC_RSSC
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


static void radio_channel(const uint8_t value)
{
	radio_reg_write(A7106_REG_PLL1, value);
}

#define RADIO_SPEED_2 0xF9
#define RADIO_SPEED_10 0x31
#define RADIO_SPEED_25 0x09
#define RADIO_SPEED_100 0x04
#define RADIO_SPEED_125 0x03
#define RADIO_SPEED_250 0x01
#define RADIO_SPEED_500 0x00

static void radio_speed(uint8_t speed, uint8_t fec, uint8_t crc)
{
	// speeds are 2, 50, 125, 250 or 500 Kbps
	// default is 500 Kbps with a divisor of 1
	// 16MHz input clock, GCS=0, DBL=1, CSC=01, RRC=00, CHR=1111
	radio_reg_write(A7106_REG_DATA_RATE, speed);

	radio_reg_write(A7106_REG_CODE1, 0
		| (fec ? A7106_REG_CODE1_FECS : 0)
		| (crc ? A7106_REG_CODE1_CRCS : 0)
		| A7106_REG_CODE1_IDL
		| A7106_REG_CODE1_PML
	);
}

static void radio_fifo_reset(const uint8_t len)
{
	radio_reg_write(A7106_REG_FIFO_END, len - 1);
	radio_strobe(RADIO_CMD_WRITE_FIFO_RESET);
	//radio_strobe(RADIO_CMD_READ_FIFO_RESET);
}

/*
 * Port 1 pin 1 is mapped to the IO1 pin from the radio
 * This is controlled by the GIO1 register.
 * 0x0b, 0x01, // GIO1 pin control: TX End of access code / RX FSYNC
 *
 * In TX Mode, it goes high when transmitting and low when the packet is done.
 *
 * In RX mode, it outputs the FSYNC (Frame Sync) signal.
 * FSYNC goes high when the ID code has been matched, and then low
 * when the packet is complete.
 */

static uint8_t radio_busy(void)
{
	return pin_read(RADIO_IO2);
}


/* 16.4.1 Easy FIFO
 * In Easy FIFO, max FIFO length is 64 bytes.
 * FIFO length is equal to (FEP [7:0] +1).
 * User just needs to control FEP [7:0] (03h) and disable PSA and FPM as shown below.
 *
 * Procedures of TX FIFO Transmitting
 * 1. Initialize all control registers (refer to A7106 reference code).
 * 2. Set FEP [7:0] = 0x3F for 64-bytes FIFO.
 * 3. Refer to section 11.2 ~ 11.4.
 * 4. Send Strobe command – TX FIFO write pointer reset.
 * 5. MCU writes 64-bytes data to TX FIFO.
 * 6. Send TX Strobe Command.
 * 7. Done
 */
static int8_t radio_tx_buf(const uint8_t *buf, const unsigned len)
{
	radio_fifo_reset(len);
	radio_reg_write_buf(A7106_REG_FIFO_DATA, buf, len);

	//delay(500);
	radio_strobe(RADIO_CMD_TX);
	radio_stats.tx_count++;

	//delay(1000);

	delay(1);

	for(unsigned i = 0 ; i < 64 ; i++)
	{
		if (!radio_busy())
			return 0;
	}

	radio_stats.tx_error++;
	return -1;
}

// todo: document these registers and their bits
static const uint8_t radio_init_cmd[] = {
        0x00, 0x00, // Software reset
        0x01, 0x62, // 0b01100010, // Enable auto RSSI measurement, disable RF IF shift, NOTE: AIF inverted
        0x02, 0x00, // Set during calibration
        0x03, 0x3F, // Set maximum size fo RX mode
        0x04, 0x00, // Clear FPM and PSA to use FIFO simple mode
        // 0x05, 0x00, // FIFO data, not needed for setup
        // 0x06, 0x00, // ID Data, set later.
        0x07, 0x00,
        0x08, 0x00, // Wake on radio disabled
        0x09, 0x00, // Wake on radio disabled
        // 0x0A, 0b10100010, // Configure CKO pin to output Fsysck
        0x0A, 0x00, // Configure CKO pin to off
        0x0B, 0x19, // Configure GIO1 pin for 4-wire SPI mode
        0x0C, 0x01, // Configure GIO2 pin to output WTR
        0x0D, 0x05, // Configure for 16MHz crystal, 500Kpbs data rate
        0x0E, 0x00, // Configure for 16MHz crystal, 500Kbps data rate
        0x0F, 0x00, // TODO: pass channel into config?
        0x10, 0x9e, //  Configure for 16MHz crystal, 500kbps data rate
        0x11, 0x4B, //recommended
        0x12, 0x00, // Configure for 16 MHz crystal, 500kbps data rate
        0x13, 0x02, // Configure for 16 MHz crystal, 500kbps data rate 
        0x14, 0x16, // Recommended, disable TXSM moving average, disable TX modulation, disable filter
        0x15, 0x2b, // TODO
        0x16, 0x12, // Recommended,
        0x17, 0x4f, // Recommended, XTAL delay 600us, AGC delay 20us, RSSI delay 80us
        0x18, 0x63, // TODO: BWS=1
        0x19, 0x80, // Recommended, set pixer gain to 24dB, LNA gain to 24dB
        0x1A, 0x80, // TODO: Why (copied from sample code)
        0x1B, 0x00, // TODO: Why (copied from sample code)
        0x1C, 0x0a, // Recommended
        0x1D, 0x32, // TODO: Why (copied from sample code)
        0x1E, 0xc3, // Recommended TODO: this enables RSSI, what is the effect?
        0x1F, 0x1f, // Disable whitening, enable FEC, CRC, id=4 bytes, preamble=4 bytes TODO: what is MCS
        0x20, 0x16, // PMD = 10 for 250/500 kbps signal rate
        0x21, 0x00, // Data whitening disabled
        0x22, 0x00, // Recommeded TODO: should MFB still be set?
        // 0x23, 0x00, // Read only
        0x24, 0x13, // Recommended
        // 0x25, 0x00,
        0x26, 0x23, // TODO: Why (copied from sample code)
        0x27, 0x00,
        0x28, 0x17, // TX output power: 0dBm TODO: verify me
        0x29, 0x47, // Reserved, DCM=10 for 250/500 Kbps
        0x2A, 0x80, // Recommended
        0x2B, 0xD6, // Reserved, Recommended
        0x2C, 0x01, // Reserved
        0x2D, 0x51, // Reserved, TODO: PRS
        0x2E, 0x18, // Reserved
        0x2F, 0x00, // Recommended
        0x30, 0x01, // Reserved
        0x31, 0x0F, // Reserved
        0x32, 0x00, // Reserved
        0x32, 0x7F, // Max ramping
};


void radio_init(uint8_t channel)
{
	pin_ddr(RADIO_IO1, 0);
	pin_ddr(RADIO_IO2, 0);
	pin_ddr(RADIO_SDIO, 1);
	pin_ddr(RADIO_SCK, 1);
	pin_ddr(RADIO_SCS, 1);

	pin_write(RADIO_SCS, 1);

	// send all of our initial register states
	// first is a reset
	for(unsigned i = 0 ; i < sizeof(radio_init_cmd) ; i+=2)
		radio_reg_write(radio_init_cmd[i+0], radio_init_cmd[i+1]);

	if (!radio_calibrate()
	&&  !radio_calibrate()
	&&  !radio_calibrate())
	{
		// we tried.  we really tried.
		return;
	}

	delay(100);

	// wake on radio is not yet configured, so it is not calibrated
	if (0)
	if (!radio_osc_setup())
		return;

	// high speed, FEC + CRC
	if (0)
		radio_speed(RADIO_SPEED_500, 1, 1);
	//radio_reg_write(A7106_REG_FIFO2, 0x00); // FPM=0, PSA=0

	// frequency = 2400 MHz + channel * 500 KHz
	// looks like maybe (G?)FSK at ~350 KHz separation
	radio_channel(channel);

	return;
}

int8_t radio_tx(uint32_t id, const uint8_t * buf, uint8_t len)
{
	radio_wakeup();
	radio_set_id(id);
	return radio_tx_buf(buf, len);
}

int8_t radio_rx(uint32_t id, uint8_t * buf, uint8_t max_len, uint16_t timeout)
{
	radio_wakeup();
	radio_set_id(id);

	radio_strobe(RADIO_CMD_RX);
	//delay(1);

	// wait for WTR to go low, indicating rx complete
	for(uint16_t spin = 0 ; radio_busy() ; spin++)
	{
		if (spin < timeout)
			continue;

		// cancel the RX
		radio_strobe(RADIO_CMD_STBY);
		return 0;
	}

	// check the CRC and FEC registers
	const uint8_t status = radio_reg_read(A7106_REG_MODE);
	if (status & (A7106_REG_MODE_CRCF | A7106_REG_MODE_FECF))
	{
		radio_stats.rx_error++;
		return -1;
	}

	// read in the message to our buffer
	radio_fifo_reset(max_len);
	radio_reg_read_buf(A7106_REG_FIFO_DATA, buf, max_len);
	radio_stats.rx_count++;
	return 1;
}
