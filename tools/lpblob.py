#!/usr/bin/env python3
"""Convert the lp_*_code/data arrays of arch/padre/kernel/lpram.c to binary.
Usage: lpblob.py lpram.c OUTDIR"""
import re, struct, sys
src = open(sys.argv[1]).read()
for name, body in re.findall(r'const unsigned long (lp_\w+)\[\] = \{(.*?)\};', src, re.S):
    words = [int(w, 16) for w in re.findall(r'0x[0-9a-fA-F]+', re.sub(r'/\*.*?\*/', '', body))]
    open(f'{sys.argv[2]}/{name}.bin', 'wb').write(struct.pack(f'>{len(words)}I', *words))
    print(name, len(words) * 4)
