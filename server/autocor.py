#!/usr/bin/env python3
import secrets

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

def mac(prefix=0x5, threshold=17, debug=False):
	while True:
		x = (prefix << 28) | secrets.randbits(28)
		vals = autocor(x)

		# check for goodness
		fails = 0
		almost_fails = 0
		avg = 0
		for v in vals:
			avg += v
			if v > threshold:
				fails += 1
			elif v >= threshold:
				almost_fails += 1
		if fails > 1 or almost_fails > 2:
			continue

		if not debug:
			return x

		avg = 0
		for v in vals:
			avg += v
		print("%08x %.1f" % (x, avg / len(vals)), vals)
#		print("%08x %.1f" % (x, avg / len(vals)))
#		for x in range(0,32):
#			for v in vals:
#				if v < 32 - x:
#					print(" ", end="")
#				else:
#					print("#", end="")
#			print()
		return x

if __name__ == "__main__":
	import sys

	if len(sys.argv) == 1:
		while True:
			#print("%08x" % (mac(prefix=0xa)))
			mac(prefix=0xa, debug=True)
		sys.exit(0)

	for x_arg in sys.argv[1:]:
		x = int(x_arg,16)
		vals = autocor(x)
		avg = 0
		for v in vals:
			avg += v
		print("%08x %.1f" % (x, avg / len(vals)), vals)
