#!/usr/bin/env python3
"""Log a serial console and break into the iboot console.

Waits for the iboot banner, sends 'i' until the prompt appears or a few seconds
pass, then stops sending by itself. After that, each line written to the FIFO is sent to the target with a
carriage return; nothing else is ever transmitted.

With --no-break, 'i' is never sent and only FIFO lines are transmitted.

Usage: serial-iboot.py [--no-break] OUTFILE FIFO [PORT] [BAUD]"""
import os
import select
import sys
import time

import serial

BANNER = b'Loading'
PROMPT = b'iboot>'
BREAK_SECONDS = 6
BREAK_INTERVAL = 0.05
BYTE_GAP = 0.02

args = [a for a in sys.argv[1:] if a != '--no-break']
send_break = len(args) == len(sys.argv) - 1
out, fifo = args[:2]
port = args[2] if len(args) > 2 else '/dev/ttyUSB0'
baud = int(args[3]) if len(args) > 3 else 9600

if not os.path.exists(fifo):
    os.mkfifo(fifo)
cmd = os.open(fifo, os.O_RDWR | os.O_NONBLOCK)

with serial.Serial(port, baud, bytesize=8, parity='N', stopbits=1, timeout=0) as s, \
        open(out, 'ab', buffering=0) as f:
    f.write(b'--- capture start %d ---\n' % time.time())
    print(f'waiting for iboot on {port} at {baud}', flush=True)
    seen = b''
    until = None
    pending = b''
    while True:
        select.select([s, cmd], [], [], BREAK_INTERVAL)
        d = s.read(4096)
        if d:
            f.write(d)
            if until is None:
                seen = (seen + d)[-64:]
                if send_break and BANNER in seen:
                    until = time.time() + BREAK_SECONDS
        if until is not None:
            seen = (seen + d)[-64:]
            if PROMPT in seen:
                until = 0
        if until is not None and time.time() < until:
            s.write(b'i')
        try:
            pending += os.read(cmd, 4096)
        except BlockingIOError:
            pass
        while b'\n' in pending:
            line, pending = pending.split(b'\n', 1)
            # Bursts have wedged the padre_serial receive path, so pace the bytes.
            for b in line + b'\r':
                s.write(bytes([b]))
                time.sleep(BYTE_GAP)
