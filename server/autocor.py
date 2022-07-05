#!/usr/bin/env python3
import secrets
import sys

def bitcount(x):
	count = 0
	while x != 0:
		count += (x & 1)
		x >>= 1
	return count

def autocor(x):
	vals = []

	for shift in range(-31,32):
		if shift < 0:
			xor = ((x >> -shift) ^ x) & 0xFFFFFFFF
		else:
			xor = ((x << shift) ^ x) & 0xFFFFFFFF

		#print("%d %d" % (shift, bitcount(xor)))
		vals.append(32 - bitcount(xor))

	return vals


threshold = 17

if len(sys.argv) > 1:
	for x_arg in sys.argv[1:]:
		x = int(x_arg,16)
		vals = autocor(x)
		print("%08x" % (x), vals)
	sys.exit(0)

while True:
	x = (0x5 << 28) | secrets.randbits(28)
	vals = autocor(x)

	# check for goodness
	fails = 0
	for v in vals:
		if v > threshold:
			fails += 1
	if fails > 1:
		continue

	print("%08x" % (x), vals)
	break
