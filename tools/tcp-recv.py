#!/usr/bin/env python3
"""Save each TCP connection from one peer to its own numbered file.
Usage: tcp-recv.py PEER_IP PORT OUTDIR"""
import os
import socket
import sys

peer, port, outdir = sys.argv[1], int(sys.argv[2]), sys.argv[3]
os.makedirs(outdir, exist_ok=True)

srv = socket.socket()
srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
srv.bind(('0.0.0.0', port))
srv.listen(1)
print(f'waiting for {peer} on port {port}', flush=True)

n = 0
while True:
    c, addr = srv.accept()
    if addr[0] != peer:
        print('rejected', addr[0], flush=True)
        c.close()
        continue
    n += 1
    path = os.path.join(outdir, f'recv-{n}.bin')
    size = 0
    with c, open(path, 'wb') as f:
        while (d := c.recv(65536)):
            f.write(d)
            size += len(d)
    print(path, size, flush=True)
