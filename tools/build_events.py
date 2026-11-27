"""Build progress for the build window (tools/build_ui), as an append-only file of JSON lines, one event per line.

FEW_EVENTS names the file; without it nothing is written. The steps come from gerar-few.ps1 (Windows) and
tools/linux/few_build.py (Linux), the compile progress from tools/build.py. The window only reads the file, so
closing it never stops the build, and a window opened late still sees every step from the start.

Every event has "type" and "t" (seconds since the epoch):
    start     steps: [{"id", "label"}], title, log (where the full output goes, or "")
    step      id: the step that starts now (the ones before it are done)
    progress  id, fraction (0-1), text; while compiling also done, total, eta (seconds, or null while unknown),
              files (the files that just finished)
    done      play (the program that starts the game), folder (the game folder)
    error     text (why the build stopped)
The ids are those of STEPS; gerar-few.ps1 writes the same ones.
"""
import json
import os
from pathlib import Path
import re
import subprocess
import time

STEPS = [
    ("check", "Checking your files"),
    ("tools", "Getting the build tools"),
    ("update", "Unpacking update 1.5.0"),
    ("translate", "Translating the game's code to C"),
    ("compile", "Compiling the game"),
    ("finish", "Finishing the game folder"),
]
NINJA_STATUS = "[%f/%t] "  # finished/total: ninja prints an edge's line when it ends when the output is not a terminal
NINJA_LINE = re.compile(r"^\[(\d+)/(\d+)\]\s+(.*?)\s*$")
OBJECT = re.compile(r"^Building (?:C|CXX|RC) object (?:CMakeFiles/[^/]+\.dir/)?(.+?)\.(?:obj|o)$")
MIN_WEIGHT = 32 * 1024      # a step without a source file to weigh (or a tiny one) counts like a 32 KB file
FUNCTION_END = re.compile(rb"\n\}\r?\n")  # a generated function ends with "}" alone on a line
LINK_SHARE = 0.03           # the final link of the game, as a share of all the work
ETA_AFTER_SHARE = 0.02      # no time left shown before 2% of the work...
ETA_AFTER_SECONDS = 20      # ... and 20 s have passed


class EventLog:
    def __init__(self, path, clock=time.time):
        self.path, self.clock = Path(path), clock

    @classmethod
    def from_env(cls):
        path = os.environ.get("FEW_EVENTS")
        return cls(path) if path else None

    def emit(self, kind, **fields):
        event = {"type": kind, "t": round(self.clock(), 3), **fields}
        line = json.dumps(event, ensure_ascii=False, separators=(",", ":")) + "\n"
        try:
            with open(self.path, "a", encoding="utf-8", newline="\n") as stream:
                stream.write(line)
        except OSError:
            pass  # the window is a convenience: never stop the build for it

    def start(self, title, log=""):
        try:
            self.path.parent.mkdir(parents=True, exist_ok=True)
            self.path.write_text("", encoding="utf-8")
        except OSError:
            return
        self.emit("start", title=title, log=str(log), steps=[{"id": i, "label": l} for i, l in STEPS])


def emit(kind, **fields):
    """Writes one event when FEW_EVENTS is set (the module-level shortcut the build scripts use)."""
    log = EventLog.from_env()
    if log:
        log.emit(kind, **fields)


def cost(data):
    """Expected compile work of one source file: its size plus the square of its longest function (in MB). Clang
    spends far more than linear time on one huge function: over the 413 generated files of the Windows build
    (2026-11-27) this follows the measured times with r = 0.92, the size alone with r = 0.87."""
    longest, previous = 0, 0
    for m in FUNCTION_END.finditer(data):
        longest, previous = max(longest, m.start() - previous), m.start()
    return len(data) + longest * longest / 1e6


def weigh(description, root):
    """How much work one ninja step is (cost() of the source file it compiles), at least MIN_WEIGHT."""
    m = OBJECT.match(description)
    if m:
        try:
            return max(MIN_WEIGHT, cost((Path(root) / m.group(1)).read_bytes()))
        except OSError:
            pass
    return MIN_WEIGHT


def plan(lines, root):
    """{description: weight} of the steps a dry run (`ninja -n`) lists. The game's final link gets LINK_SHARE."""
    weights = {}
    for line in lines:
        m = NINJA_LINE.match(line)
        if m:
            weights[m.group(3)] = weigh(m.group(3), root)
    links = [d for d in weights if d.startswith("Linking")]
    if links:
        compiled = sum(w for d, w in weights.items() if d not in links)
        for d in links:
            weights[d] = max(MIN_WEIGHT, compiled * LINK_SHARE / len(links))
    return weights


def dry_run(ninja, build_dir, target, env, root):
    try:
        out = subprocess.run([ninja, "-C", str(build_dir), "-n", target], env=env, capture_output=True, text=True,
                             encoding="utf-8", errors="replace").stdout
    except OSError:
        return {}
    return plan(out.splitlines(), root)


class CompileProgress:
    """Turns ninja's finished-step lines into progress events, weighted by file size so that the few huge files
    (minutes each) and the many small ones (seconds) move the bar about as much as they take."""

    def __init__(self, weights, clock=time.monotonic):
        self.weights, self.clock = dict(weights), clock
        self.total = sum(self.weights.values())
        self.done_weight, self.started = 0.0, clock()

    def line(self, text):
        m = NINJA_LINE.match(text.rstrip("\r\n"))
        if not m:
            return None
        done, total, description = int(m.group(1)), int(m.group(2)), m.group(3)
        self.done_weight += self.weights.pop(description, 0.0)
        if self.total > 0:
            fraction = min(1.0, self.done_weight / self.total)
        else:  # nothing planned (no dry run): count the steps
            fraction = done / total if total else 0.0
        if done >= total:
            fraction = 1.0
        elapsed = self.clock() - self.started
        eta = None
        if ETA_AFTER_SHARE <= fraction < 1.0 and elapsed >= ETA_AFTER_SECONDS:
            eta = round(elapsed / fraction * (1.0 - fraction))
        obj = OBJECT.match(description)
        files = [Path(obj.group(1)).name] if obj else []
        return {"fraction": round(fraction, 4), "done": done, "total": total, "eta": eta, "files": files,
                "text": f"{done} of {total} steps"}


def run_ninja(ninja, build_dir, jobs, target, env, root, stdout=None):
    """Runs ninja like `ninja -C build_dir -j jobs target`; with FEW_EVENTS set, also reports its progress. Returns
    ninja's exit code. Without FEW_EVENTS ninja keeps the terminal to itself, as before."""
    import sys
    stdout = stdout or sys.stdout
    log = EventLog.from_env()
    command = [ninja, "-C", str(build_dir), "-j", str(jobs), target]
    if not log:
        return subprocess.run(command, env=env).returncode
    env = dict(env, NINJA_STATUS=NINJA_STATUS)
    progress = CompileProgress(dry_run(ninja, build_dir, target, env, root))
    process = subprocess.Popen(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                               encoding="utf-8", errors="replace", bufsize=1)
    for line in process.stdout:
        stdout.write(line)
        stdout.flush()
        event = progress.line(line)
        if event:
            log.emit("progress", id="compile", **event)
    return process.wait()
