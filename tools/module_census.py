#!/usr/bin/env python3
"""Census of an ExeFS: module layout, build IDs, SHA-256, SDK middleware, imports per namespace, SVCs used.

    python tools/module_census.py <exefs_dir> [--compare <supported_games.json>]

Read-only; prints only facts about the code (no game data is written).
"""
import argparse
import collections
import hashlib
import json
import os
import re
import struct
import sys

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import nso  # noqa: E402


def ns_of(name):
    if not name.startswith("_ZN"):
        return "C/other"
    rest, parts = name[3:].lstrip("K"), []
    for _ in range(2):
        mo = re.match(r"(\d+)", rest)
        if not mo:
            break
        n = int(mo.group(1))
        parts.append(rest[len(mo.group(1)):len(mo.group(1)) + n])
        rest = rest[len(mo.group(1)) + n:]
    return "::".join(parts) or "C++/other"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("exefs")
    ap.add_argument("--compare", help="supported_games.json whose module hashes to compare against")
    a = ap.parse_args()
    proc = nso.Process(a.exefs)
    known = {}
    if a.compare:
        for prof in json.load(open(a.compare))["profiles"]:
            for mod, h in prof["modules_sha256"].items():
                known.setdefault(h, []).append(f"{prof['id']}:{mod}")
    print(f"{'module':<8} {'base':>12} {'text':>10} {'rodata':>10} {'data':>9} {'bss':>10}  build_id(16)                      name")
    total = 0
    for m in proc.modules:
        total += m.text_size
        raw = open(os.path.join(a.exefs, m.name), "rb").read()
        sha = hashlib.sha256(raw).hexdigest()
        print(f"{m.name:<8} {m.base:#12x} {m.text_size:>10,} {m.ro_size:>10,} {m.data_size:>9,} {m.bss_size:>10,}  "
              f"{m.build_id[:16].hex()}  {m.module_name}")
        print(f"         sha256={sha}" + (f"  == {', '.join(known[sha])}" if sha in known else ""))
    print(f"end of modules: {proc.end:#x}; total text {total:,} bytes ({total // 4:,} instructions)")
    unresolved = [u for u in proc.unresolved if u[0] != "rtld"]
    print(f"unresolved imports outside rtld: {len(unresolved)} {unresolved[:10]}")

    print("\nSDK middleware:")
    seen = set()
    for m in proc.modules:
        for s in re.findall(rb"SDK MW\+[ -~]{4,90}", bytes(m.image)):
            if (m.name, s) not in seen:
                seen.add((m.name, s))
                print(f"  {m.name:<8} {s.decode()}")

    print("\nimports by namespace:")
    ns = collections.defaultdict(collections.Counter)
    for (mod, name), tgt in proc.imports.items():
        if tgt is not None and tgt.module.name != mod:
            ns[mod][ns_of(name)] += 1
    for mod, c in ns.items():
        print(f"  {mod}: total={sum(c.values())}  " + ", ".join(f"{k}={v}" for k, v in c.most_common(30)))

    svc = collections.Counter()
    for m in proc.modules:
        for off in range(m.text_off, m.text_off + m.text_size, 4):
            w = struct.unpack_from("<I", m.image, off)[0]
            if (w & 0xFFE0001F) == 0xD4000001:
                svc[(w >> 5) & 0xFFFF] += 1
    print(f"\nSVCs used ({len(svc)}): " + " ".join(f"{k:#x}" for k in sorted(svc)))
    for m in proc.modules:
        print(f"  {m.name}: needed={[m.cstr(m.strtab + n).rsplit('/', 1)[-1] for n in m.needed]}")


if __name__ == "__main__":
    main()
