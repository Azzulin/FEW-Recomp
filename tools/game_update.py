"""Unpack the game's update (tools/game_profile.py: 1.5.0, title 0100F15003E64800 for the US release or
0100EAE003A5A800 for the Japanese one) from the user's own .nsp or .nsz into a private folder.

    python tools/game_update.py <update.nsp|.nsz> <prod.keys> <out dir>

The output is what the runtime and the generator need, decrypted, and nothing else:
  exefs/*                       the update's whole ExeFS. It replaces the base game's: Fire Emblem Warriors 1.5.0 was
                                rebuilt on SDK 3.7.0 with a different module set (5 modules instead of the base's 8)
  romfs.patch                   the update's RomFS section: BKTR tables plus patch data, decrypted
  update.json                   title, version, and where the tables and the patched RomFS live in romfs.patch

The RomFS section of an update is a patch over the base game's: an indirect table maps every range of the new
RomFS either to the base game's RomFS section or to patch data in this section (BKTR). In an .nsp the patch data is
encrypted per region (AES-CTR-Ex, the counter's generation changes); in an .nsz it is stored decrypted inside one
zstd stream, which cannot be read at random, hence this one-time unpacking. Nothing is written but the output
folder; keys are read in place.
"""
import json
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import nx_extract as nx
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from game_profile import REGIONS, UPDATE_ID, UPDATE_VERSION


def _tickets(files, read):
    keys = {}
    for name, off, size in files:
        if name.endswith(".tik") and size <= 1024 * 1024:
            t = read(off, size)
            body = {0x10000: 0x240, 0x10001: 0x140, 0x10002: 0x80,
                    0x10003: 0x240, 0x10004: 0x140, 0x10005: 0x80}.get(struct.unpack_from("<I", t, 0)[0])
            if body and len(t) >= body + 0x170:
                keys[t[body + 0x160:body + 0x170]] = t[body + 0x40:body + 0x50]
    return keys


def _sections(hdr):
    out = []
    for i in range(4):
        start, end = struct.unpack_from("<II", hdr, 0x240 + 16 * i)
        if end:
            out.append((i, start * 0x200, end * 0x200, hdr[0x400 + 0x200 * i:0x600 + 0x200 * i]))
    return out


def _pfs_main(read_section, fsh):
    h = fsh[8:]
    layers = struct.unpack_from("<I", h, 0x24)[0]
    doff, _ = struct.unpack_from("<QQ", h, 0x28 + 16 * (layers - 1))
    files = {n: (o, s) for n, o, s in nx.parse_pfs0(lambda o, s: read_section(doff + o, s), 0)}
    if "main" not in files:
        return None
    return {n: read_section(doff + o, s) for n, (o, s) in files.items()}


