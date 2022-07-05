import a7106
import struct
import time
import hashlib
from datetime import datetime

def now():
    return datetime.now().strftime("%Y%M%d-%H%M%S")

class eink_server:
    def __init__(self, gateway_id=0x59abcdef, channel=4):
        self.radio = a7106.A7106(channel=channel, id=gateway_id, packet_len=40)

        self.gateway_id = gateway_id
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

                if not client_id in clients:
                    clients[client_id] = {
                        'rx_count': 0,
                        'img_id': img_id,
		    }
                    print(now(), "%08x: New client type %08x hash %08x" % (client_id, tag_type, githash))
                clients[client_id]['rx_count'] += 1

                offset = None
                if img_id != self.img_id:
                    # they have a different image
                    print(now(), '%08x: old image!' % (client_id))
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
                    print(now(), '%08x: image %08x complete' % (client_id, img_id))
                    continue

                #print('%08x: sending %d' % (client_id, offset))

                self.radio.set_id(client_id)
                self.radio.transmit(
                  struct.pack("<IHH", self.img_id, offset, 0) + self.image[offset:offset+32])

                print(now(), '%08x: image %08x offset %d' % (client_id, img_id, offset))

            #except a7106.RxError as e:
            except Exception as e:
                print(now(), e)

server = eink_server()

with open("hello.raw", "rb") as f:
    server.image = f.read()
server.img_id = int(hashlib.sha256(server.image).digest()[0:4].hex(), 16)
print(now(), 'image id %08x' %(server.img_id))

server.serve()
