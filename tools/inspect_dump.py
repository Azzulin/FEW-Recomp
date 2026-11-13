#!/usr/bin/env python3
"""Read-only census of the user's own NSPs (base game, update, add-on content).

    python tools/inspect_dump.py <prod.keys> <out_dir> <a.nsp> [<b.nsp> ...]

For each NSP: its NCAs (content type, title, SDK, key generation), the CNMT, the NACP, the ExeFS file list and
a RomFS summary. Program ExeFS files are written to <out_dir>/<title>/exefs (the recompiler's input) and the full
RomFS file list to <out_dir>/<title>/romfs_<kind>.txt. Key values are never printed or written.
"""
import collections
import os
import struct
import sys

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import nx_extract as nx  # noqa: E402

CNMT_TYPES = {0x80: "Application", 0x81: "Patch", 0x82: "AddOnContent", 0x83: "Delta"}
LANGS = ["en-US", "en-GB", "ja", "fr", "de", "es-419", "es", "it", "nl", "fr-CA", "pt", "ru", "ko", "zh-TW", "zh-CN",
         "pt-BR"]


def ver_str(v):
    return f"{v >> 26}.{(v >> 20) & 0x3F}.{(v >> 16) & 0xF}.{v & 0xFFFF}"


def sdk_str(v):
    return f"{v >> 24}.{(v >> 16) & 255}.{(v >> 8) & 255}"


def romfs_files(sec):
    """[(path, offset in the RomFS image, size)] of an AES-CTR or plain RomFS section."""
    data_off, _ = sec.data_region()
    read = lambda o, s: sec.read(data_off + o, s)
    v = struct.unpack_from("<10Q", read(0, 0x50), 0)
    dir_meta, file_meta = read(v[3], v[4]), read(v[7], v[8])
    NONE = 0xFFFFFFFF
    out, stack = [], [(0, "")]
    while stack:
        do, path = stack.pop()
        _p, _s, child_dir, child_file, _h, nlen = struct.unpack_from("<IIIIII", dir_meta, do)
        name = dir_meta[do + 0x18:do + 0x18 + nlen].decode("utf-8", "replace")
        p = f"{path}/{name}" if name else path
        fo = child_file
        while fo != NONE:
            _pp, sib, doff, size, _hh, fl = struct.unpack_from("<IIQQII", file_meta, fo)
            out.append((f"{p}/{file_meta[fo + 0x20:fo + 0x20 + fl].decode('utf-8', 'replace')}", v[9] + doff, size))
            fo = sib
        cd = child_dir
        while cd != NONE:
            stack.append((cd, p))
            cd = struct.unpack_from("<I", dir_meta, cd + 4)[0]
    return out, read


def summarize(files, indent="    "):
    total = sum(s for _, _, s in files)
    print(f"{indent}files={len(files)} total={total / 2**30:.2f} GiB")
    ext, extn, top, topn = collections.Counter(), collections.Counter(), collections.Counter(), collections.Counter()
    for p, _, s in files:
        e = os.path.splitext(p)[1].lower() or "(none)"
        ext[e] += s
        extn[e] += 1
        parts = p.split("/")
        t = "/".join(parts[1:4]) if len(parts) > 4 else "/".join(parts[1:-1]) or "/"
        top[t] += s
        topn[t] += 1
    print(f"{indent}by extension: " + ", ".join(f"{e} {extn[e]}x {n / 2**20:,.0f}M" for e, n in ext.most_common(12)))
    print(f"{indent}by folder:")
    for t, n in top.most_common(30):
        print(f"{indent}  {n / 2**20:>9,.1f} MiB  n={topn[t]:>5}  {t}")


