"""XEX2 (Xbox 360 executable) reader.

usage: xex.py <file.xex>                 print headers
       xex.py <file.xex> dump <out.bin>  write the decrypted, decompressed image (loaded at load address)

Retail images are encrypted: set XEX_RETAIL_KEY (32 hex digits) before using "dump".
"""
import os
import struct
import sys

# The retail XEX key is not included here. To dump a retail image, supply it yourself as 32 hex
# digits in the XEX_RETAIL_KEY environment variable.
RETAIL_KEY = bytes.fromhex(os.environ.get("XEX_RETAIL_KEY", "00" * 16))
DEVKIT_KEY = bytes(16)

HEADER_NAMES = {
    0x000002FF: "RESOURCE_INFO",
    0x000003FF: "FILE_FORMAT_INFO",
    0x000005FF: "DELTA_PATCH_DESCRIPTOR",
    0x00000405: "BASE_REFERENCE",
    0x000080FF: "BOUNDING_PATH",
    0x00008105: "DEVICE_ID",
    0x00010001: "ORIGINAL_BASE_ADDRESS",
    0x00010100: "ENTRY_POINT",
    0x00010201: "IMAGE_BASE_ADDRESS",
    0x000103FF: "IMPORT_LIBRARIES",
    0x00018002: "CHECKSUM_TIMESTAMP",
    0x00018102: "ENABLED_FOR_CALLCAP",
    0x00018200: "ENABLED_FOR_FASTCAP",
    0x000183FF: "ORIGINAL_PE_NAME",
    0x000200FF: "STATIC_LIBRARIES",
    0x00020104: "TLS_INFO",
    0x00020200: "DEFAULT_STACK_SIZE",
    0x00020301: "DEFAULT_FILESYSTEM_CACHE_SIZE",
    0x00020401: "DEFAULT_HEAP_SIZE",
    0x00028002: "PAGE_HEAP_SIZE_AND_FLAGS",
    0x00030000: "SYSTEM_FLAGS",
    0x00040006: "EXECUTION_INFO",
    0x00040201: "TITLE_WORKSPACE_SIZE",
    0x00040310: "GAME_RATINGS",
    0x00040404: "LAN_KEY",
    0x000405FF: "XBOX360_LOGO",
    0x000406FF: "MULTIDISC_MEDIA_IDS",
    0x000407FF: "ALTERNATE_TITLE_IDS",
    0x00040801: "ADDITIONAL_TITLE_MEMORY",
    0x00E10402: "EXPORTS_BY_NAME",
}


def be32(b, o):
    return struct.unpack_from(">I", b, o)[0]


def be16(b, o):
    return struct.unpack_from(">H", b, o)[0]


