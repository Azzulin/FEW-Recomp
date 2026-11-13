#!/usr/bin/env python3
"""H.264 parameters of the game's movies (the avcC box of each .mp4 in the base RomFS), read in place.

    python tools/movie_info.py <prod.keys> <base.nsp> [folder prefix, default /nx/movie/eng/]

Prints what the runtime's NVDEC path has to reproduce when it synthesizes SPS/PPS (video/decoder.cpp
build_headers): profile, level, POC type, reference frames, reordering, entropy coder, weighted prediction.
"""
import os
import struct
import sys

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import nx_extract as nx  # noqa: E402
from inspect_dump import romfs_files  # noqa: E402


class Bits:
    def __init__(self, data):
        # Drop emulation-prevention bytes.
        out, zeros = bytearray(), 0
        for b in data:
            if zeros >= 2 and b == 3:
                zeros = 0
                continue
            out.append(b)
            zeros = zeros + 1 if b == 0 else 0
        self.d, self.p = bytes(out), 0

    def u(self, n):
        v = 0
        for _ in range(n):
            v = (v << 1) | ((self.d[self.p >> 3] >> (7 - (self.p & 7))) & 1)
            self.p += 1
        return v

    def ue(self):
        z = 0
        while self.u(1) == 0:
            z += 1
        return (1 << z) - 1 + self.u(z)

    def se(self):
        v = self.ue()
        return (v + 1) // 2 if v & 1 else -(v // 2)


def skip_scaling(b, n):
    last = nxt = 8
    for _ in range(n):
        if nxt:
            nxt = (last + b.se() + 256) % 256
        last = nxt or last


def sps(data):
    b = Bits(data[1:])
    r = {"profile": b.u(8)}
    b.u(8)
    r["level"] = b.u(8)
    b.ue()
    if r["profile"] in (100, 110, 122, 244, 44, 83, 86, 118, 128):
        r["chroma"] = b.ue()
        if r["chroma"] == 3:
            b.u(1)
        b.ue(), b.ue(), b.u(1)
        if b.u(1):
            for i in range(8 if r["chroma"] != 3 else 12):
                if b.u(1):
                    skip_scaling(b, 16 if i < 6 else 64)
    r["log2_max_frame_num_minus4"] = b.ue()
    r["poc_type"] = b.ue()
    if r["poc_type"] == 0:
        r["log2_max_poc_lsb_minus4"] = b.ue()
    elif r["poc_type"] == 1:
        b.u(1), b.se(), b.se()
        for _ in range(b.ue()):
            b.se()
    r["max_num_ref_frames"] = b.ue()
    r["gaps"] = b.u(1)
    r["width_mbs"] = b.ue() + 1
    r["height_map_units"] = b.ue() + 1
    r["frame_mbs_only"] = b.u(1)
    if not r["frame_mbs_only"]:
        b.u(1)
    r["direct_8x8"] = b.u(1)
    if b.u(1):
        b.ue(), b.ue(), b.ue(), b.ue()
    if b.u(1):  # VUI
        if b.u(1):
            if b.u(8) == 255:
                b.u(16), b.u(16)
        if b.u(1):
            b.u(1)
        if b.u(1):
            b.u(3), b.u(1)
            if b.u(1):
                b.u(8), b.u(8), b.u(8)
        if b.u(1):
            b.ue(), b.ue()
        if b.u(1):
            n, s = b.u(32), b.u(32)
            r["fps"] = round(s / (2 * n), 3) if n else None
            b.u(1)
        hrd = 0
        for _ in range(2):
            if b.u(1):
                hrd = 1
                cnt = b.ue() + 1
                b.u(4), b.u(4)
                for _ in range(cnt):
                    b.ue(), b.ue(), b.u(1)
                b.u(5), b.u(5), b.u(5), b.u(5)
        if hrd:
            b.u(1)
        b.u(1)
        if b.u(1):
            b.u(1), b.ue(), b.ue(), b.ue(), b.ue()
            r["max_num_reorder_frames"] = b.ue()
            r["max_dec_frame_buffering"] = b.ue()
    return r


def pps(data):
    b = Bits(data[1:])
    b.ue(), b.ue()
    r = {"cabac": b.u(1), "bottom_field_pic_order_present": b.u(1)}
    if b.ue():
        r["slice_groups"] = "yes (unsupported here)"
        return r
    r["num_ref_idx_l0"] = b.ue() + 1
    r["num_ref_idx_l1"] = b.ue() + 1
    r["weighted_pred"] = b.u(1)
    r["weighted_bipred_idc"] = b.u(2)
    r["init_qp"] = 26 + b.se()
    b.se()
    r["chroma_qp_offset"] = b.se()
    r["deblocking_control"] = b.u(1)
    r["constrained_intra"] = b.u(1)
    r["redundant_pic_cnt"] = b.u(1)
    try:
        r["transform_8x8"] = b.u(1)
    except IndexError:
        r["transform_8x8"] = 0
    return r


def find_box(data, name):
    i = data.find(name)
    return i - 4 if i >= 4 else -1


def main():
    keys = nx.load_keys(sys.argv[1])
    prefix = sys.argv[3] if len(sys.argv) > 3 else "/nx/movie/eng/"
    f = open(sys.argv[2], "rb")
    raw = lambda o, s: (f.seek(o), f.read(s))[1]
    files = nx.parse_pfs0(raw, 0)
    tk = {}
    for name, off, size in files:
        if name.endswith(".tik"):
            t = raw(off, size)
            tk[t[0x140 + 0x160:0x140 + 0x170]] = t[0x140 + 0x40:0x140 + 0x50]
    for name, off, size in files:
        if not name.endswith(".nca") or name.endswith(".cnmt.nca"):
            continue
        nca = nx.Nca(f, off, size, keys, tk)
        if nca.content_type != 0:
            continue
        sec = next(s for s in nca.sections if s.fs_type == 0)
        lst, read = romfs_files(sec)
        for p, o, s in sorted(lst):
            if not p.startswith(prefix) or not p.endswith(".mp4"):
                continue
            # moov is usually at the end of these files; read both ends.
            head, tail = read(o, min(s, 1 << 20)), read(o + max(0, s - (4 << 20)), min(s, 4 << 20))
            data = head if b"avcC" in head else tail
            i = data.find(b"avcC")
            if i < 0:
                print(f"{p}: no avcC")
                continue
            c = data[i + 4:]
            nsps = c[5] & 0x1F
            q = 6
            sp = []
            for _ in range(nsps):
                n = struct.unpack(">H", c[q:q + 2])[0]
                sp.append(c[q + 2:q + 2 + n])
                q += 2 + n
            npps = c[q]
            q += 1
            pp = []
            for _ in range(npps):
                n = struct.unpack(">H", c[q:q + 2])[0]
                pp.append(c[q + 2:q + 2 + n])
                q += 2 + n
            st = data.find(b"stss")
            sync = struct.unpack(">I", data[st + 12:st + 16])[0] if st > 0 else None
            sz = data.find(b"stsz")
            count = struct.unpack(">I", data[sz + 12:sz + 16])[0] if sz > 0 else None
            ctts = b"ctts" in data
            print(f"{p}  {s / 2**20:.1f} MiB  samples={count} sync={sync} ctts(B-frames)={'yes' if ctts else 'no'}")
            print(f"    SPS {sps(sp[0])}")
            print(f"    PPS {pps(pp[0])}")


if __name__ == "__main__":
    main()
