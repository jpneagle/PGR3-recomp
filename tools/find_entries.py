"""Find code addresses referenced from data or built with lis/addi that the codegen did not register
as function entries. Prints [functions] lines for pgr3_config.toml.
usage: find_entries.py <image.pe> <generated/default/pgr3_init.cpp>"""
import re, struct, sys, os
sys.path.insert(0, os.path.dirname(__file__))
from ppcscan import Image, BASE

im = Image(sys.argv[1])
known = {int(m, 16) for m in re.findall(r"\{ 0x([0-9A-F]+), ", open(sys.argv[2]).read())}
code = [(va, va + vs) for n, (va, vs, ch) in im.sections.items() if ch & 0x20000000]
def in_code(a): return any(s <= a < e for s, e in code)

def starts_like_function(a):
    """Previous word is padding or an unconditional terminator (blr, b, bctr)."""
    p = im.u32(a - 4)
    return p == 0 or p == 0x4E800020 or p == 0x4E800420 or (p >> 26 == 18 and not p & 1)

cands = {}
# 1) pointer-sized values in data sections
for n, (va, vs, ch) in im.sections.items():
    if ch & 0x20000000 or n in (".reloc", ".pdata"):
        continue
    for a in range(va, va + vs - 3, 4):
        v = im.u32(a)
        if v & 3 == 0 and in_code(v) and v not in known:
            cands.setdefault(v, set()).add(f"data@{a:08x}")
# 2) lis rX,hi ; addi rY,rX,lo  (callbacks passed in registers)
for s, e in code:
    hi = {}
    for a in range(s, e, 4):
        w = im.u32(a)
        op = w >> 26
        if op == 15 and (w >> 16) & 31 == 0:  # lis
            hi[(w >> 21) & 31] = (w & 0xFFFF) << 16
        elif op == 14 and (w >> 16) & 31 in hi:  # addi
            v = (hi[(w >> 16) & 31] + struct.unpack(">h", struct.pack(">H", w & 0xFFFF))[0]) & 0xFFFFFFFF
            # catch funclets return their continuation address in r3 right before the epilogue
            funclet_ret = (w >> 21) & 31 == 3 and im.u32(a + 4) >> 16 == 0x3821 and                 any(im.u32(a + k) == 0x4E800020 for k in range(8, 24, 4))
            if v & 3 == 0 and in_code(v) and v not in known and not funclet_ret:
                cands.setdefault(v, set()).add(f"lis@{a:08x}")
        if w == 0x4E800020:
            hi.clear()
pdata = sorted((s, s + sz) for s, sz, _ in im.pdata())
import bisect
starts = [p[0] for p in pdata]
def in_pdata(a):
    i = bisect.bisect_right(starts, a) - 1
    return i >= 0 and pdata[i][0] < a < pdata[i][1]
# Addresses inside a .pdata function are labels (jump tables, catch continuations), not entries.
# Inline jump tables (right after a bctr) hold code addresses themselves.
out = sorted(a for a in cands if starts_like_function(a) and not in_pdata(a) and not in_code(im.u32(a)))
bounds = sorted(known | set(out) | set(starts))
for a in out:
    nxt = bounds[bisect.bisect_right(bounds, a)]
    e = a
    while e < nxt and im.u32(e) != 0:
        e += 4
    print(f"0x{a:08X} = {{ end = 0x{e:08X} }}  # {', '.join(sorted(cands[a])[:3])}")
print(f"# {len(out)} entries ({len(cands)} unregistered code references)", file=sys.stderr)
