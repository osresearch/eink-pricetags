import a7106
import struct
import time
import hashlib
import os
from PIL import Image, ImageFont, ImageDraw, ImageOps
from threading import Thread
from datetime import datetime

def now():
    return datetime.now().strftime("%Y%M%d-%H%M%S")

class eink_server:
    def __init__(self, gateway_id=0xaed8e4fd, channel=4):
        self.radio = a7106.A7106(channel=channel, id=gateway_id, packet_len=40)

        self.gateway_id = gateway_id
        self.img_id = 0
        #self.radio.set_id(self.gateway_id)

    def serve(self):
        clients = {}

        while True:
            try:
                self.radio.set_id(self.gateway_id)
                data = self.radio.blocking_receive()

		# received the hello message
                #print('got packet, data_length={} data={}'.format(len(data), data))

                [tag_type,client_id,githash,install_date,reserved,img_id] = struct.unpack('<IIIIII',data[0:24])
                img_map = data[24:]

                new = False
                if not client_id in clients:
                    clients[client_id] = {
                        'rx_count': 0,
                        'img_id': img_id,
		    }
                    new = True
                clients[client_id]['rx_count'] += 1

                offset = None
                flags = 0
                if img_id != self.img_id:
                    # they have a different image
                    #print(now(), '%08x: old image!' % (client_id))
                    offset = 0
                else:
                    # find the first not present packet in the image
                    # 250 * 128 = 32000 bits / (32 * 8 bits per packet) = 125 packets
                    for i in range(0,126):
                       b = img_map[i // 8]
                       mask = 1 << (i % 8)
                       if b & mask != 0:
                          offset = 32 * i
                          break

                if offset is None:
                    #print(now(), '%08x: image %08x complete' % (client_id, img_id))
                    offset = 0
                    flags = 1 # go back to sleep

                #print('%08x: sending %d' % (client_id, offset))

                self.radio.set_id(client_id)
                self.radio.transmit(
                  struct.pack("<IHH", self.img_id, offset, flags) + self.image[offset:offset+32])

                if new:
                    print(now(), "%08x: New client type %08x hash %08x" % (client_id, tag_type, githash))
                if (flags & 1) == 0:
                    print(now(), '%08x: %08x offset %d' % (client_id, img_id, offset))
                else:
                    print(now(), '%08x: %08x complete' % (client_id, img_id))

            #except a7106.RxError as e:
            except Exception as e:
                print(now(), e)

def monitor_files(server, filename):
	last_mtime = 0
	while True:
		try:
			st = os.stat(filename)
			if st.st_mtime == last_mtime:
				time.sleep(1)
				continue
			last_mtime = st.st_mtime

			img = Image.open(filename)

			# rotate it clockwise if the wrong orientation
			if img.width > img.height:
				img = img.rotate(-90, expand=True)

			# shrink it to fit
			if img.width != 128 or img.height != 250:
				img = img.resize((128,250))

			# convert it to 1 channel
			if img.mode != "1":
				img = img.convert(mode="1") #, dither=Image.Dither.FLOYDSTEINBERG)

			# convert it to raw bytes
			img = ImageOps.flip(img).tobytes()
			img_id = int(hashlib.sha256(img).digest()[0:4].hex(), 16)
			if server.img_id == img_id:
				time.sleep(1)
				continue
			server.image = img
			server.img_id = img_id
			print(now(), 'image id %08x' %(server.img_id))
		except Exception as e:
			print(now(), e)
			time.sleep(5)

server = eink_server()
fs_thread = Thread(target=monitor_files, args=(server,"hello.png"))

fs_thread.start()
server.serve()
