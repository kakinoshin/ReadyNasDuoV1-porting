#!/usr/bin/env python3
"""Extract members of a remote ZIP whose names match REGEX.
Usage: zipfetch.py URL REGEX OUTDIR"""
import importlib.util, io, os, re, sys, zipfile

url, rx, out = sys.argv[1:4]
out = os.path.abspath(out)

# remotezip.py runs on import, so load only its range reader.
here = os.path.dirname(os.path.abspath(__file__))
src = open(os.path.join(here, 'remotezip.py')).read().split('z = zipfile.ZipFile')[0]
ns = {}
exec(src, ns)

z = zipfile.ZipFile(io.BufferedReader(ns['R'](url), 1 << 20))
n = 0
for i in z.infolist():
    if i.is_dir() or not re.search(rx, i.filename):
        continue
    p = os.path.normpath(os.path.join(out, i.filename))
    if not p.startswith(out + os.sep):
        continue
    os.makedirs(os.path.dirname(p), exist_ok=True)
    with z.open(i) as f, open(p, 'wb') as o:
        o.write(f.read())
    n += 1
print(n, 'files')
