#!/usr/bin/env python3
from PIL import Image, ImageFont, ImageDraw, ImageOps
from datetime import datetime
import argparse
import subprocess
import autocor
import sys

height = 250
width = 128  # actual 122, but padded

# 1 bit per pixel for the output
img = Image.new("1", (width,height))
#font_big = ImageFont.truetype("fonts/IBMPlexMono-Bold.ttf", 40)
font_big = ImageFont.truetype("fonts/Anonymous Pro B.ttf", 38)
font_small = ImageFont.truetype("fonts/Anonymous Pro.ttf", 14)

def draw_text(img,x,y,msg,font):
	text_w, text_h = font.getsize(msg)
	text_mask = Image.new("1", (text_w,text_h))

	draw = ImageDraw.Draw(text_mask)
	draw.text((0,0), msg, font=font, fill=1)
	img.paste(1, (x,y), text_mask.rotate(-90, expand=True))


now = datetime.now()
now_str = now.strftime("%Y%m%d-%H%M")
now_sec = now.strftime("%s")
tag_type = 0x02500120
channel = 4
gw = "aed8e4fd"
mac = "%08x" % (autocor.mac(threshold=18))
extra = subprocess.run(["git","rev-parse","--short","HEAD"], capture_output=True).stdout.decode('utf-8').rstrip()

draw_text(img, 0, 0, now_str + " #" + extra + " %dx%d" % (height,width), font=font_small)
draw_text(img, 25, 40, gw, font=font_big)
draw_text(img, 55, 0, " gateway chan %d" % (channel), font=font_small)
draw_text(img, 80, 40, mac, font=font_big)
draw_text(img, 110, 0, " address", font=font_small)

def rle(b):
	from bitstring import BitArray
	bits = BitArray(b)
	last = bits[0]
	count = 1
	out = []

	for b in bits[1:]:
		if b == last and count != 127:
			count += 1
			continue
		out.append(last << 7 | count)
		last = b
		count = 1
	out.append(last << 7 | count)
	return out

with open("provision.png", "wb") as f:
	img.rotate(90,expand=True).save(f, format="png")

with sys.stdout as f:
	rle_data = rle(ImageOps.flip(img).tobytes())
	print("const uint8_t channel = %d;" % (channel), file=f)
	print("const uint32_t tag_type = 0x%08x;" % (tag_type), file=f)
	print("const uint32_t gateway = 0x" + gw + ";", file=f)
	print("const uint32_t macaddr = 0x" + mac + ";", file=f)
	print("const uint32_t githash = 0x" + extra + ";", file=f)
	print("const uint32_t install_date = " + now_sec + ";", file=f)
	print("const uint16_t bootscreen_len = %d;" % (len(rle_data)), file=f)

	print("const uint8_t bootscreen[] = {", file=f)
	for b in rle_data:
		print("0x%02x," % (b ^ 0x80), end="", file=f)
	print("};", file=f)

