#!/usr/bin/env python3
"""List or extract single members of a remote ZIP via HTTP Range requests.
Usage: remotezip.py URL            list
       remotezip.py URL MEMBER OUT extract"""
import io, sys, time, urllib.request, zipfile

class R(io.RawIOBase):
    def __init__(s, u):
        s.u, s.p = u, 0
        s.n = int(s._get(0, 0)[1].split('/')[1])
    def _get(s, a, b):
        for t in range(6):
            try:
                q = urllib.request.Request(s.u, headers={'Range': f'bytes={a}-{b}'})
                with urllib.request.urlopen(q, timeout=120) as r:
                    return r.read(), r.headers.get('Content-Range')
            except Exception as e:
                print('retry', e, file=sys.stderr); time.sleep(5 * (t + 1))
        raise IOError('range failed')
    def seekable(s): return True
    def readable(s): return True
    def seek(s, o, w=0):
        s.p = o if w == 0 else s.p + o if w == 1 else s.n + o
        return s.p
    def tell(s): return s.p
    def read(s, n=-1):
        if n < 0: n = s.n - s.p
        if n == 0 or s.p >= s.n: return b''
        d = s._get(s.p, min(s.p + n, s.n) - 1)[0]
        s.p += len(d)
        return d
    def readinto(s, b):
        d = s.read(len(b)); b[:len(d)] = d; return len(d)

z = zipfile.ZipFile(io.BufferedReader(R(sys.argv[1]), 1 << 20))
if len(sys.argv) == 2:
    for i in z.infolist(): print(i.file_size, i.compress_size, i.filename)
else:
    with z.open(sys.argv[2]) as f, open(sys.argv[3], 'wb') as o:
        while (b := f.read(1 << 20)): o.write(b)
