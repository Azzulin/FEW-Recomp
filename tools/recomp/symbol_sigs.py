"""Find the symbols named in tools/recomp/symbols.txt in another build of the game's code.

    python -m recomp.symbol_sigs --exefs data/update/exefs --functable <generated>/functable.c

symbols.txt holds addresses in the tested US update 1.5.0. Another release can carry the same code at other
addresses: the Japanese update's main is the US one moved by a few bytes (GitHub issue #1). gen_main checks every
named symbol against tools/recomp/symbols.sig (written by this tool from the US ExeFS) and, when the code at its
address is not the code it was named in, looks for that code in the module; a symbol it cannot find is left out,
and so is its hook (the runtime builds without it).

A signature is hashes and sizes only, no game bytes: the size of the function that holds the symbol, the symbol's
offset in it, a CRC-32 of its first four instructions and a SHA-256 of its whole body, both with PC-relative
fields and ADRP offsets masked (remap_symbols.normalize), and how many places in the module have that body (and
which of them it is). A data symbol is found through the n-th ADRP reference of a function whose body is unique,
plus a small offset (off=) when the code reaches it from a structure base next to it.
"""
import argparse
import bisect
import hashlib
import os
import re
import struct
import sys
import zlib
from array import array

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from recomp.remap_symbols import data_refs, normalize  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
SIG_PATH = os.path.join(HERE, "symbols.sig")
HEAD = 4  # instructions in the quick filter
MAX_DATA_OFF = 0x40  # a data symbol may sit this far from the reference it is found through


def loose(w):
    """normalize() for one instruction, with the ADD/LDR/STR immediates masked whether or not an ADRP came before:
    two bodies equal under normalize() are equal under this mask too, so it can filter candidates."""
    if (w & 0x7C000000) == 0x14000000: return w & 0xFC000000
    if (w & 0xFF000010) == 0x54000000: return w & 0xFF00001F
    if (w & 0x7E000000) == 0x34000000: return w & 0xFF00001F
    if (w & 0x7E000000) == 0x36000000: return w & 0xFFF8001F
    if (w & 0x1F000000) == 0x10000000: return w & 0x9F00001F
    if (w & 0x3B000000) == 0x18000000: return w & 0xFF00001F
    if (w & 0x7F800000) == 0x11000000: return w & 0xFFC003FF
    if (w & 0x3B000000) == 0x39000000: return w & 0xFFC003FF
    return w


def body_hash(img, at, size):
    ws = normalize(struct.unpack_from(f"<{size // 4}I", img, at))
    return hashlib.sha256(struct.pack(f"<{len(ws)}I", *ws)).hexdigest()[:32]