def main():
    keys_path, out = sys.argv[1], sys.argv[2]
    keys = nx.load_keys(keys_path)
    for nsp in sys.argv[3:]:
        print("=" * 100)
        print(os.path.basename(nsp), f"({os.path.getsize(nsp) / 2**30:.2f} GiB)")
        f = open(nsp, "rb")
        raw = lambda o, s: (f.seek(o), f.read(s))[1]
        files = nx.parse_pfs0(raw, 0)
        title_keys = {}
        for name, off, size in files:
            if name.endswith(".tik"):
                tik = raw(off, size)
                body = {0x10000: 0x240, 0x10001: 0x140, 0x10002: 0x80, 0x10003: 0x240, 0x10004: 0x140,
                        0x10005: 0x80}[struct.unpack_from("<I", tik, 0)[0]]
                title_keys[tik[body + 0x160:body + 0x170]] = tik[body + 0x40:body + 0x50]
        print(f"  entries={len(files)} tickets={len(title_keys)}")
        # Output folder per NSP: the CNMT's title ID (an update's program NCA has the base title ID, its CNMT the
        # update's), so base and update ExeFS never land in the same folder.
        ncas = []
        for name, off, size in files:
            if not name.endswith(".nca"):
                continue
            try:
                ncas.append((name, size, nx.Nca(f, off, size, keys, title_keys)))
            except Exception as e:  # noqa: BLE001
                print(f"  {name}: cannot open ({type(e).__name__}: {str(e)[:60]})")
        cnmt_tid = next((n.title_id for _, _, n in ncas if n.content_type == 1), None)
        tdir = os.path.join(out, f"{cnmt_tid if cnmt_tid is not None else ncas[0][2].title_id:016x}")
        for name, size, nca in ncas:
            kind = nx.CONTENT_TYPES.get(nca.content_type, str(nca.content_type))
            print(f"  -- {name[:12]}.. {kind:<10} title={nca.title_id:016x} sdk={sdk_str(nca.sdk_version)} "
                  f"keygen={nca.mkrev + 1} rights_id={'yes' if any(nca.hdr[0x230:0x240]) else 'no'} "
                  f"size={size / 2**20:,.1f} MiB")
            for sec in nca.sections:
                fs = {0: "RomFS", 1: "PFS0"}.get(sec.fs_type, sec.fs_type)
                enc = {1: "none", 2: "xts", 3: "ctr", 4: "ctr-ex(BKTR)"}.get(sec.enc_type, sec.enc_type)
                print(f"     section {sec.idx}: {fs} enc={enc} {(sec.end - sec.start) / 2**20:,.1f} MiB")
                if sec.enc_type not in (1, 3):
                    continue
                if sec.fs_type == 1:
                    doff, _ = sec.data_region()
                    pfs = nx.parse_pfs0(lambda o, s: sec.read(doff + o, s), 0)
                    print("       " + ", ".join(f"{fn} ({fsz:,})" for fn, _, fsz in pfs))
                    if kind == "program" and any(fn == "main" for fn, _, _ in pfs):
                        d = os.path.join(tdir, "exefs")
                        os.makedirs(d, exist_ok=True)
                        for old in os.listdir(d):  # never mix files of two ExeFS
                            os.remove(os.path.join(d, old))
                        for fn, foff, fsz in pfs:
                            with open(os.path.join(d, fn), "wb") as o:
                                o.write(sec.read(doff + foff, fsz))
                    if kind == "meta":
                        for fn, foff, fsz in pfs:
                            c = sec.read(doff + foff, fsz)
                            tid, ver, mtype = struct.unpack_from("<QIB", c, 0)
                            ext_sz = struct.unpack_from("<H", c, 0x0E)[0]
                            extra = ""
                            if mtype in (0x80, 0x81, 0x82) and ext_sz >= 0x10:
                                other, req = struct.unpack_from("<QI", c, 0x20)
                                extra = f" {'patch' if mtype == 0x80 else 'application'}={other:016x} required_version={req}"
                            print(f"       cnmt: {CNMT_TYPES.get(mtype, hex(mtype))} title={tid:016x} "
                                  f"version={ver} ({ver_str(ver)}){extra}")
                elif sec.fs_type == 0:
                    lst, read = romfs_files(sec)
                    if kind == "control":
                        for p, o, s in lst:
                            if p.endswith("control.nacp"):
                                nacp = read(o, s)
                                names = [(LANGS[i], nacp[i * 0x300:i * 0x300 + 0x200].split(b"\0")[0].decode())
                                         for i in range(16)]
                                print("       title: " + "; ".join(f"{lg}={t}" for lg, t in names if t))
                                flags = struct.unpack_from("<I", nacp, 0x302C)[0]
                                print(f"       languages: {[LANGS[i] for i in range(16) if flags >> i & 1]}")
                                print(f"       display_version={nacp[0x3060:0x3070].split(b'\\0')[0].decode()} "
                                      f"save={struct.unpack_from('<Q', nacp, 0x3080)[0]:#x} "
                                      f"aoc_base={struct.unpack_from('<Q', nacp, 0x3070)[0]:#x}")
                        continue
                    summarize(lst, "       ")
                    os.makedirs(tdir, exist_ok=True)
                    with open(os.path.join(tdir, f"romfs_{kind}.txt"), "w", encoding="utf-8") as o:
                        for p, _, s in sorted(lst):
                            o.write(f"{s}\t{p}\n")


if __name__ == "__main__":
    main()
