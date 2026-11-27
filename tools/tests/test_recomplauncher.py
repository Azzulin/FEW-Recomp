"""RecompLauncher manifest and Linux setup step (tools/recomplauncher): offline, no game files, no network.

    python -m unittest tools/tests/test_recomplauncher.py
"""
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "recomplauncher"))
import manifest  # noqa: E402
import prepare  # noqa: E402


class Chunks:
    """A pipe that hands out the given byte chunks one read at a time."""

    def __init__(self, parts):
        self.parts = list(parts)

    def read1(self, n):
        return self.parts.pop(0) if self.parts else b""


class ManifestTest(unittest.TestCase):
    def test_one_setup_program_per_system(self):
        win, lin = manifest.manifest("windows"), manifest.manifest("linux")
        self.assertEqual(win["prepare"]["program"], ["few-prepare.bat"])
        self.assertEqual(lin["prepare"]["program"], ["few-prepare.sh"])
        self.assertEqual(win["format"], 1)

    def test_inputs_and_arguments(self):
        m = manifest.manifest("linux")
        ids = [i["id"] for i in m["inputs"]]
        self.assertEqual(ids[:2], ["game", "update"])
        self.assertIn("keys", ids)
        self.assertEqual(sum(i.startswith("dlc") for i in ids), manifest.DLC_COUNT)
        keys = next(i for i in m["inputs"] if i["id"] == "keys")
        self.assertEqual((keys["role"], keys["shared"]), ("key", "switch.prod-keys"))
        # One argument per file, flag and path together.
        self.assertEqual(m["prepare"]["args"], ["--game={input:game}", "--update={input:update}",
                                                "--keys={input:keys}"])
        self.assertEqual(m["start"]["env"]["HWDER_NSP"], "{input:game}")
        self.assertEqual(m["start"]["env"]["HWDER_AOC_DIR"], "{install}/dlc")
        self.assertTrue(all(i.get("into") == "dlc" for i in m["inputs"] if i["id"].startswith("dlc")))

    def test_root_file_is_the_windows_manifest(self):
        root = json.loads((manifest.ROOT / "recomplauncher.json").read_text(encoding="utf-8"))
        self.assertEqual(root, manifest.manifest("windows"),
                         "run: python tools/recomplauncher/manifest.py --write-root")


class ProgressTest(unittest.TestCase):
    def run_lines(self, parts):
        out = io.StringIO()
        p = prepare.Progress(out)
        prepare.pump(Chunks(parts), p)
        return p, out.getvalue().splitlines()

    def test_steps_compile_and_redraws(self):
        p, lines = self.run_lines([
            b"\n==> Build tools (downloaded once)\n",
            b"\r   zig 0.15.2: 1 of 54 MB\r   zig 0.15.2: 54 of 54 MB\n",
            b"==> Compiling (8 at a time)\r\n[1/100] a.o\r\n[50/100] b.o\r\n[100/100] c.o\r\n",
        ])
        self.assertIn("progress 0.020 Downloading the build tools (once, about 190 MB)", lines)
        self.assertIn("   zig 0.15.2: 54 of 54 MB", lines)  # the last state of a download stays in the log
        self.assertIn("progress 0.610 Compiling (50 of 100)", lines)
        self.assertIn("[50/100] b.o", lines)  # \r\n is an ordinary line end, never a redraw
        self.assertNotIn("zig 0.15.2: 1 of 54 MB", [l.strip() for l in lines if not l.startswith("progress")])

    def test_cr_split_across_reads(self):
        _, lines = self.run_lines([b"first\r", b"\nsecond"])
        self.assertEqual(lines, ["first", "second"])

    def test_error_and_update_reason(self):
        p, _ = self.run_lines([b"update: prod.keys has no 'titlekek_04'.\n",
                               b"ERROR: Could not unpack the update.\n"])
        self.assertEqual(p.update_line, "update: prod.keys has no 'titlekek_04'.")
        self.assertTrue(p.error.startswith("ERROR: Could not unpack"))


class SkipTest(unittest.TestCase):
    def test_already_built(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            game = tmp / "FireEmblemWarriors"
            game.mkdir()
            (game / "few").write_text("x")
            files = []
            for name in ("base.nsp", "update.nsp", "prod.keys"):
                (tmp / name).write_text("x")
                files.append(tmp / name)
            with patch.object(prepare, "ROOT", tmp), patch.object(prepare, "GAME_DIR", game), \
                    patch.object(prepare, "STAMP", game / "few-build.json"):
                (game / "few-build.json").write_text(json.dumps(prepare.build_key(files[1])))
                out = io.StringIO()
                with patch.object(sys, "stdout", out), patch.object(prepare.subprocess, "Popen") as popen:
                    code = prepare.main([f"--game={files[0]}", f"--update={files[1]}", f"--keys={files[2]}"])
                self.assertEqual(code, 0)
                popen.assert_not_called()
                self.assertIn("Already built", out.getvalue())


if __name__ == "__main__":
    unittest.main()