def _bucket_entries(table, entry_size, fmt):
    """Entries of a BKTR bucket tree: a 0x4000 offset node, then 0x4000-byte entry sets."""
    out = []
    for node in range(1, len(table) // 0x4000):
        base = node * 0x4000
        _, count, _ = struct.unpack_from("<iiq", table, base)
        for k in range(count):
            out.append(struct.unpack_from(fmt, table, base + 16 + entry_size * k))
    return out


def unpack(path, keys_path, out_dir):
    path, out_dir = Path(path), Path(out_dir)
    keys = nx.load_keys(keys_path)
    f = path.open("rb")
    read = lambda o, s: (f.seek(o), f.read(s))[1]
    files = nx.parse_pfs0(read, 0)
    tk = _tickets(files, read)
    # The release comes from the ticket's rights ID (title ID in its first 8 bytes): US or JP (game_profile.REGIONS).
    region = next((r for r, pid in REGIONS.items() if any(k[:8] == (pid | 0x800).to_bytes(8, "big") for k in tk)), None)
    if region is None:
        raise ValueError(f"Not the game's update: no ticket for title {UPDATE_ID:016X}")
    program_id = REGIONS[region]
    update_id = program_id | 0x800
    program = None
    for name, off, size in files:
        if name.endswith(".cnmt.nca") or not (name.endswith(".nca") or name.endswith(".ncz")):
            continue
        hdr = nx.xts_dec_sectors(keys["header_key"], read(off, 0xC00), 0)
        if hdr[0x205] == 0 and struct.unpack_from("<Q", hdr, 0x210)[0] == program_id:
            program = (name, off, size, hdr)
    if not program:
        raise ValueError("The update has no program content")
    version = _version(f, files, keys, tk, update_id)
    if version != UPDATE_VERSION:
        raise ValueError(f"This is update v{version}; the generated code is built from update 1.5.0 "
                         f"(v{UPDATE_VERSION}). Use that update.")
    name, off, size, hdr = program
    secs = _sections(hdr)
    if name.endswith(".ncz"):
        import zstandard  # BSD-3-Clause; only needed for .nsz
        count = struct.unpack_from("<Q", read(off + 0x4008, 8), 0)[0]
        if read(off + 0x4000, 8) != b"NCZSECTN" or count > 16:
            raise ValueError("Unsupported .nsz layout")
        p = off + 0x4010 + 0x40 * count
        if read(p, 8) == b"NCZBLOCK":
            raise ValueError("Block-compressed .nsz is not supported; use a solid .nsz or the .nsp")
        d = zstandard.ZstdDecompressor().decompressobj()
        parts = [bytes(0x4000)]
        while p < off + size:
            chunk = read(p, min(1 << 22, off + size - p))
            p += len(chunk)
            parts.append(d.decompress(chunk))
        body = b"".join(parts)  # NCA bytes from 0x4000 on, decrypted
        plain = lambda start, end: body[start:end]
    else:
        nca = nx.Nca(f, off, size, keys, tk)
        ctr_key = nca.ctr_key
        def plain(start, end, fsh=None):
            return _decrypt_section(read, off, start, end, fsh, ctr_key)
    exefs = romfs = None
    for idx, start, end, fsh in secs:
        if fsh[2] == 1 and exefs is None:
            data = plain(start, end) if name.endswith(".ncz") else plain(start, end, fsh)
            exefs = _pfs_main(lambda o, s, d=data: d[o:o + s], fsh)
        elif fsh[2] == 0 and fsh[4] == 4:
            romfs = (start, end, fsh, plain(start, end) if name.endswith(".ncz") else plain(start, end, fsh))
    if not exefs or not romfs:
        raise ValueError("The update lacks its ExeFS or RomFS patch")
    start, end, fsh, patch = romfs
    info = fsh[0x100:0x140]
    ind_off, ind_size, magic, _, ind_count = struct.unpack_from("<QQ4sII", info, 0)
    if magic != b"BKTR":
        raise ValueError("RomFS patch without BKTR header")
    lvl_off, lvl_size = struct.unpack_from("<QQ", fsh[8:], 0x10 + 0x18 * 5)
    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / "exefs").mkdir(exist_ok=True)
    for old in (out_dir / "exefs").iterdir():  # never mix files of two ExeFS
        old.unlink()
    for n, data in exefs.items():
        (out_dir / "exefs" / n).write_bytes(data)
    (out_dir / "romfs.patch").write_bytes(patch)
    meta = {"schema": 1, "title_id": f"{update_id:016x}", "region": region, "version": version,
            "indirect_offset": ind_off, "indirect_size": ind_size, "indirect_count": ind_count,
            "romfs_offset": lvl_off, "romfs_size": lvl_size, "patch_size": len(patch)}
    (out_dir / "update.json").write_text(json.dumps(meta, indent=1))
    return meta


