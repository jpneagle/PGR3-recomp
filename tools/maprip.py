"""maprip.py <offset hex>... - map pgr3.exe+offset (from pgr3_crash.txt) to symbol+offset via pgr3.map."""
import re, sys, bisect
syms = []
base = None
for line in open('out/build/win-amd64-release/pgr3.map', errors='replace'):
    m = re.match(r'\s*[0-9a-f]{4}:[0-9a-f]{8}\s+(\S+)\s+([0-9a-f]{16})\s', line)
    if m:
        syms.append((int(m.group(2), 16), m.group(1)))
    m = re.search(r'Preferred load address is ([0-9a-f]+)', line)
    if m: base = int(m.group(1), 16)
syms.sort(); addrs = [a for a, _ in syms]
for arg in sys.argv[1:]:
    off = int(arg, 16); a = base + off
    i = bisect.bisect_right(addrs, a) - 1
    print(f"+{off:x}  {syms[i][1]}+{a - syms[i][0]:#x}")