class Xex:
    def __init__(self, path):
        with open(path, "rb") as f:
            self.raw = f.read()
        b = self.raw
        if b[:4] != b"XEX2":
            raise ValueError("not a XEX2 file")
        self.module_flags = be32(b, 4)
        self.pe_offset = be32(b, 8)
        self.security_offset = be32(b, 0x10)
        count = be32(b, 0x14)
        self.headers = {}
        for i in range(count):
            key, val = struct.unpack_from(">II", b, 0x18 + i * 8)
            low = key & 0xFF
            if low == 0x00 or low == 0x01:
                self.headers[key] = val
            elif low == 0xFF:
                size = be32(b, val)
                self.headers[key] = b[val:val + size]
            else:
                self.headers[key] = b[val:val + low * 4]
        s = self.security_offset
        self.image_size = be32(b, s + 4)
        self.image_flags = be32(b, s + 0x10C)
        self.load_address = be32(b, s + 0x110)
        self.file_key = b[s + 0x150:s + 0x160]
        self.region = be32(b, s + 0x178)
        self.allowed_media = be32(b, s + 0x17C)
        n = be32(b, s + 0x180)
        self.pages = []  # (size, flags)
        for i in range(n):
            info = be32(b, s + 0x184 + i * 24)
            self.pages.append((info >> 4, info & 0xF))
        ffi = self.headers.get(0x000003FF)
        self.encryption = be16(ffi, 4) if ffi else 0
        self.compression = be16(ffi, 6) if ffi else 0
        self.entry = self.headers.get(0x00010100)
        if 0x00010201 in self.headers:
            self.load_address = self.headers[0x00010201]

    # ---- optional header decoding ----
    def execution_info(self):
        d = self.headers.get(0x00040006)
        if not d:
            return None
        media_id, version, base_version, title_id = struct.unpack_from(">IIII", d, 0)
        platform, exe_type, disc, discs = d[16], d[17], d[18], d[19]
        return dict(media_id=media_id, version=version, base_version=base_version,
                    title_id=title_id, disc=disc, discs=discs)

    def import_libraries(self):
        """returns [(name, version, min_version, [record addresses])]"""
        d = self.headers.get(0x000103FF)
        if not d:
            return []
        string_size, lib_count = be32(d, 4), be32(d, 8)
        strs = d[12:12 + string_size].split(b"\0")
        names = [x.decode() for x in strs if x]
        o = 12 + string_size
        libs = []
        for _ in range(lib_count):
            size = be32(d, o)
            name_index = be16(d, o + 0x24 + 0x0C) if False else None
            # layout: size, digest[20], id, version, min_version, name_index(u16), count(u16), records[count]
            lib_id = be32(d, o + 0x18)
            version = be32(d, o + 0x1C)
            min_version = be32(d, o + 0x20)
            name_index = be16(d, o + 0x24)
            rec_count = be16(d, o + 0x26)
            recs = [be32(d, o + 0x28 + i * 4) for i in range(rec_count)]
            libs.append((names[name_index], version, min_version, recs))
            o += size
        return libs

    def static_libraries(self):
        d = self.headers.get(0x000200FF)
        if not d:
            return []
        out = []
        for o in range(4, len(d), 16):
            name = d[o:o + 8].split(b"\0")[0].decode()
            major, minor, build, qfe = struct.unpack_from(">HHHH", d, o + 8)
            out.append((name, major, minor, build, qfe & 0xFF, qfe >> 8))
        return out

    def resources(self):
        d = self.headers.get(0x000002FF)
        if not d:
            return []
        out = []
        for o in range(4, len(d), 16):
            name = d[o:o + 8].split(b"\0")[0].decode(errors="replace")
            addr, size = struct.unpack_from(">II", d, o + 8)
            out.append((name, addr, size))
        return out

    def tls_info(self):
        d = self.headers.get(0x00020104)
        if not d:
            return None
        return struct.unpack_from(">IIII", d, 0)  # slot_count, raw_data_address, data_size, raw_data_size

    # ---- image extraction ----
    def session_key(self, key=RETAIL_KEY):
        from Crypto.Cipher import AES
        return AES.new(key, AES.MODE_ECB).decrypt(self.file_key)

    def _decrypt(self, data, key=RETAIL_KEY):
        if self.encryption == 0:
            return data
        from Crypto.Cipher import AES
        return AES.new(self.session_key(key), AES.MODE_CBC, bytes(16)).decrypt(data)

    def image(self, key=RETAIL_KEY):
        payload = self.raw[self.pe_offset:]
        if self.compression == 1:  # basic: (data_size, zero_size) blocks
            ffi = self.headers[0x000003FF]
            data = self._decrypt(payload[:len(payload) & ~15] , key)
            out = bytearray()
            src = 0
            for o in range(8, be32(ffi, 0), 8):
                ds, zs = struct.unpack_from(">II", ffi, o)
                out += data[src:src + ds]
                out += bytes(zs)
                src += ds
            return bytes(out)
        if self.compression == 2:  # normal: LZX in a chain of hashed blocks
            ffi = self.headers[0x000003FF]
            window_size = be32(ffi, 8)
            block_size = be32(ffi, 12)
            data = self._decrypt(payload[:len(payload) & ~15], key)
            comp = bytearray()
            p = 0
            while block_size:
                next_size = be32(data, p)
                q = p + 24
                end = p + block_size
                while q < end:
                    n = be16(data, q)
                    q += 2
                    if n == 0:
                        break
                    comp += data[q:q + n]
                    q += n
                p = end
                block_size = next_size
            import lzx
            return lzx.decompress(bytes(comp), window_size, self.image_size)
        if self.compression == 0:
            return self._decrypt(payload, key)[:self.image_size]
        raise ValueError(f"unsupported compression {self.compression}")


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    x = Xex(sys.argv[1])
    if len(sys.argv) > 3 and sys.argv[2] == "dump":
        img = x.image()
        if img[:2] != b"MZ":
            img2 = x.image(DEVKIT_KEY)
            if img2[:2] == b"MZ":
                img = img2
            else:
                print("warning: image does not start with MZ (is XEX_RETAIL_KEY set?)")
        with open(sys.argv[3], "wb") as f:
            f.write(img)
        print(f"wrote {len(img):#x} bytes (load address {x.load_address:#010x})")
        return
    print(f"module flags   {x.module_flags:#x}")
    print(f"load address   {x.load_address:#010x}  image size {x.image_size:#x}")
    print(f"entry point    {x.entry:#010x}" if x.entry else "entry point    -")
    print(f"image flags    {x.image_flags:#x}  region {x.region:#x}  media {x.allowed_media:#x}")
    print(f"encryption {x.encryption}  compression {x.compression}")
    ei = x.execution_info()
    if ei:
        print("execution      " + " ".join(f"{k}={v:#x}" for k, v in ei.items()))
    for k, v in x.headers.items():
        name = HEADER_NAMES.get(k, "?")
        if isinstance(v, int):
            print(f"  {k:08x} {name:30s} {v:#x}")
        else:
            print(f"  {k:08x} {name:30s} [{len(v)} bytes]")
    pe = x.headers.get(0x000183FF)
    if pe:
        print("original PE    ", pe[4:].split(b"\0")[0].decode())
    print("tls            ", x.tls_info())
    for r in x.resources():
        print(f"resource       {r[0]:8s} {r[1]:#010x} {r[2]:#x}")
    for s in x.static_libraries():
        print("static lib     %-8s %d.%d.%d.%d (approval %d)" % s)
    for name, ver, minv, recs in x.import_libraries():
        print(f"import         {name} {ver:#x} ({len(recs)} records)")
    print("pages          ", len(x.pages), "x 64K" if x.pages else "")


if __name__ == "__main__":
    main()