def _decrypt_section(read, nca_off, start, end, fsh, ctr_key):
    """Decrypt one NCA section of an .nsp: plain AES-CTR, or AES-CTR-Ex for an update's RomFS patch."""
    ctr_hi = fsh[0x140:0x148][::-1]
    raw = read(nca_off + start, end - start)
    def ctr_crypt(data, rel, upper):
        abs_off = start + rel
        c = upper + (abs_off >> 4).to_bytes(8, "big")
        return Cipher(algorithms.AES(ctr_key), modes.CTR(c)).decryptor().update(data)
    if fsh[4] == 3:
        return ctr_crypt(raw, 0, ctr_hi)
    if fsh[4] != 4:
        raise ValueError(f"Unsupported section encryption {fsh[4]}")
    info = fsh[0x100:0x140]
    ex_off, ex_size = struct.unpack_from("<QQ", info, 0x20)
    out = bytearray(len(raw))
    # The bucket tables (from the indirect table on) use the section's own counter.
    ind_off = struct.unpack_from("<Q", info, 0)[0]
    out[ind_off:] = ctr_crypt(raw[ind_off:], ind_off, ctr_hi)
    entries = _bucket_entries(bytes(out[ex_off:ex_off + ex_size]), 0x10, "<QII")
    for k, (o, _, generation) in enumerate(entries):
        e = entries[k + 1][0] if k + 1 < len(entries) else ind_off
        upper = ctr_hi[:4] + generation.to_bytes(4, "big")
        out[o:e] = ctr_crypt(raw[o:e], o, upper)
    return bytes(out)


def _version(f, files, keys, tk, update_id):
    """Title version from the update's content meta (PackagedContentMeta: title id at 0, version at 8)."""
    for name, off, size in files:
        if name.endswith(".cnmt.nca"):
            nca = nx.Nca(f, off, size, keys, tk)
            sec = nca.sections[0]
            doff, _ = sec.data_region()
            for fn, foff, fsz in nx.parse_pfs0(lambda o, s: sec.read(doff + o, s), 0):
                if fn.endswith(".cnmt"):
                    tid, version = struct.unpack_from("<QI", sec.read(doff + foff, 12), 0)
                    if tid == update_id:
                        return version
    raise ValueError("The update's content meta is missing")


def explain(error):
    """A failure of unpack() in words a player can act on (the build scripts print this instead of a traceback)."""
    if isinstance(error, KeyError):
        missing = str(error.args[0]) if error.args else "?"
        if missing.startswith("missing title key"):
            return ("The update NSP has no ticket (title key) for its content. Dump the update again as an .nsp with "
                    "its ticket.")
        return (f"prod.keys has no '{missing}'. Dump prod.keys again from your console, on a firmware at least as "
                "new as the one that installed the update, and put it in place of the old one.")
    if isinstance(error, FileNotFoundError):
        return f"File not found: {error.filename}"
    if isinstance(error, MemoryError):
        return "Not enough free memory to unpack the update; close other programs and run the build again."
    text = str(error)
    if text.startswith("Not the game's update"):
        ids = " or ".join(f"{pid | 0x800:016X} ({r})" for r, pid in REGIONS.items())
        return (f"This file is not the update 1.5.0 of Fire Emblem Warriors: expected the update {ids}; other "
                "releases are not supported.")
    if "PFS0" in text or "NCA3" in text:
        return f"This file is not a readable NSP ({text}). It may be damaged or incomplete; copy it again."
    return f"{type(error).__name__}: {text}" if text else type(error).__name__


def main(argv):
    """Unpacks like unpack(), printing a plain reason on failure; returns the exit code. Used by the build scripts."""
    try:
        meta = unpack(*argv[:3])
    except Exception as e:  # noqa: BLE001 - every failure becomes one readable line
        print(f"update: {explain(e)}", file=sys.stderr, flush=True)
        return 1
    print(f"update: {meta['region']} release, v{meta['version']} unpacked", flush=True)
    print(f"update: {code_check(Path(argv[2]) / 'exefs')}", flush=True)
    return 0


def code_check(exefs):
    """Whether the unpacked ExeFS is byte for byte the tested one (tools/supported_games.json). Another release can
    carry the same code (then nothing changes but the IDs and the language); if not, it is built but untested."""
    import hashlib
    manifest = Path(__file__).resolve().parent / "supported_games.json"
    known = json.loads(manifest.read_text(encoding="utf-8"))["profiles"][0]["modules_sha256"]
    got = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(Path(exefs).iterdir())
           if p.name in known}
    same = [n for n in known if got.get(n) == known[n]]
    if len(same) == len(known):
        return "code identical to the tested US update 1.5.0"
    differs = ", ".join(n for n in known if n not in same)
    return (f"code differs from the tested US update 1.5.0 in {differs}: it is built anyway, but untested. Please "
            "report how it runs (GitHub issue #1).")


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:4]))
