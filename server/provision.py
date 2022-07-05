#!/usr/bin/env python3
from PIL import Image, ImageFont, ImageDraw, ImageOps
from datetime import datetime
import argparse
import subprocess

height = 250
width = 128  # actual 122, but padded

# 1 bit per pixel for the output
img = Image.new("1", (width,height))
font_big = ImageFont.truetype("fonts/IBMPlexMono-Bold.ttf", 48)
font_small = ImageFont.truetype("fonts/IBMPlexMono-Medium.ttf", 16)

def draw_text(img,x,y,msg,font):
	text_w, text_h = font.getsize(msg)
	text_mask = Image.new("1", (text_w,text_h))

	draw = ImageDraw.Draw(text_mask)
	draw.text((0,0), msg, font=font, fill=1)
	img.paste(1, (x,y), text_mask.rotate(-90, expand=True))


now = datetime.now().strftime("%Y%m%d-%H%M")
gw = "59abcdef"
mac = "50123456"
extra = subprocess.run(["git","rev-parse","--short","HEAD"], capture_output=True).stdout.decode('utf-8')

draw_text(img, 0, 0, now + " #" + extra, font=font_small)
draw_text(img, 17, 15, gw, font=font_big)
draw_text(img, 55, 0, "gateway", font=font_small)
draw_text(img, 70, 15, mac, font=font_big)
draw_text(img, 110, 0, "address", font=font_small)

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

with open("provision.h", "w") as f:
	rle_data = rle(ImageOps.flip(img).tobytes())
	print("const uint8_t bootscreen[] = {", file=f)
	for b in rle_data:
		print("0x%02x," % (b ^ 0x80), end="", file=f)
	print("};", file=f)
	print("const uint16_t bootscreen_len = %d;" % (len(rle_data)), file=f)
	#f.write(rle(img.tobytes()))

with open("provision.png", "wb") as f:
	img.rotate(90,expand=True).save(f, format="png")
