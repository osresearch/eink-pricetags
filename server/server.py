import a7106
import struct
import time
import hashlib

class eink_server:
    def __init__(self, gateway_id=0x55abcdef, channel=4):
        self.radio = a7106.A7106(channel=channel, id=gateway_id, packet_len=40)

        self.gateway_id = gateway_id
        #self.radio.set_id(self.gateway_id)

    def serve(self):
        clients = {}

        while True:
            try:
                self.radio.set_id(self.gateway_id)
                payload = self.radio.blocking_receive()

                l = int(payload[0])
                data = payload[1:(l+1)]
                #print('got packet, data_length={} data={}'.format(len(data), data))

                [client_id,img_id,offset] = struct.unpack('<IIH',data[0:10])

                if not client_id in clients:
                    clients[client_id] = {
                        'rx_count': 0,
                        'img_id': img_id,
                        'offset': offset,
		    }
                clients[client_id]['rx_count'] += 1

                if img_id != self.img_id:
                    # they have a different image
                    print('%08x: old image!' % (client_id))
                    offset = 0
                if offset >= len(self.image):
                    print('%08x: image %08x complete' % (client_id, img_id))
                    continue

                #print('%08x: sending %d' % (client_id, offset))

                self.radio.set_id(client_id)
                self.radio.transmit(
                  struct.pack("<IH", self.img_id, offset) + self.image[offset:offset+32])

                print('%08x: image %08x offset %d' % (client_id, img_id, offset))

            #except a7106.RxError as e:
            except Exception as e:
                print(e)

server = eink_server()

with open("hello.raw", "rb") as f:
    server.image = f.read()
server.img_id = int(hashlib.sha256(server.image).digest()[0:4].hex(), 16)
print('image id %08x' %(server.img_id))

server.serve()
