#!/usr/bin/env python3
"""The pipeline catalog: what lets the game prepare its graphics before the first frame instead of in the middle of a
battle. A catalog holds, for each shader program the game used, where its code is in the game's own files (RomFS file
name, offset, size, SHA-256) and the pipeline states it was drawn with. No shader code, no game bytes: the game reads
the code from the player's own copy, checks the SHA-256 and translates it again (runtime/src/gpu/vk/pipeline_catalog.*,
docs/pipeline-cache.md). Format "HWDCAT01" schema 1, written by the runtime; this module reads, merges and writes it.

    python tools/pipeline_catalog/catalog.py info  <catalog>...
    python tools/pipeline_catalog/catalog.py merge <out> <catalog>...   union of recordings (same build ABI and game)
    python tools/pipeline_catalog/catalog.py check <catalog>            exit 1 if it is not a valid catalog

Recording: run the game with HWDER_PIPELINE_CATALOG_RECORD=<what this run covers>; it writes pipeline_catalog.bin in
its folder. Each run replaces that file, so merge the runs into the shipped catalog (CATALOG below), which the build
scripts copy next to the game as pipeline_catalog.bin.
"""
import collections
import hashlib
from pathlib import Path
import struct
import sys

CATALOG = Path(__file__).resolve().parent / "few-us-1.5.0.bin"  # the one the builds ship (US update 1.5.0)
MAGIC = b"HWDCAT01"
SCHEMA = 1
STAGES = 5
KEY_WORDS = 229            # warmup::key_words: the program key (2 words) and the fixed pipeline state
MAX_PROGRAMS = 512         # warmup::kMaxPrograms
MAX_PIPELINES = 8192       # warmup::kMaxPipelines
MAX_SCOPE = 128


class CatalogError(ValueError):
    pass


class Reader:
    def __init__(self, data, pos):
        self.data, self.pos = data, pos

    def take(self, n):
        if self.pos + n > len(self.data):
            raise CatalogError("truncated")
        out = self.data[self.pos:self.pos + n]
        self.pos += n
        return out

    def u32(self):
        return struct.unpack("<I", self.take(4))[0]

    def u64(self):
        return struct.unpack("<Q", self.take(8))[0]

    def string(self, limit):
        n = self.u32()
        if n > limit:
            raise CatalogError("string too long")
        return self.take(n).decode("utf-8", errors="strict")


def _stage(r):
    """One shader stage, returned as its raw bytes (identity) and its resource name (or None if absent)."""
    start = r.pos
    present = r.u32()
    if present > 1:
        raise CatalogError("bad flag")
    resource = None
    if present:
        resource = r.string(256)
        r.take(8 + 4 + 32 + 8 + 4 + 4)   # offset, size, sha256, attributes, render targets, bound buffer
        for _ in range(3):                 # flip_y, convert_depth_mode, picture_controls
            if r.u32() > 1:
                raise CatalogError("bad flag")
        r.take(4 + 4)                      # bloom cbuf, bloom offset
    return r.data[start:r.pos], resource


def parse(data):
    """A catalog file's contents: {build, title, version, scope, programs: [{key, raw, resources}], pipelines:
    [(program index, key bytes)]}. Raises CatalogError when the file is not a valid catalog."""
    if len(data) < 76 or data[:8] != MAGIC:
        raise CatalogError("not a pipeline catalog")
    body, digest = data[:-32], data[-32:]
    if hashlib.sha256(body).digest() != digest:
        raise CatalogError("checksum mismatch (damaged file)")
    r = Reader(body, 8)
    if r.u32() != SCHEMA:
        raise CatalogError("unknown schema")
    build, title, version = r.u64(), r.u64(), r.u32()
    scope = r.string(MAX_SCOPE)
    n_programs, n_pipelines = r.u32(), r.u32()
    if n_programs > MAX_PROGRAMS or n_pipelines > MAX_PIPELINES:
        raise CatalogError("too many entries")
    programs = []
    for _ in range(n_programs):
        start = r.pos
        key = r.u64()
        r.u64()  # expected translation
        resources = [res for _, res in (_stage(r) for _ in range(STAGES)) if res]
        programs.append({"key": key, "raw": body[start:r.pos], "resources": resources})
    pipelines = []
    for _ in range(n_pipelines):
        index = r.u32()
        key = r.take(KEY_WORDS * 4)
        if index >= n_programs or struct.unpack_from("<Q", key, 0)[0] != programs[index]["key"]:
            raise CatalogError("pipeline names a missing program")
        pipelines.append((index, key))
    if r.pos != len(body):
        raise CatalogError("trailing bytes")
    return {"build": build, "title": title, "version": version, "scope": scope, "programs": programs,
            "pipelines": pipelines}


