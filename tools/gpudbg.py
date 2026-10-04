"""gpudbg.py <run name> - summarise --gpu_debug_log_frame_ms output (run-length by state)."""
import re, sys, os
d = 'titles/pgr3/run/'; name = sys.argv[1]
files = sorted((f for f in os.listdir(d) if re.fullmatch(re.escape(name) + r'\.\d+\.log', f)),
               key=lambda f: -int(f.split('.')[-2])) + [name + '.log']
prev = None; n = 0; first = None
def flush():
    if prev: print(f"{first:6d} x{n:<6d} {prev}")
for f in files:
    for line in open(d + f, encoding='utf-8', errors='replace'):
        m = re.search(r'gpudbg: (.*)', line)
        if not m: continue
        t = m.group(1)
        dm = re.match(r'draw (\d+) prim (\d+) count \d+ (.*)', t)
        if dm:
            key = f"prim {dm.group(2)} {dm.group(3)}"
            if key != prev:
                flush(); prev = key; n = 0; first = int(dm.group(1))
            n += 1
        else:
            flush(); prev = None
            print(line[12:24], t)
flush()
