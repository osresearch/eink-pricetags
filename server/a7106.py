#!/usr/bin/python3
import RPi.GPIO as GPIO
import csv
import struct
import time

def load_csv_regs(filename):
    regs = []

    with open(filename) as csvfile:
        reader = csv.DictReader(csvfile)

        for row in reader:
            reg = {}
            reg['address'] = int(row['address'])
            reg['name'] = row['name']
            reg['read'] = [row['read{}'.format(i)] for i in range(7,-1, -1)]
            reg['write'] = [row['write{}'.format(i)] for i in range(7,-1, -1)]

            regs.append(reg)

    return regs

class RxError(Exception):
    pass

class A7106:
    pins = {
            'cs':2,     # CS
            'ck':3,     # SCK
            'da':4,     # MOSI
            'io1':17,   # MISO (after configuration)
            'io2':27,   # WTR (signals that a packet was received)
            }

    def __init__(self, id=0x930B51DE, channel=0, packet_len=64, pins={}):
        """ Initialize the A7106 radio """
        if pins != {}:
            self.pins = pins

        GPIO.setmode(GPIO.BCM)
        
        GPIO.setwarnings(False) 
        GPIO.setup(self.pins['cs'], GPIO.OUT, initial = GPIO.HIGH)
        GPIO.setup(self.pins['ck'], GPIO.OUT, initial = GPIO.LOW)
        GPIO.setup(self.pins['da'], GPIO.OUT, initial = GPIO.LOW)
        GPIO.setup(self.pins['io1'], GPIO.IN)
        GPIO.setup(self.pins['io2'], GPIO.IN)
        GPIO.setwarnings(True)

        self.regs = load_csv_regs('a7106_registers.csv')

        self.setup()
        self.set_channel(channel)
        self.set_id(id)
        self.set_packet_length(64)

    def setup(self):
        # These settings were derived from the recommended values in the datasheet, except where noted.
        self.write_reg(0x00, 0b0)       # Software reset
        self.write_reg(0x01, 0b01100010) # Enable auto RSSI measurement, disable RF IF shift, NOTE: AIF inverted
        self.write_reg(0x02, 0x00) # Set during calibration
        self.write_reg(0x03, 0x3F) # Set maximum size fo RX mode
        self.write_reg(0x04, 0b00000000)  # Clear FPM and PSA to use FIFO simple mode
        #self.write_reg(0x05, 0x00) # FIFO data, not needed for setup
        #self.write_reg(0x06, 0x00) # ID Data, set later.
        self.write_reg(0x07, 0x00) 
        self.write_reg(0x08, 0x00) # Wake on radio disabled
        self.write_reg(0x09, 0x00) # Wake on radio disabled
        #self.write_reg(0x0A, 0b10100010) # Configure CKO pin to output Fsysck
        self.write_reg(0x0A, 0b00000000) # Configure CKO pin to off
        self.write_reg(0x0B, 0b00011001) # Configure GIO1 pin for 4-wire SPI mode
        self.write_reg(0x0C, 0b00000001) # Configure GIO2 pin to output WTR
        self.write_reg(0x0D, 0b00000101) # Configure for 16MHz crystal, 500Kpbs data rate
        self.write_reg(0x0E, 0b00000000) # Configure for 16MHz crystal, 500Kbps data rate
        self.write_reg(0x0F, 0x00) # TODO: pass channel into config?
        self.write_reg(0x10, 0b10011110) #  Configure for 16MHz crystal, 500kbps data rate
        self.write_reg(0x11, 0x4B) #recommended
        self.write_reg(0x12, 0x00) # Configure for 16 MHz crystal, 500kbps data rate
        self.write_reg(0x13, 0x02) # Configure for 16 MHz crystal, 500kbps data rate 
        self.write_reg(0x14, 0b00010110) # Recommended, disable TXSM moving average, disable TX modulation, disable filter
        self.write_reg(0x15, 0b00101011) # TODO
        self.write_reg(0x16, 0b00010010) # Recommended,
        self.write_reg(0x17, 0b01001111) # Recommended, XTAL delay 600us, AGC delay 20us, RSSI delay 80us
        self.write_reg(0x18, 0b01100011) # TODO: BWS=1
        self.write_reg(0x19, 0b10000000) # Recommended, set pixer gain to 24dB, LNA gain to 24dB
        self.write_reg(0x1A, 0x80) # TODO: Why (copied from sample code)
        self.write_reg(0x1B, 0x00) # TODO: Why (copied from sample code)
        self.write_reg(0x1C, 0b00001010) # Recommended
        self.write_reg(0x1D, 0x32) # TODO: Why (copied from sample code)
        self.write_reg(0x1E, 0b11000011) # Recommended TODO: this enables RSSI, what is the effect?
        self.write_reg(0x1F, 0b00011111) # Disable whitening, enable FEC, CRC, id=4 bytes, preamble=4 bytes TODO: what is MCS
        self.write_reg(0x20, 0b00010110) # PMD = 10 for 250/500 kbps signal rate
        self.write_reg(0x21, 0x00) # Data whitening disabled
        self.write_reg(0x22, 0b00000000) # Recommeded TODO: should MFB still be set?
        #self.write_reg(0x23, 0x00) # Read only
        self.write_reg(0x24, 0b00010011) # Recommended
        #self.write_reg(0x25, 0x00)
        self.write_reg(0x26, 0x23) # TODO: Why (copied from sample code)
        self.write_reg(0x27, 0x00)
	# TX power setting : TXCS=0 PAC=2 TBG=7 => 0.08 dBm 19mA
        #self.write_reg(0x28, 0b00010111) # TX output power
	# TX power setting : TXCS=0 TBG=7 PAC=3 => 1.35 dBm 22mA
        self.write_reg(0x28, 0b00110111) # TX output power
        self.write_reg(0x29, 0b01000111) # Reserved, DCM=10 for 250/500 Kbps
        self.write_reg(0x2A, 0x80) # Recommended
        self.write_reg(0x2B, 0b11010110) # Reserved, Recommended
        self.write_reg(0x2C, 0b00000001) # Reserved
        self.write_reg(0x2D, 0b01010001) # Reserved, TODO: PRS
        self.write_reg(0x2E, 0b00011000) # Reserved
        self.write_reg(0x2F, 0b00000000) # Recommended
        self.write_reg(0x30, 0b00000001) # Reserved
        self.write_reg(0x31, 0x0F) # Reserved
        self.write_reg(0x32, 0x00) # Reserved
        self.write_reg(0x32, 0b01111111) # Max ramping

        # Calibration 
        self.write_reg(0x22, 0b00000000) # Set MFBS = 0
        self.write_reg(0x24, 0b00000000) # Set MCVS = 0
        self.write_reg(0x25, 0b00000000) # Set MVBS = 0
        self.strobe(0b1011) # Enter PLL mode
        self.write_reg(0x02, 0b00001111) # Enable IF Filter Bank
        time.sleep(0.5) # Wait for calibration to finish

        ccr = ord(self.read_reg(0x02))
        if ccr & 0b00001111:
            print('Error: calibrations failed to finish, CCR:0b{:08b} expected:0bxxxx0000'.format(ccr))

        if_cal = ord(self.read_reg(0x22))
        if if_cal & 0b00010000:
            print('Error: IF filter auto calibration failed. if_cal:0b{:08b} expected:0xxx0xxxx'.format(if_cal))
