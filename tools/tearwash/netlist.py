#!/usr/bin/env python3
"""Development aid for TW4: turn a loaded 224X/XL program image into a readable signal-flow
netlist, so the algorithm can be written as a native C++ network (docs/tearwash/01 §7).

Input: a 512-byte program image as printed by BlueBox's `lexprobe mem ID 4000:200` (reference
tool, run locally). Output (stdout): one line per memory write / DAC output as a weighted sum
of delayed memory taps and ADC inputs, in execution order, with allpass sections recognised.
Nothing here contains program data; it only reads what the user's local tools produce.

usage: lexprobe mem 01 4000:200 | tools/tearwash/netlist.py [--raw | --list]
"""
import sys


def parse_dump(text):
    out = []
    for line in text.splitlines():
        if ':' not in line:
            continue
        out += [int(b, 16) for b in line.split(':', 1)[1].split()]
    if len(out) < 512:
        sys.exit('need 512 bytes')
    return out[:512]


def decode(img):
    """Steps in execution order (pc 0 = 8080 step 127)."""
    steps = []
    for pc in range(128):
        n = 127 - pc
        b = img[4 * n:4 * n + 4]
        w = b[0] | b[1] << 8 | b[2] << 16 | b[3] << 24
        op = {3: 'none', 2: 'io', 1: 'mw', 0: 'mr'}[(w >> 16) & 3]
        s = dict(pc=pc, op=op, low=w & 0xFFFF, wa=(w >> 18) & 3, ra=(w >> 20) & 3,
                 sign=1 if (w >> 23) & 1 else -1, xfer=not (w >> 24) & 1, zero=not (w >> 25) & 1,
                 m=(~w >> 26) & 0x3F)
        if op == 'io':
            lo = s['low']
            s['src'] = {3: None, 2: 'rreg', 1: 'xreg', 0: 'adc'}[(lo >> 12) & 3]
            s['dacs'] = ''.join(c for i, c in zip((11, 10, 9, 8), 'ABCD') if not (lo >> i) & 1) if not (lo >> 7) & 1 else ''
            s['reset'] = not (lo >> 3) & 1
        steps.append(s)
    loop = next(s['pc'] for s in steps if s['op'] == 'io' and s['reset']) + 2
    return steps[:loop], loop


def analyse(steps):
    """Symbolic pass: expressions are lists of (coef_m_signed, symbol)."""
    regs = [None] * 4
    acc, result = [], None
    nodes, writes, outs = [], [], []
    adc = 0
    for s in steps:
        # Bus value of this step.
        bus = None
        if s['op'] == 'mr':
            bus = ('tap', s['low'], s['pc'])
        elif s['op'] == 'mw':
            bus = result                      # pre-XFER result register
            writes.append((s['low'], s['pc'], result))
        elif s['op'] == 'io':
            if s['src'] == 'adc':
                bus = ('adc', 'LR'[adc % 2]); adc += 1
            elif s['src'] == 'rreg':
                bus = result
            if s['dacs'] and bus is not None:
                outs.append((s['dacs'], s['pc'], bus))
        if bus is not None:
            regs[s['wa']] = bus
        x = regs[s['ra']]
        if s['xfer']:
            nodes.append(list(acc))
            result = ('node', len(nodes) - 1)
        if s['zero']:
            acc = []
        if s['m'] and x is not None:
            acc.append((s['sign'] * s['m'], x))
    return nodes, writes, outs


def fmt_sym(sym, writes):
    kind = sym[0]
    if kind == 'adc':
        return 'in' + sym[1]
    if kind == 'node':
        return 'N%d' % sym[1]
    q, pc = sym[1], sym[2]
    # Last writer of the address read at offset q: the write with the largest offset <= q
    # (same offset counts only if it was written earlier in the pass).
    best = None
    for o, wpc, _ in writes:
        d = (q - o) & 0xFFFF
        if d == 0 and wpc > pc:
            continue
        if best is None or d < best[0]:
            best = (d, o)
    if best is None:
        return 'tap@%d' % q
    return 'M%d[-%d]' % (best[1], best[0])


def listing(steps):
    """Compact per-step listing: bus source, register traffic, MAC, XFER/ZERO, store, DAC."""
    for s in steps:
        bus = {'mr': 'tap %d' % s['low'], 'mw': 'R->mem %d' % s['low'], 'none': '0'}.get(s['op'])
        if s['op'] == 'io':
            bus = {'adc': 'adc', 'rreg': 'R', 'xreg': 'X', None: '0'}[s['src']]
            if s['dacs']:
                bus += ' ->dac ' + s['dacs']
            if s['reset']:
                bus += ' RESET'
        mac = '%+d*r%d' % (s['sign'] * s['m'], s['ra']) if s['m'] else ''
        flags = ('X' if s['xfer'] else '.') + ('Z' if s['zero'] else '.')
        print('pc%3d %-18s r%d<-bus  %s %s' % (s['pc'], bus, s['wa'], flags, mac))


def main():
    if '--list' in sys.argv:
        steps, loop = decode(parse_dump(sys.stdin.read()))
        print('loop %d steps' % loop)
        listing(steps)
        return
    raw = '--raw' in sys.argv
    steps, loop = decode(parse_dump(sys.stdin.read()))
    nodes, writes, outs = analyse(steps)
    print('loop %d steps' % loop)
    if raw:
        for s in steps:
            print(s)

    def expr(terms):
        return ' '.join('%+g*%s' % (c / 32.0, fmt_sym(sym, writes)) for c, sym in terms) or '0'

    def resolve(sym):
        return fmt_sym(sym, writes) if sym else '?'

    for o, pc, src in writes:
        body = expr(nodes[src[1]]) if src and src[0] == 'node' else resolve(src)
        print('pc%3d  M%-5d = %s' % (pc, o, body))
    for dacs, pc, src in outs:
        body = expr(nodes[src[1]]) if src and src[0] == 'node' else resolve(src)
        print('pc%3d  out%-4s = %s' % (pc, dacs, body))


if __name__ == '__main__':
    main()