def head_crc(img, at, size):
    n = min(size // 4, HEAD)
    return zlib.crc32(struct.pack(f"<{n}I", *(loose(w) for w in struct.unpack_from(f"<{n}I", img, at))))


class Text:
    """The code of one module (img: its mapped image; lo, hi: offsets of its text), with every place whose first
    instructions match a wanted head CRC found in one pass."""

    def __init__(self, img, lo, hi):
        self.img, self.lo, self.hi = img, lo, hi
        self._loose = None

    def find(self, heads):
        """{head CRC: [offsets]} for the CRCs in heads (each is (crc, instructions))."""
        if self._loose is None:
            self._loose = array("I", (loose(w) for w in struct.unpack_from(f"<{(self.hi - self.lo) // 4}I",
                                                                          self.img, self.lo))).tobytes()
        buf, out = self._loose, {}
        for n in {n for _, n in heads}:
            want = {c for c, k in heads if k == n}
            for i in range(0, len(buf) - 4 * n + 1, 4):
                c = zlib.crc32(buf[i:i + 4 * n])
                if c in want:
                    out.setdefault((c, n), []).append(self.lo + i)
        return out

    def places(self, sig, found):
        """Offsets in this text holding the body sig describes, in address order."""
        size = sig["size"]
        return [at for at in found.get((sig["head"], min(size // 4, HEAD)), [])
                if at + size <= self.hi and body_hash(self.img, at, size) == sig["body"]]


# ---------------------------------------------------------------- symbols.txt and symbols.sig

def read_symbols(path):
    """tools/recomp/symbols.txt: `<addr> <Name> [func|data] [hook]  # note` per line -> list of
    (addr, name, is_func, is_hook). Names must be C identifiers and unique; so must addresses."""
    out, names, addrs = [], set(), set()
    if not os.path.exists(path):
        return out
    with open(path, encoding="utf-8") as f:
        lines = f.read().splitlines()
    for n, ln in enumerate(lines, 1):
        ln = ln.split("#")[0].strip()
        if not ln:
            continue
        parts = ln.split()
        if len(parts) < 2 or not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", parts[1]):
            raise SystemExit(f"{path}:{n}: expected `<addr> <Name> [func|data] [hook]`")
        addr, name, attrs = int(parts[0], 16), parts[1], set(parts[2:])
        if attrs - {"func", "data", "hook"}:
            raise SystemExit(f"{path}:{n}: unknown attribute in {sorted(attrs)}")
        is_func = "data" not in attrs
        if "hook" in attrs and not is_func:
            raise SystemExit(f"{path}:{n}: a data symbol cannot be a hook")
        if name in names or addr in addrs:
            raise SystemExit(f"{path}:{n}: duplicate symbol {name} / {addr:#x}")
        names.add(name)
        addrs.add(addr)
        out.append((addr, name, is_func, "hook" in attrs))
    return out


def read_sigs(path=SIG_PATH):
    """{name: {module, kind, size, off, head, body, count, rank, at, ref}} from symbols.sig."""
    out = {}
    if not os.path.exists(path):
        return out
    with open(path, encoding="utf-8") as f:
        lines = f.read().splitlines()
    for n, ln in enumerate(lines, 1):
        ln = ln.split("#")[0].split()
        if not ln:
            continue
        try:
            sig = dict(kv.split("=", 1) for kv in ln[1:])
            sig = {"module": sig["module"], "kind": sig["kind"], "at": int(sig["at"], 16),
                   "size": int(sig["size"], 16), "off": int(sig["off"], 16), "head": int(sig["head"], 16),
                   "body": sig["body"], "count": int(sig["count"]), "rank": int(sig["rank"]),
                   "ref": int(sig["ref"]) if "ref" in sig else None}
        except (KeyError, ValueError) as e:
            raise SystemExit(f"{path}:{n}: bad signature ({e})")
        out[ln[0]] = sig
    return out


def relocate(proc, symbols, sigs, log):
    """symbols (read_symbols) with each address checked against its signature and, where the code moved,
    replaced by where it is in this build. Symbols that cannot be found are dropped (with their hooks)."""
    texts, checks, out = {}, [], {}
    by_name = {m.name: m for m in proc.modules}

    def text(mod):
        if mod not in texts:
            m = by_name[mod]
            texts[mod] = Text(m.image, m.text_off, m.text_off + m.text_size)
        return texts[mod]

    def holds(sig, start):
        """Whether the owner function of sig sits at module offset start, and the symbol's offset if so."""
        t = text(sig["module"])
        if start < t.lo or start + sig["size"] > t.hi or body_hash(t.img, start, sig["size"]) != sig["body"]:
            return None
        if sig["kind"] == "func":
            return start + sig["off"]
        refs = data_refs(t.img, start, sig["size"])
        return refs[sig["ref"]] + sig["off"] if sig["ref"] < len(refs) else None

    for sym in symbols:
        addr, name = sym[0], sym[1]
        sig = sigs.get(name)
        if sig is None or sig["module"] not in by_name:
            out[name] = sym  # no signature: taken as written
            continue
        base = by_name[sig["module"]].base
        start = (addr - sig["off"] if sig["kind"] == "func" else sig["at"]) - base
        got = holds(sig, start)
        if got is not None and got + base == addr:
            out[name] = sym
        else:
            checks.append((sym, sig))
    if not checks:
        return list(symbols)
    moved, missing = [], []
    for mod in {sig["module"] for _, sig in checks}:
        group = [(sym, sig) for sym, sig in checks if sig["module"] == mod]
        t = text(mod)
        found = t.find({(sig["head"], min(sig["size"] // 4, HEAD)) for _, sig in group})
        base = by_name[mod].base
        for sym, sig in group:
            places = t.places(sig, found)
            got = holds(sig, places[sig["rank"]]) if len(places) == sig["count"] else None
            if got is None:
                missing.append(sym)
                continue
            out[sym[1]] = (got + base,) + tuple(sym[1:])
            moved.append((sym[1], sym[0], got + base))
    for name, old, new in sorted(moved, key=lambda x: x[2]):
        log(f"symbols: {name} {old:#x} -> {new:#x} (same code, moved in this build)")
    for sym in missing:
        log(f"symbols: WARNING: {sym[1]} ({sym[0]:#x} in the US update) not found in this build: "
            + ("its hook is left out" if sym[3] else "left out"))
    taken = {}
    for sym in out.values():  # two names can't land on one address
        if sym[0] in taken:
            raise SystemExit(f"symbols: {sym[1]} and {taken[sym[0]]} both found at {sym[0]:#x}")
        taken[sym[0]] = sym[1]
    return [out[s[1]] for s in symbols if s[1] in out]


# ---------------------------------------------------------------- writing symbols.sig (from the US ExeFS)

def make(proc, symbols, starts, log=print):
    """Signature lines for symbols; starts: every recompiled function start (functable.c)."""
    starts = sorted(starts)
    lines, texts = [], {}

    def sig_of(m, start, end):
        t = texts.setdefault(m.name, Text(m.image, m.text_off, m.text_off + m.text_size))
        size = end - start
        sig = {"module": m.name, "size": size, "head": head_crc(m.image, start, size),
               "body": body_hash(m.image, start, size)}
        places = t.places(sig, t.find({(sig["head"], min(size // 4, HEAD))}))
        return dict(sig, count=len(places), rank=places.index(start))

    def func_bounds(m, a):
        i = bisect.bisect_right(starts, a) - 1
        end = starts[i + 1] if i + 1 < len(starts) else m.base + m.text_off + m.text_size
        return starts[i], min(end, m.base + m.text_off + m.text_size)

    for addr, name, is_func, _ in symbols:
        m = proc.module_at(addr)
        if is_func:
            start, end = func_bounds(m, addr)
            sig = sig_of(m, start - m.base, end - m.base)
            lines.append((name, "func", start, addr - start, sig, None))
            continue
        # A data symbol: the first function with a unique body whose ADRP references reach it, or reach the
        # closest place before or after it (a structure base the code adds a small offset to).
        lo, hi = m.base + m.text_off, m.base + m.text_off + m.text_size
        best = None
        for st in (s for s in starts if lo <= s < hi):
            start, end = func_bounds(m, st)
            refs = data_refs(m.image, start - m.base, end - start)
            near = [(abs(addr - m.base - r), k, addr - m.base - r) for k, r in enumerate(refs)
                    if abs(addr - m.base - r) <= MAX_DATA_OFF]
            if not near or (best and min(near)[0] >= best[0]):
                continue
            sig = sig_of(m, start - m.base, end - m.base)
            if sig["count"] == 1:
                dist, k, off = min(near)
                best = (dist, (name, "data", start, off, sig, k))
                if dist == 0:
                    break
        if best is None:
            raise SystemExit(f"{name} {addr:#x}: no function with a unique body references it")
        lines.append(best[1])
    out = []
    for name, kind, at, off, s, ref in lines:
        out.append(f"{name:<28} module={s['module']} kind={kind} at={at:#x} size={s['size']:#x} off={off:#x} "
                   f"head={s['head']:08x} body={s['body']} count={s['count']} rank={s['rank']}"
                   + (f" ref={ref}" if ref is not None else ""))
        if s["count"] > 1:
            log(f"{name}: its function's body appears {s['count']} times; found by order")
    return out


def main():
    from nso import Process
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--exefs", required=True, help="the tested US update's ExeFS")
    ap.add_argument("--functable", required=True, help="generated/functable.c built from that ExeFS")
    ap.add_argument("--symbols", default=os.path.join(HERE, "symbols.txt"))
    ap.add_argument("--out", default=SIG_PATH)
    a = ap.parse_args()
    proc = Process(a.exefs)
    starts = {int(x, 16) for x in re.findall(r"\{ 0x([0-9A-Fa-f]+)ULL", open(a.functable, encoding="utf-8").read())}
    lines = make(proc, read_symbols(a.symbols), starts)
    header = ["# Signatures of the symbols in symbols.txt (hashes and sizes only, no game bytes), written by",
              "# tools/recomp/symbol_sigs.py from the US update 1.5.0. gen_main uses them to find each symbol in",
              "# another build of the code (the Japanese release, GitHub issue #1). Regenerate after editing symbols.txt."]
    with open(a.out, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(header + lines) + "\n")
    print(f"wrote {len(lines)} signatures to {a.out}")


if __name__ == "__main__":
    main()