#        else:
#            print('IF filter calibrated, FB:0b{:04b} FCD:{:05b}'.format(
#                if_cal & 0b00001111,
#                ord(self.read_reg(0x23)) & 0xb00011111,
#                ))

        vco_current_cal = ord(self.read_reg(0x24))
        if vco_current_cal & 0b00010000:
            print('Error: VCO current calibration failed. vco_cal:0b{:08b} expected:0bxxxx0xxx'.format(vco_current_cal))
#        else:
#            print('VCO current calibrated, VCB:0b{:04b}'.format(vco_current_cal & 0b00001111))

        vco_cal = ord(self.read_reg(0x25))
        if vco_cal & 0b00001000:
            print('Error: VCO single bank calibration failed. vco_cal:0b{:08b} expected:0bxxxx0xxx'.format(vco_cal))
#        else:
#            print('VCO bank calibrated, VB:0b{:03b}'.format(vco_cal & 0b00000111))

    def txrx(self, data, rx_len=0):
        #print("TX: 0x%02x, 0x%s" % (int(data[0]), data[1:].hex()))
        #GPIO.setup(self.pins['da'], GPIO.OUT)

        GPIO.output(self.pins['cs'], GPIO.LOW)
        for d in data:
            for b in '{:08b}'.format(d):
                GPIO.output(self.pins['ck'], GPIO.LOW)

                if b=='0':
                    GPIO.output(self.pins['da'], GPIO.LOW)
                else:
                    GPIO.output(self.pins['da'], GPIO.HIGH)
                GPIO.output(self.pins['ck'], GPIO.HIGH)

        #GPIO.setup(self.pins['da'], GPIO.IN)
        GPIO.output(self.pins['ck'], GPIO.LOW)

        rx_data = bytearray()
        for i in range(0, rx_len):
            din = 0;
            for b in range(0,8):
                GPIO.output(self.pins['ck'], GPIO.HIGH)
                din = din << 1

                #if GPIO.input(self.pins['da']) == GPIO.HIGH:
                if GPIO.input(self.pins['io1']) == GPIO.HIGH:
                    din = din | 1
                GPIO.output(self.pins['ck'], GPIO.LOW)
            rx_data.append(din)    

        GPIO.output(self.pins['cs'], GPIO.HIGH)
        return rx_data

    def write_reg(self, address, data):
        """ Write a value into a register. Accepts an integer from 0-255 for single byte writes, or a byte array for multiple byte writes """
        if type(data) == int:
            if (data > 255) or (data < 0):
                raise Exception('Data value out of range, got:{} min:0 max:255'.format(data))
            data = bytes([data])

        d = bytearray()
        d.append(address)
        d.extend(data)
        self.txrx(d)

    def read_reg(self, address, len=1):
        d = bytearray()
        d.append(address | (1<<6))
        return self.txrx(d,len)

    def strobe(self, command):
        """ Send the 4-bit strobe command """
        d = bytearray()
        d.append(command<<4)
        return self.txrx(d)


    def dump_regs(self):
        for reg in self.regs:
            val = ord(self.read_reg(reg['address']))

            bits = list('{:08b}'.format(val))
            vals = ', '.join(['{:}:{:}'.format(name,val) for name, val in zip(reg['read'], bits) if name != '' and name != '--'])

            print('0x{:02x} ({:}): 0x{:02x}  [{:}]'.format(
                reg['address'],
                reg['name'],
                val,
                vals
                ))

    def set_channel(self, channel):
        """ Set the RF channel. Frequency = (2400.001 + 0.5*channel)MHz """
        self.write_reg(0x0F, channel)

    def set_id(self, id):
        """ Set the radio id, where ID is a 32-bit number """

        val = struct.pack('>I',id)
        self.write_reg(0x06, val)
        
        v = struct.unpack('>I', self.read_reg(0x06,len=4))[0]
        
        if(v != id):
            raise Exception('failed to set id, read:0x{:08x} expected:0x{:08x}'.format(v,id))
        else:
            print("id validated", val.hex())

    def set_packet_length(self, packet_length):
        if (packet_length > 64) or (packet_length < 1):
            raise Exception('Packet length out of range, got:{} min:1 max:64'.format(packet_length))

        self.packet_length = packet_length
        self.write_reg(0x03, packet_length-1)

    def transmit(self, data):
        """ Transmit a data packet """
        if len(data) > self.packet_length:
            raise Exception('packet data too long, length:{} maximum:{}'.format(len(data),self.packet_length))

        payload = bytearray()
        payload.extend(data)
        payload.extend(bytes(self.packet_length-len(payload)))

        self.strobe(0b1110) # Fifo write pointer reset
        self.write_reg(0x05, payload) # Write packet to FIFO
        self.strobe(0b1101) # TX
        while GPIO.input(self.pins['io2']) == GPIO.HIGH:
            pass

    def blocking_receive(self):
        """ Wait for one packet to be recevied """
        self.strobe(0b1100) # RX
        while GPIO.input(self.pins['io2']) == GPIO.HIGH:
            pass

        mode_reg = ord(self.read_reg(0x00))
        if mode_reg & 0b00100000:
            raise RxError('CRC error on receive')
        if mode_reg & 0b01000000:
            raise RxError('FEC error on receive')

        self.strobe(0b1111) # RX FIFO read reset

        return self.read_reg(0x05, len=self.packet_length)

if __name__ == "__main__":
    import argparse

    def tx_example():
        from datetime import datetime
    
        pins = {
                'cs':10,
                'ck':9,
                'da':11,
                'io1':5,
                'io2':6,
                }
        r = A7106(pins=pins)
        
        while True:
            data = datetime.now().isoformat().encode('utf-8')
            payload = bytearray()
            payload.append(len(data))
            payload.extend(data)
            print('sending packet, data_length={} data={}'.format(len(data),data))
            r.transmit(payload)
        
            time.sleep(0.5)
    
    def rx_example():
        r = A7106(channel=4, id=0x55abcdef )
        
        while True:
            try:
                payload = r.blocking_receive()
                l = int(payload[0])
                data = payload[1:(l+1)]
                print('got packet, data_length={} data={}'.format(len(data), data))
            except RxError as e:
                print(e)



    parser = argparse.ArgumentParser(description='A7106 radio demonstrator')
    parser.add_argument('--mode', help='Radio mode: tx or rx')

    args = parser.parse_args()
    if args.mode == 'tx':
        tx_example()
    elif args.mode == 'rx':
        rx_example()
    else:
        print('invalid mode, exiting')

