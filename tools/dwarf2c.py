#!/usr/bin/env python3
# Print C declarations for named types in a binary's DWARF, from the output of
# `readelf --debug-dump=info`. Meant for binary-only drivers that still carry
# debug information, such as RAIDiator's padre_gmac.ko.
# Usage: tools/dwarf2c.py INFO.txt NAME...
import re
import sys

die_re = re.compile(r'^\s*<(\d+)><([0-9a-f]+)>: Abbrev Number: (\d+)(?: \((\w+)\))?')
attr_re = re.compile(r'^\s*<[0-9a-f]+>\s+(DW_AT_\w+)\s*:\s*(.*)$')


def parse(path):
    dies = {}
    stack = []
    cur = None
    for line in open(path, errors='replace'):
        m = die_re.match(line)
        if m:
            depth, off, num, tag = int(m[1]), int(m[2], 16), m[3], m[4]
            if num == '0':
                cur = None
                continue
            del stack[depth:]
            cur = {'off': off, 'tag': tag, 'kids': [], 'at': {}}
            if stack:
                stack[-1]['kids'].append(cur)
            stack.append(cur)
            dies[off] = cur
            continue
        m = attr_re.match(line)
        if m and cur is not None:
            cur['at'][m[1]] = m[2].strip()
    return dies


def name(d):
    v = d['at'].get('DW_AT_name')
    if v is None:
        return None
    return v.split('): ')[-1] if '): ' in v else v


def ref(d, at='DW_AT_type'):
    v = d['at'].get(at)
    if v is None:
        return None
    m = re.search(r'<0x([0-9a-f]+)>', v)
    return int(m[1], 16) if m else None


def num(d, at):
    v = d['at'].get(at)
    if v is None:
        return None
    m = re.search(r'DW_OP_plus_uconst: (\d+)', v)
    if m:
        return int(m[1])
    m = re.match(r'(-?\d+)', v)
    if m:
        return int(m[1])
    m = re.match(r'0x([0-9a-f]+)', v)
    return int(m[1], 16) if m else None


def tname(dies, off, decl=''):
    """C spelling of the type at off, wrapped around decl."""
    if off is None:
        return ('void ' + decl).rstrip()
    d = dies[off]
    t = d['tag']
    if t in ('DW_TAG_base_type', 'DW_TAG_typedef'):
        return (name(d) + ' ' + decl).rstrip()
    if t in ('DW_TAG_structure_type', 'DW_TAG_union_type',
             'DW_TAG_enumeration_type'):
        kw = {'DW_TAG_structure_type': 'struct', 'DW_TAG_union_type': 'union',
              'DW_TAG_enumeration_type': 'enum'}[t]
        return ('%s %s %s' % (kw, name(d) or '<anon@%x>' % off, decl)).rstrip()
    if t == 'DW_TAG_pointer_type':
        inner = dies.get(ref(d))
        if inner and inner['tag'] in ('DW_TAG_array_type',
                                      'DW_TAG_subroutine_type'):
            return tname(dies, ref(d), '(*' + decl + ')')
        return tname(dies, ref(d), '*' + decl)
    if t in ('DW_TAG_const_type', 'DW_TAG_volatile_type'):
        q = 'const' if t == 'DW_TAG_const_type' else 'volatile'
        return q + ' ' + tname(dies, ref(d), decl)
    if t == 'DW_TAG_array_type':
        dims = ''
        for k in d['kids']:
            ub = num(k, 'DW_AT_upper_bound')
            dims += '[%s]' % ('' if ub is None else ub + 1)
        return tname(dies, ref(d), decl + dims)
    if t == 'DW_TAG_subroutine_type':
        return tname(dies, ref(d), decl + '(...)')
    return '/* %s */ %s' % (t, decl)


def size(dies, off):
    while off is not None:
        d = dies[off]
        s = num(d, 'DW_AT_byte_size')
        if s is not None:
            return s
        if d['tag'] == 'DW_TAG_pointer_type':
            return 4
        if d['tag'] == 'DW_TAG_array_type':
            n = 1
            for k in d['kids']:
                ub = num(k, 'DW_AT_upper_bound')
                n *= 0 if ub is None else ub + 1
            return n * (size(dies, ref(d)) or 0)
        off = ref(d)
    return None


def dump(dies, d, ind=''):
    t = d['tag']
    if t == 'DW_TAG_typedef':
        print('%stypedef %s;\n' % (ind, tname(dies, ref(d), name(d))))
        tgt = dies.get(ref(d))
        if tgt and tgt['tag'] in ('DW_TAG_structure_type', 'DW_TAG_union_type',
                                  'DW_TAG_enumeration_type'):
            dump(dies, tgt, ind)
        return
    if t == 'DW_TAG_enumeration_type':
        print('%senum %s {' % (ind, name(d) or ''))
        for k in d['kids']:
            v = num(k, 'DW_AT_const_value')
            print('%s\t%s = %#x,' % (ind, name(k), v if v is not None else 0))
        print('%s};\n' % ind)
        return
    kw = 'union' if t == 'DW_TAG_union_type' else 'struct'
    print('%s%s %s {\t/* size %#x */' % (ind, kw, name(d) or '',
                                         num(d, 'DW_AT_byte_size') or 0))
    for k in d['kids']:
        if k['tag'] != 'DW_TAG_member':
            continue
        loc = num(k, 'DW_AT_data_member_location')
        bits = ''
        bs = num(k, 'DW_AT_bit_size')
        if bs is not None:
            # Big-endian DWARF 2 bit offsets count from the top of the
            # storage unit; give the bits within the aligned 32-bit word.
            bo = num(k, 'DW_AT_bit_offset')
            msb = 31 - ((loc or 0) % 4) * 8 - bo
            bits = ' : %d\t/* bits %d..%d */' % (bs, msb, msb - bs + 1)
        print('%s\t/* %#5x */ %s%s;' % (ind, loc or 0,
                                        tname(dies, ref(k), name(k) or ''),
                                        bits))
    print('%s};\n' % ind)


def main():
    dies = parse(sys.argv[1])
    want = set(sys.argv[2:])
    seen = set()
    for d in dies.values():
        n = name(d)
        if n in want and d['tag'] in ('DW_TAG_structure_type',
                                      'DW_TAG_union_type', 'DW_TAG_typedef',
                                      'DW_TAG_enumeration_type') \
                and d['kids'] or (n in want and d['tag'] == 'DW_TAG_typedef'):
            if (n, d['tag']) in seen:
                continue
            seen.add((n, d['tag']))
            dump(dies, d)


if __name__ == '__main__':
    main()
