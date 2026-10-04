"""Lists or extracts the files of an Xbox DVD image (XDVDFS).

Accepts both full disc images (redump: video partition followed by the game
partition) and game-partition-only images (xiso).

usage: xdvdfs.py <image.iso>                          list files
       xdvdfs.py <image.iso> extract <out dir> [suffix...]
"""
import os
import struct
import sys

MAGIC = b"MICROSOFT*XBOX*MEDIA"
# Where the game partition starts: xiso, redump, and two other known layouts.
PARTITION_OFFSETS = (0, 0x18300000, 0x2080000, 0xFD90000)


def find_partition(f):
    for off in PARTITION_OFFSETS:
        f.seek(off + 0x10000)
        if f.read(20) == MAGIC:
            return off
    return None


def list_files(f, base):
    f.seek(base + 0x10000 + 20)
    root_sec, root_size = struct.unpack("<II", f.read(8))
    files = []

    def walk(sec, size, path):
        if size == 0:
            return
        f.seek(base + sec * 2048)
        data = f.read(size)
        stack, seen = [0], set()
        while stack:
            o = stack.pop()
            if o in seen or o + 14 > len(data):
                continue
            seen.add(o)
            left, right, s, sz, attr, nl = struct.unpack_from("<HHIIBB", data, o)
            if left == 0xFFFF:
                continue
            p = path + "/" + data[o + 14:o + 14 + nl].decode("latin1")
            if attr & 0x10:
                walk(s, sz, p)
            else:
                files.append((p, s, sz))
            if left:
                stack.append(left * 4)
            if right:
                stack.append(right * 4)

    walk(root_sec, root_size, "")
    return sorted(files)


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    with open(sys.argv[1], "rb") as f:
        base = find_partition(f)
        if base is None:
            sys.exit(f"{sys.argv[1]}: not an Xbox disc image (no XDVDFS volume found)")
        files = list_files(f, base)
        if len(sys.argv) > 3 and sys.argv[2] == "extract":
            out, suffixes = sys.argv[3], [s.lower() for s in sys.argv[4:]]
            count = total = 0
            for p, s, sz in files:
                if suffixes and not any(p.lower().endswith(x) for x in suffixes):
                    continue
                dst = os.path.join(out, *p.split("/")[1:])
                os.makedirs(os.path.dirname(dst), exist_ok=True)
                f.seek(base + s * 2048)
                with open(dst, "wb") as o:
                    remaining = sz
                    while remaining:
                        chunk = f.read(min(remaining, 1 << 24))
                        if not chunk:
                            sys.exit(f"{p}: image is truncated")
                        o.write(chunk)
                        remaining -= len(chunk)
                count += 1
                total += sz
            print(f"extracted {count} files, {total} bytes to {out}")
        else:
            for p, s, sz in files:
                print(f"{sz:12d}  {p}")
            print(len(files), "files", sum(sz for _, _, sz in files), "bytes")


if __name__ == "__main__":
    main()
