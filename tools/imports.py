"""List a XEX's kernel imports and whether the ReXGlue SDK implements them.
usage: imports.py <xex> <image.bin> <rexglue-sdk dir>"""
import os, re, struct, sys
sys.path.insert(0, os.path.dirname(__file__))
from xex import Xex

x = Xex(sys.argv[1]); img = open(sys.argv[2], "rb").read(); sdk = sys.argv[3]
names = {}
for mod in ("xboxkrnl", "xam"):
    for m in re.finditer(r"XE_EXPORT\(\w+,\s*(0x[0-9A-Fa-f]+),\s*(\w+),\s*(\w+)", open(f"{sdk}/src/kernel/{mod}/export_table.inc").read()):
        names[(mod, int(m.group(1), 16))] = (m.group(2), m.group(3))
impl = {}
for root, _, files in os.walk(f"{sdk}/src/kernel"):
    for fn in files:
        if fn.endswith(".cpp"):
            for m in re.finditer(r"REX_EXPORT(\w*)\(\s*(?:__imp__)?(\w+)", open(os.path.join(root, fn), encoding="utf-8", errors="replace").read()):
                kind = "stub" if "STUB" in m.group(1) else "impl"
                impl.setdefault(m.group(2), kind)
    # variables are registered differently; mark separately
for lib, ver, minv, recs in x.import_libraries():
    mod = lib.split(".")[0]
    for r in recs:
        v = struct.unpack_from(">I", img, r - x.load_address)[0]
        typ, ordinal = v >> 24, v & 0xFFFF
        if typ != 0:
            continue
        name, kind = names.get((mod, ordinal), (f"#{ordinal}", "?"))
        status = impl.get(name, "MISSING") if kind == "kFunction" else "variable"
        print(f"{mod:9s} {ordinal:4d} {status:8s} {name}")