def encode(cat):
    out = bytearray(MAGIC)
    out += struct.pack("<IQQI", SCHEMA, cat["build"], cat["title"], cat["version"])
    scope = cat["scope"].encode("utf-8")
    if not scope or len(scope) > MAX_SCOPE:
        raise CatalogError("scope must be 1-128 bytes")
    out += struct.pack("<I", len(scope)) + scope
    if len(cat["programs"]) > MAX_PROGRAMS or len(cat["pipelines"]) > MAX_PIPELINES:
        raise CatalogError(f"too many entries ({len(cat['programs'])} programs, {len(cat['pipelines'])} pipelines; "
                           f"the runtime takes {MAX_PROGRAMS} and {MAX_PIPELINES})")
    out += struct.pack("<II", len(cat["programs"]), len(cat["pipelines"]))
    for p in cat["programs"]:
        out += p["raw"]
    for index, key in cat["pipelines"]:
        out += struct.pack("<I", index) + key
    return bytes(out) + hashlib.sha256(out).digest()


def merge(catalogs):
    """The union of several catalogs of the same build ABI, title and game version, in first-seen order."""
    if not catalogs:
        raise CatalogError("nothing to merge")
    first = catalogs[0]
    for c in catalogs[1:]:
        if (c["build"], c["title"], c["version"]) != (first["build"], first["title"], first["version"]):
            raise CatalogError("catalogs from different builds ABIs, titles or game versions cannot be merged")
    programs, index_of, pipelines, seen, scopes = [], {}, [], set(), []
    for c in catalogs:
        remap = []
        for p in c["programs"]:
            if p["raw"] not in index_of:
                index_of[p["raw"]] = len(programs)
                programs.append(p)
            remap.append(index_of[p["raw"]])
        for index, key in c["pipelines"]:
            entry = (remap[index], key)
            if entry not in seen:
                seen.add(entry)
                pipelines.append(entry)
        for s in c["scope"].split("+"):
            if s and s not in scopes:
                scopes.append(s)
    scope = "+".join(scopes)
    if len(scope.encode("utf-8")) > MAX_SCOPE:
        scope = f"{len(scopes)} recordings merged"
    return {"build": first["build"], "title": first["title"], "version": first["version"], "scope": scope,
            "programs": programs, "pipelines": pipelines}


def describe(cat):
    files = collections.Counter(res for p in cat["programs"] for res in set(p["resources"]))
    lines = [f"scope {cat['scope']!r}, title {cat['title']:016X}, game version {cat['version']}, "
             f"build ABI {cat['build']:016x}",
             f"{len(cat['programs'])} programs, {len(cat['pipelines'])} pipelines "
             f"(the runtime takes up to {MAX_PROGRAMS} / {MAX_PIPELINES})"]
    lines += [f"  {n:4d} programs from {name}" for name, n in files.most_common()]
    return "\n".join(lines)


def main(argv):
    if len(argv) >= 2 and argv[0] == "info":
        for path in argv[1:]:
            print(f"{path}:\n{describe(parse(Path(path).read_bytes()))}")
        return 0
    if len(argv) >= 3 and argv[0] == "merge":
        merged = merge([parse(Path(p).read_bytes()) for p in argv[2:]])
        Path(argv[1]).write_bytes(encode(merged))
        print(describe(merged))
        return 0
    if len(argv) == 2 and argv[0] == "check":
        parse(Path(argv[1]).read_bytes())
        print("ok")
        return 0
    print(__doc__, file=sys.stderr)
    return 2


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv[1:]))
    except (CatalogError, OSError) as e:
        print(f"catalog: {e}", file=sys.stderr)
        sys.exit(1)
