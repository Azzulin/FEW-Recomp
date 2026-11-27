#!/usr/bin/env python3
"""RecompLauncher's setup step on Linux and the Steam Deck (started by few-prepare.sh, see manifest.py).

    few-prepare.sh --game=<base.nsp> --update=<update.nsp> --keys=<prod.keys>

Runs tools/linux/few_build.py with the player's files (no window, no questions, no Steam shortcut: the launcher
starts the game) and turns its output into the launcher's `progress <0-1> <text>` lines. Build tools go to
{data}/cache/tools (RECOMPLAUNCHER_DATA), so uninstalling the game in the launcher removes them too. When the game
in FireEmblemWarriors/ was built by this release from the same update, it says "Already built" and stops. On a
failure the reason is the last line printed, as the launcher shows the last 10.
Runs on the system Python 3.8+ (standard library only), like few_build.py.
"""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parent.parent.parent
GAME_DIR = ROOT / "FireEmblemWarriors"
STAMP = GAME_DIR / "few-build.json"
# few_build.py's steps ("==> <text>") -> where they start on the bar, and what the player reads.
STEPS = [
    ("Checking this computer", 0.0, "Checking this computer"),
    ("Your game files", 0.01, "Checking your files"),
    ("Build tools", 0.02, "Downloading the build tools (once, about 190 MB)"),
    ("Unpacking update", 0.12, "Unpacking update 1.5.0"),
    ("Translating", 0.16, "Translating the game's code to C"),
    ("Compiling", 0.25, "Compiling"),
    ("Game folder", 0.98, "Finishing the game folder"),
]
COMPILE_START, COMPILE_END = 0.25, 0.97
NINJA = re.compile(r"^\[(\d+)/(\d+)\]")


def release():
    """The release this folder came from (RELEASE, written by package.py), else "dev"."""
    try:
        return (ROOT / "RELEASE").read_text(encoding="utf-8").strip() or "dev"
    except OSError:
        return "dev"


def build_key(update):
    st = Path(update).stat()
    return {"release": release(), "update": f"{Path(update).resolve()}|{st.st_size}|{st.st_mtime_ns}"}


class Progress:
    """Maps few_build.py's output to progress lines: at most one per percent, or every 5 s on the same percent."""

    def __init__(self, out=None):
        self.out, self.base, self.text, self.last, self.when = out or sys.stdout, 0.0, "Starting", -1.0, 0.0
        self.error = self.update_line = ""

    def emit(self, fraction, text, force=False):
        fraction = max(0.0, min(1.0, fraction))
        now = time.monotonic()
        if force or fraction - self.last >= 0.01 or now - self.when >= 5:
            self.out.write(f"progress {fraction:.3f} {text}\n")
            self.out.flush()
            self.last, self.when = fraction, now

    def line(self, text, redraw=False):
        """One piece of output; redraw = it ended in \\r (a download's progress line redrawing itself)."""
        stripped = text.strip()
        if stripped.startswith("==> "):
            for key, start, label in STEPS:
                if stripped[4:].startswith(key):
                    self.base, self.text = start, label
                    self.emit(start, label, force=True)
                    break
        elif stripped.startswith("ERROR: "):
            self.error = stripped
        elif stripped.startswith("update: "):
            self.update_line = stripped  # game_update.py's reason when the update cannot be unpacked
        m = NINJA.match(stripped)
        if m and self.text == "Compiling":
            done, total = int(m.group(1)), max(1, int(m.group(2)))
            self.emit(COMPILE_START + (COMPILE_END - COMPILE_START) * done / total,
                      f"Compiling ({done} of {total})")
        elif redraw and stripped:
            self.emit(self.base, stripped)  # e.g. "zig 0.15.2: 12 of 54 MB, 1.2 MB/s, 40 s left"
        if not redraw and stripped:
            self.out.write(text.rstrip("\r\n") + "\n")
            self.out.flush()


def pump(stream, progress):
    """Reads the child's output as it comes, splitting lines on \\n (or \\r\\n) and on the lone \\r that ends a
    redrawn progress line. A \\r at the end of what has arrived waits for the next read, which may bring its \\n."""
    pending, eof = b"", False
    while not eof:
        chunk = stream.read1(65536) if hasattr(stream, "read1") else stream.read(65536)
        eof = not chunk
        pending += chunk
        while True:
            m = re.search(rb"\r\n|\r|\n", pending)
            if not m or (m.group() == b"\r" and m.end() == len(pending) and not eof):
                break
            piece, sep, pending = pending[:m.start()], m.group(), pending[m.end():]
            progress.line(piece.decode("utf-8", "replace"), redraw=sep == b"\r")
    if pending:
        progress.line(pending.decode("utf-8", "replace"))


def command(game, update, keys, tools):
    build = [sys.executable, "-u", str(ROOT / "tools" / "linux" / "few_build.py"), "--base", game, "--update", update,
             "--keys", keys, "--no-steam", "--tools-dir", str(tools)]
    # The first build takes hours: keep the machine awake when allowed (as build-linux.sh does).
    try:
        allowed = subprocess.run(["systemd-inhibit", "--what=sleep:idle", "--who=FEW-Recomp", "--why=check", "true"],
                                 capture_output=True).returncode == 0
    except OSError:
        allowed = False
    if allowed:
        return ["systemd-inhibit", "--what=sleep:idle", "--who=FEW-Recomp",
                "--why=Building Fire Emblem Warriors"] + build
    return build


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--game", required=True, help="the base game NSP")
    ap.add_argument("--update", required=True, help="the update 1.5.0 NSP")
    ap.add_argument("--keys", required=True, help="prod.keys")
    a = ap.parse_args(argv)
    data = Path(os.environ.get("RECOMPLAUNCHER_DATA") or ROOT / "recompdata")
    progress = Progress()
    for what, value in (("base game", a.game), ("update", a.update), ("prod.keys", a.keys)):
        if not Path(value).is_file():
            print(f"The {what} file was not found: {value}. Pick it again on the setup screen.", flush=True)
            return 2
    key = build_key(a.update)
    try:
        if json.loads(STAMP.read_text(encoding="utf-8")) == key and (GAME_DIR / "few").is_file():
            progress.emit(1.0, "Already built", force=True)
            return 0
    except (OSError, ValueError):
        pass
    try:
        STAMP.unlink()  # a build cut short must never look finished
    except OSError:
        pass
    progress.emit(0.0, "Starting the build", force=True)
    env = dict(os.environ, PYTHONUNBUFFERED="1")
    child = subprocess.Popen(command(a.game, a.update, a.keys, data / "cache" / "tools"), cwd=str(ROOT), env=env,
                             stdout=subprocess.PIPE, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL)
    pump(child.stdout, progress)
    code = child.wait()
    if code:
        print(f"The build stopped during: {progress.text}. Press Set Up again to continue where it stopped "
              f"(full output: {ROOT / 'build-linux.log'}).", flush=True)
        if progress.error:
            print(progress.error[len('ERROR: '):], flush=True)
        if progress.update_line and "update" in progress.error:
            print(progress.update_line[len("update: "):], flush=True)  # the actual reason, last
        return code
    if not (GAME_DIR / "few").is_file():
        print(f"The build ended without {GAME_DIR / 'few'}; see the output above.", flush=True)
        return 1
    STAMP.write_text(json.dumps(key) + "\n", encoding="utf-8")
    progress.emit(1.0, "Ready to play", force=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
