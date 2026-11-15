"""Rebuild a shadertest program (.bin: 0x50-byte SPH + code) from a HWDER_DUMP_SHADERS listing.

    python tools/shadertest/listing_to_bin.py build/run/shader_<key>_6.maxwell.txt out/<key>.cs.bin

The listing has one line per 8-byte word ("<pc>: <word hex> ..."), sched slots included. Compute programs have no
SPH, so the header stays zero; for graphics stages the SPH is not in the listing either (only compute is useful).
"""
import re
import struct
import sys


def main():
    src, dst = sys.argv[1], sys.argv[2]
    words = {}
    for line in open(src, encoding="utf-8", errors="replace"):
        m = re.match(r"\s*([0-9a-f]+):\s+([0-9a-f]{16})", line)
        if m:
            words[int(m.group(1), 16)] = int(m.group(2), 16)
    if not words:
        sys.exit("no words in " + src)
    end = max(words) + 8
    data = bytearray(0x50 + end)
    for pc, w in words.items():
        struct.pack_into("<Q", data, 0x50 + pc, w)
    open(dst, "wb").write(data)
    print(f"{dst}: {end // 8} words")


if __name__ == "__main__":
    main()
