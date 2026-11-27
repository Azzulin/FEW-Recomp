"""tools/pipeline_catalog/catalog.py: reading, merging and writing pipeline catalogs. Synthetic catalogs only (no game
data): python -m unittest tools/tests/test_pipeline_catalog.py"""
import hashlib
from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "pipeline_catalog"))
import catalog  # noqa: E402

TITLE = 0x0100F15003E64000


def stage(resource, offset):
    name = resource.encode()
    return (struct.pack("<I", 1) + struct.pack("<I", len(name)) + name + struct.pack("<QI", offset, 0x100) +
            hashlib.sha256(name + struct.pack("<Q", offset)).digest() + struct.pack("<QII", 0, 0, 2) +
            struct.pack("<III", 0, 0, 0) + struct.pack("<II", 0, 0))


def program(key, resource, offset):
    absent = struct.pack("<I", 0)
    return {"key": key, "raw": struct.pack("<QQ", key, key ^ 0xFFFF) + stage(resource, offset) + absent * 4,
            "resources": [resource]}


def pipeline_key(program_key, state):
    return struct.pack("<Q", program_key) + struct.pack("<I", state) + bytes(4 * (catalog.KEY_WORDS - 3))


def make(scope, programs, pipelines, build=0x1234):
    return {"build": build, "title": TITLE, "version": 327680, "scope": scope, "programs": programs,
            "pipelines": pipelines}


class CatalogTest(unittest.TestCase):
    def test_roundtrip(self):
        p = program(0xA, "nx/shader/Effect.spkg", 0x40)
        cat = make("menus", [p], [(0, pipeline_key(0xA, 1)), (0, pipeline_key(0xA, 2))])
        data = catalog.encode(cat)
        back = catalog.parse(data)
        self.assertEqual((back["scope"], len(back["programs"]), len(back["pipelines"])), ("menus", 1, 2))
        self.assertEqual(catalog.encode(back), data)

    def test_merge_is_a_union_with_remapped_programs(self):
        a, b, c = (program(k, "nx/shader/SM.spkg", k * 0x100) for k in (1, 2, 3))
        one = make("menus", [a, b], [(0, pipeline_key(1, 1)), (1, pipeline_key(2, 1))])
        two = make("prologue", [c, b], [(1, pipeline_key(2, 1)), (1, pipeline_key(2, 7)), (0, pipeline_key(3, 1))])
        merged = catalog.parse(catalog.encode(catalog.merge([one, two])))
        self.assertEqual(merged["scope"], "menus+prologue")
        self.assertEqual([p["key"] for p in merged["programs"]], [1, 2, 3])
        self.assertEqual(len(merged["pipelines"]), 4)  # the shared (2, state 1) once
        for index, key in merged["pipelines"]:
            self.assertEqual(struct.unpack_from("<Q", key)[0], merged["programs"][index]["key"])

    def test_different_builds_do_not_merge(self):
        p = program(1, "nx/shader/SM.spkg", 0)
        with self.assertRaises(catalog.CatalogError):
            catalog.merge([make("a", [p], []), make("b", [p], [], build=0x99)])

    def test_damaged_file_is_refused(self):
        data = bytearray(catalog.encode(make("a", [program(1, "nx/shader/SM.spkg", 0)], [])))
        data[20] ^= 1
        with self.assertRaises(catalog.CatalogError):
            catalog.parse(bytes(data))

    def test_too_many_programs_is_refused_on_write(self):
        programs = [program(k, "nx/shader/SM.spkg", k) for k in range(catalog.MAX_PROGRAMS + 1)]
        with self.assertRaises(catalog.CatalogError):
            catalog.encode(make("a", programs, []))

    def test_shipped_catalog_is_valid_when_present(self):
        if catalog.CATALOG.is_file():
            cat = catalog.parse(catalog.CATALOG.read_bytes())
            self.assertEqual(cat["title"], TITLE)
            self.assertGreater(len(cat["pipelines"]), 0)


if __name__ == "__main__":
    unittest.main()
