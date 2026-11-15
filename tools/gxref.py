"""Cross references in the game's ExeFS (update 1.5.0): who loads a string or address, who calls a function, who writes
a field. The searches that found the display modes and the frame rate (docs/few.md "Modos de exibição").

    python3 tools/gxref.py str nvnWindowSetPresentInterval   # code that loads the string (ADRP+ADD) and its context
    python3 tools/gxref.py addr 0x1001309980                 # ADRP+ADD/LDR/STR of an address (GOT slots, globals)
    python3 tools/gxref.py callers 0x10008b7f1c              # BL/B to a function, plus 8-byte data words equal to it
    python3 tools/gxref.py stores 0x2adc [ldr]               # STR (or LDR) W, [Xn, #off] with that unsigned offset
    python3 tools/gxref.py movz 0x9cb4                       # MOV Wd, #imm (large struct offsets go through a register)
    python3 tools/gxref.py dis 0x10008b7f1c 0x10008b7f78     # disassemble a range
    python3 tools/gxref.py words 0x1000dae718 10             # s32 words (jump and lookup tables)

Needs capstone and lz4 (WSL archlinux: pacman -S python-capstone python-lz4). Searches "main" unless --mods says
otherwise (--mods main,sdk). Addresses are guest addresses (main at 0x1000000000).
"""
import argparse
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from nso import Process  # noqa: E402
import capstone  # noqa: E402

P = Process(os.path.join(HERE, "..", "data", "update", "exefs"))
MD = capstone.Cs(capstone.CS_ARCH_ARM64, capstone.CS_MODE_ARM)


def mod_of(a):
    for m in P.modules:
        if m.base <= a < m.base + len(m.image):
            return m
    raise SystemExit(f"{a:#x} is in no module")


def words(m):
    return struct.iter_unpack("<I", m.image[: len(m.image) // 4 * 4])


def dis(a, b, mark=None):
    m = mod_of(a)
    for x, _, mn, op in MD.disasm_lite(bytes(m.image[a - m.base:b - m.base]), a):
        print(f"   {'>' if x == mark else ' '}{x:#x} {mn} {op}")


def adrp_page(w, pc):
    if (w & 0x9F000000) != 0x90000000:
        return None
    imm = (((w >> 5) & 0x7FFFF) << 2) | ((w >> 29) & 3)
    if imm & 0x100000:
        imm -= 0x200000
    return (pc & ~0xFFF) + (imm << 12), w & 31


def addr_refs(target, mods):
    out = []
    for m in P.modules:
        if m.name not in mods:
            continue
        ws = [w for (w,) in words(m)]
        for i, w in enumerate(ws):
            r = adrp_page(w, m.base + i * 4)
            if not r or r[0] != (target & ~0xFFF):
                continue
            page, rd = r
            for w2 in ws[i + 1:i + 8]:
                if ((w2 >> 5) & 31) != rd:
                    continue
                top = w2 & 0xFFC00000
                imm = (w2 >> 10) & 0xFFF
                scale = {0x91000000: 1, 0xF9400000: 8, 0xF9000000: 8, 0xB9400000: 4, 0xB9000000: 4}.get(top)
                if scale and page + imm * scale == target:
                    out.append(m.base + i * 4)
                break
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("cmd")
    ap.add_argument("args", nargs="*")
    ap.add_argument("--mods", default="main")
    a = ap.parse_args()
    mods = a.mods.split(",")
    if a.cmd == "str":
        needle = a.args[0].encode() + b"\0"
        for m in P.modules:
            i = m.image.find(needle)
            while i >= 0:
                t = m.base + i
                print(f"== {a.args[0]} at {t:#x} ({m.name})")
                for x in addr_refs(t, mods):
                    print(f" ref {x:#x}")
                    dis(x - 8, x + 48, x)
                i = m.image.find(needle, i + 1)
    elif a.cmd == "addr":
        t = int(a.args[0], 16)
        for x in addr_refs(t, mods):
            print(f" ref {x:#x}")
            dis(x - 8, x + 48, x)
    elif a.cmd == "callers":
        t = int(a.args[0], 16)
        for m in P.modules:
            if m.name not in mods:
                continue
            for i, (w,) in enumerate(words(m)):
                if (w & 0x7C000000) == 0x14000000:
                    imm = w & 0x3FFFFFF
                    if imm & 0x2000000:
                        imm -= 0x4000000
                    pc = m.base + i * 4
                    if pc + imm * 4 == t:
                        print(f" {'bl' if w & 0x80000000 else 'b'} at {pc:#x}")
                        dis(pc - 40, pc + 8, pc)
            tb = struct.pack("<Q", t)
            i = m.image.find(tb)
            while i >= 0:
                print(f" data pointer at {m.base + i:#x}")
                i = m.image.find(tb, i + 1)
    elif a.cmd in ("stores", "movz"):
        off = int(a.args[0], 16)
        if a.cmd == "stores":
            base = 0xB9400000 if len(a.args) > 1 and a.args[1] == "ldr" else 0xB9000000
            match = lambda w: (w & 0xFFFFFC00) == base | ((off // 4) << 10)
        else:
            match = lambda w: (w & 0x7FE00000) == 0x52800000 and ((w >> 5) & 0xFFFF) == off
        for m in P.modules:
            if m.name not in mods:
                continue
            for i, (w,) in enumerate(words(m)):
                if match(w):
                    pc = m.base + i * 4
                    print(f"== {pc:#x}")
                    dis(pc - 16, pc + 16, pc)
    elif a.cmd == "dis":
        dis(int(a.args[0], 16), int(a.args[1], 16))
    elif a.cmd == "words":
        t, n = int(a.args[0], 16), int(a.args[1])
        m = mod_of(t)
        print([struct.unpack_from("<i", m.image, t - m.base + 4 * k)[0] for k in range(n)])
    else:
        raise SystemExit(__doc__)


if __name__ == "__main__":
    main()
