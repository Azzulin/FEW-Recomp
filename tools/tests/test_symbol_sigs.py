"""Named symbols found in another build of the code (tools/recomp/symbol_sigs.py, GitHub issue #1): synthetic
modules only, no game files.

    python -m unittest tools/tests/test_symbol_sigs.py
"""
from pathlib import Path
import struct
import sys
import tempfile
import unittest

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))
from recomp import symbol_sigs  # noqa: E402
from recomp.symbol_sigs import read_symbols  # noqa: E402

BASE = 0x1000000000
NOP = 0xD503201F
RET = 0xD65F03C0


class Mod:
    def __init__(self, words, name="main"):
        self.name, self.base = name, BASE
        self.text_off, self.text_size = 0, 4 * len(words)
        self.image = bytearray(struct.pack(f"<{len(words)}I", *words) + bytes(0x4000))


class Proc:
    def __init__(self, mod):
        self.modules = [mod]

    def module_at(self, addr):
        m = self.modules[0]
        return m if m.base <= addr < m.base + len(m.image) else None


def func(seed, n=6):
    """A function body: distinct MOVZ immediates, then RET."""
    return [0xD2800000 | ((seed * 31 + i) & 0xFFFF) << 5 | (i % 8) for i in range(n)] + [RET]


def layout(parts):
    """Words of the module and the start (address) of each named part."""
    words, starts = [], {}
    for name, body in parts:
        starts[name] = BASE + 4 * len(words)
        words += body
    return words, starts


# A function that reaches data through ADRP x8, #1 page; ADD x8, x8, #0x10 (the symbol sits 4 bytes past it).
DATA_USER = [0xB0000008, 0x91004108, 0xB9400100, RET]


def program(pad=0, extra_copy=False):
    parts = [("a", func(1)), ("pad", [NOP] * pad), ("b", func(2)), ("dup1", func(9)), ("dup2", func(9)),
             ("user", DATA_USER), ("c", func(3, 10))]
    if extra_copy:
        parts.append(("dup3", func(9)))
    return layout(parts)


class SymbolSigsTest(unittest.TestCase):
    def setUp(self):
        words, self.st = program()
        self.data = ((self.st["user"] & ~0xFFF) + 0x1000 + 0x10 + 4)
        self.symbols = [(self.st["a"], "A", True, False), (self.st["b"], "B", True, True),
                        (self.st["dup2"], "Dup2", True, False), (self.st["c"] + 8, "CMid", True, False),
                        (self.data, "g_Data", False, False)]
        lines = symbol_sigs.make(Proc(Mod(words)), self.symbols, self.st.values(), log=lambda s: None)
        tmp = tempfile.NamedTemporaryFile("w", suffix=".sig", delete=False, encoding="utf-8")
        tmp.write("\n".join(lines) + "\n")
        tmp.close()
        self.sigs = symbol_sigs.read_sigs(tmp.name)
        Path(tmp.name).unlink()

    def relocate(self, words):
        logs = []
        return symbol_sigs.relocate(Proc(Mod(words)), self.symbols, self.sigs, logs.append), logs

    def test_same_build_keeps_every_address(self):
        got, logs = self.relocate(program()[0])
        self.assertEqual(got, self.symbols)
        self.assertEqual(logs, [])

    def test_moved_code_is_found(self):
        words, st = program(pad=0x400)  # one page of code before everything but "a"
        got, logs = self.relocate(words)
        by = {s[1]: s for s in got}
        self.assertEqual(by["A"][0], st["a"])
        self.assertEqual(by["B"], (st["b"], "B", True, True))
        self.assertEqual(by["Dup2"][0], st["dup2"])
        self.assertEqual(by["CMid"][0], st["c"] + 8)
        self.assertEqual(by["g_Data"][0], self.data + 0x1000)  # ADRP pages move with the code here
        self.assertEqual([s[1] for s in got], [s[1] for s in self.symbols])
        self.assertTrue(any("B 0x" in s and "moved" in s for s in logs))

    def test_changed_code_drops_the_symbol_and_its_hook(self):
        words, st = program(pad=8)
        words[(st["b"] - BASE) // 4 + 2] = NOP
        got, logs = self.relocate(words)
        self.assertNotIn("B", [s[1] for s in got])
        self.assertTrue(any("WARNING: B" in s and "hook is left out" in s for s in logs))

    def test_another_copy_of_a_duplicated_body_is_ambiguous(self):
        words, _ = program(pad=8, extra_copy=True)
        got, logs = self.relocate(words)
        self.assertNotIn("Dup2", [s[1] for s in got])
        self.assertIn("B", [s[1] for s in got])


class TrackedFilesTest(unittest.TestCase):
    def test_every_symbol_has_a_signature_at_its_address(self):
        symbols = read_symbols(str(TOOLS / "recomp" / "symbols.txt"))
        sigs = symbol_sigs.read_sigs(str(TOOLS / "recomp" / "symbols.sig"))
        self.assertEqual(sorted(s[1] for s in symbols), sorted(sigs), "run tools/recomp/symbol_sigs.py again")
        for addr, name, is_func, _ in symbols:
            if is_func:
                self.assertEqual(sigs[name]["at"] + sigs[name]["off"], addr, name)

    def test_signatures_hold_no_code(self):
        """Only these fields, each a name, a number or a hash: never instruction bytes."""
        text = (TOOLS / "recomp" / "symbols.sig").read_text(encoding="utf-8")
        allowed = {"module", "kind", "at", "size", "off", "head", "body", "count", "rank", "ref"}
        for ln in text.splitlines():
            if ln.startswith("#") or not ln.strip():
                continue
            fields = dict(kv.split("=", 1) for kv in ln.split()[1:])
            self.assertLessEqual(set(fields), allowed, ln)
            self.assertRegex(fields["body"], r"^[0-9a-f]{32}$")
            self.assertRegex(fields["head"], r"^[0-9a-f]{8}$")


if __name__ == "__main__":
    unittest.main()
