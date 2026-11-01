#!/usr/bin/env python3
"""Record a serial console to a file. Receive only: nothing is sent to the target.
Usage: serial-log.py OUTFILE [PORT] [BAUD]"""
import sys
import time

import serial

out = sys.argv[1]
port = sys.argv[2] if len(sys.argv) > 2 else '/dev/ttyUSB0'
baud = int(sys.argv[3]) if len(sys.argv) > 3 else 9600

with serial.Serial(port, baud, bytesize=8, parity='N', stopbits=1, timeout=0.5) as s, \
        open(out, 'ab', buffering=0) as f:
    f.write(b'--- capture start %d ---\n' % time.time())
    print(f'listening on {port} at {baud}', flush=True)
    while True:
        d = s.read(4096)
        if d:
            f.write(d)
