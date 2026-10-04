"""Helpers for scanning the dumped XEX image (big-endian PPC)."""
import struct, capstone
BASE = 0x82000000
class Image:
    def __init__(self, path):
        self.d = open(path, "rb").read()
        pe = struct.unpack_from("<I", self.d, 0x3C)[0]
        nsec = struct.unpack_from("<H", self.d, pe + 6)[0]
        opt = struct.unpack_from("<H", self.d, pe + 20)[0]
        self.sections = {}
        o = pe + 24 + opt
        for _ in range(nsec):
            name = self.d[o:o + 8].rstrip(b"\0").decode()
            vs, va = struct.unpack_from("<II", self.d, o + 8)
            ch = struct.unpack_from("<I", self.d, o + 36)[0]
            self.sections[name] = (BASE + va, vs, ch)
            o += 40
        self.md = capstone.Cs(capstone.CS_ARCH_PPC, capstone.CS_MODE_32 | capstone.CS_MODE_BIG_ENDIAN)
    def u32(self, a): return struct.unpack_from(">I", self.d, a - BASE)[0]
    def pdata(self):
        va, vs, _ = self.sections[".pdata"]
        for a in range(va, va + vs, 8):
            start, info = struct.unpack_from(">II", self.d, a - BASE)
            if start == 0: break
            yield start, ((info >> 8) & 0x3FFFFF) * 4, info
    def dis(self, start, size):
        return list(self.md.disasm(self.d[start - BASE:start - BASE + size], start))
