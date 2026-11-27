#!/usr/bin/env python3
"""Builds Fire Emblem Warriors for Linux / SteamOS from your own dump. Started by ../../build-linux.sh.

    bash build-linux.sh                     everything: tools, update, ARM64 -> C, compile, game folder, Steam
    bash build-linux.sh --game-dir DIR      NSPs (and prod.keys) somewhere other than "Game Files"
    bash build-linux.sh --tools-only        only the pinned build tools (used by tools/deck/build-deck-wsl.sh)

Runs on the system Python 3.8+ (standard library only), as a normal user: nothing is installed on the system.
The build itself runs on pinned tools downloaded once and checked by SHA-256 (tools/linux/toolchain.json: Zig/Clang
targeting glibc 2.35, CMake, Ninja, Python + wheels) and on the static SDL3/FFmpeg/libcrypto of the HWDER v0.3.5
Linux release (MIT). The game runs on any distribution with glibc 2.35+ (SteamOS 3, Bazzite, Ubuntu 22.04+, Arch).

Where things go (all ignored by git):
    ~/.local/share/FEW-Recomp/      the build tools (about 1 GB), shared by every copy of this repository
    data/update/                    update 1.5.0 unpacked from your NSP
    generated/                      the game's code translated to C
    build/linux/                    compiler output and build.log
    FireEmblemWarriors/             the game: few, its settings, saves, logs and shader caches
    build-linux.log                 everything the last run printed (for problem reports)
Every step resumes, downloads included: run it again after an interruption and only what is missing is redone.
"""
import argparse
import hashlib
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import time
import urllib.request

ROOT = Path(__file__).resolve().parent.parent.parent
TOOLS = ROOT / "tools"
sys.path.insert(0, str(TOOLS))
import linux_setup  # noqa: E402  (HWDER's Linux setup: pinned toolchain download and checks)
import game_profile  # noqa: E402  (title IDs of the releases the build accepts)
import build_events  # noqa: E402  (steps and progress for the build window, tools/build_ui)

DEPS_RELEASE = {  # HWDER v0.3.5 Linux release: only its linux-deps folder is used (static SDL3, FFmpeg, libcrypto)
    "url": "https://github.com/blocker-3/HWDER/releases/download/v0.3.5/HWDER-linux-x86_64-v0.3.5.tar.gz",
    "sha256": "ddd20149944559ff737e2e6ed6a430c899636974ee363d2b6803985c39b01934",
    "size": 31500068,
    "root": "HWDER-linux-x86_64-v0.3.5",
}
THIRD_PARTY = [  # the same pinned commits as gerar-few.ps1; Linux takes SDL from linux-deps
    ("imgui", "ocornut/imgui", "ed73ef4e84c9488256c5710de6ff1ebc2c9a8496", "imgui.cpp"),
    ("spirv-headers", "KhronosGroup/SPIRV-Headers", "86f980c731e62ae4eaf383d320449d71687936bf",
     "include/spirv/unified1/spirv.hpp"),
    ("vulkan-headers", "KhronosGroup/Vulkan-Headers", "c46850864f4661461b0f6cb9922c058ffea4915e",
     "include/vulkan/vulkan.h"),
]
DISK_NEEDED_GB = 8      # tools ~1 GB, generated C ~0.5 GB, objects ~3 GB, with headroom
GB_PER_JOB = 0.9        # the largest generated files take ~0.7 GB each in Clang
DECK_SETTINGS = {"fps60": "0", "fps60_1080p": "0", "fps30_720p": "1", "res_630p": "1", "show_fps": "1"}
COMMON_SETTINGS = {"version": "2", "face_labels": "1", "async_shaders": "1", "save_dir": "save"}

report = linux_setup.Report()
LOG = ROOT / "build-linux.log"  # everything a run prints, to attach to a problem report (ignored by git)


class BuildError(Exception):
    pass


class Tee:
    """Shows what the build prints and copies it to LOG. Progress lines that redraw themselves ("\\r...") are only
    shown: the log keeps the lines around them."""
    def __init__(self, stream, log):
        self.stream, self.log = stream, log

    def write(self, text):
        self.stream.write(text)
        if not text.startswith("\r"):
            self.log.write(text)
        return len(text)

    def flush(self):
        self.stream.flush()
        self.log.flush()

    def __getattr__(self, name):
        return getattr(self.stream, name)


def step(text, event_id=None):
    """Prints a step; event_id (build_events.STEPS) also moves the build window to it."""
    print(f"\n==> {text}", flush=True)
    if event_id:
        build_events.emit("step", id=event_id)


def info(text):
    print(f"    {text}", flush=True)


# ---- Machine checks --------------------------------------------------------------------------------------------

def is_steam_deck():
    try:
        vendor = Path("/sys/class/dmi/id/board_vendor").read_text().strip()
        board = Path("/sys/class/dmi/id/board_name").read_text().strip()
    except OSError:
        return False
    return vendor == "Valve" and board in ("Jupiter", "Galileo")


def cpu_has_avx2():
    try:
        return " avx2 " in " " + Path("/proc/cpuinfo").read_text(errors="replace").replace("\n", " ") + " "
    except OSError:
        return True  # cannot tell; the compiler and the game will say


def filesystem_type(path):
    """Type of the mount holding `path` (ext4, btrfs, exfat, vfat, fuseblk, 9p...), or ''."""
    path = str(Path(path).resolve())
    best, kind = "", ""
    try:
        for line in Path("/proc/mounts").read_text().splitlines():
            parts = line.split()
            if len(parts) < 3:
                continue
            mount = parts[1].replace("\\040", " ")
            if (path == mount or path.startswith(mount.rstrip("/") + "/")) and len(mount) >= len(best):
                best, kind = mount, parts[2]
    except OSError:
        pass
    return kind


def default_jobs():
    cpus = os.cpu_count() or 1
    try:
        meminfo = Path("/proc/meminfo").read_text()
        available_gb = int(re.search(r"MemAvailable:\s+(\d+)", meminfo).group(1)) / 1024 ** 2
    except (OSError, AttributeError):
        available_gb = 8
    return max(1, min(cpus, int((available_gb - 1.0) / GB_PER_JOB)))


def check_machine(out_dir):
    if not sys.platform.startswith("linux") or platform.machine() != "x86_64":
        raise BuildError("This builds the Linux version and needs Linux on an x86-64 PC. On Windows use gerar-few.ps1.")
    if sys.version_info < (3, 8):
        raise BuildError("Python 3.8 or newer is needed (python3 --version).")
    if not cpu_has_avx2():
        raise BuildError("This CPU has no AVX2; the game needs it (any x86-64 CPU since about 2015 has it).")
    for place in (ROOT, out_dir.parent):
        kind = filesystem_type(place)
        if kind in ("vfat", "exfat", "msdos", "ntfs", "ntfs3", "fuseblk"):
            raise BuildError(f"{place} is on a {kind} drive, which cannot hold Linux programs. Move this folder to "
                             "the internal storage (for example /home/deck) or to an ext4/btrfs SD card or drive.")
    free_gb = shutil.disk_usage(ROOT).free / 1024 ** 3
    if free_gb < DISK_NEEDED_GB:
        raise BuildError(f"Not enough free space next to {ROOT}: {free_gb:.1f} GB free, about {DISK_NEEDED_GB} GB "
                         "needed.")


# ---- Game files ------------------------------------------------------------------------------------------------

def find_inputs(game_dir, keys):
    game_dir = Path(game_dir).expanduser().resolve()
    if not game_dir.is_dir():
        raise BuildError(f'The game folder {game_dir} does not exist. Create "Game Files" next to build-linux.sh and '
                         "put your NSPs and prod.keys in it, or pass --game-dir.")
    nsps = sorted(p for p in game_dir.iterdir() if p.suffix.lower() == ".nsp" and p.is_file())
    # The release (US, or JP: Japan / Asia) is the one whose base game is there (tools/game_profile.py REGIONS).
    found = {r: [p for p in nsps if f"[{pid:016X}]" in p.name.upper()] for r, pid in game_profile.REGIONS.items()}
    regions = [r for r, files in found.items() if files]
    if len(regions) > 1:
        raise BuildError(f"{game_dir} has base game NSPs of more than one release ({', '.join(regions)}); keep one.")
    region = regions[0] if regions else "US"
    base_id = f"{game_profile.REGIONS[region]:016X}"
    update_id = f"{game_profile.REGIONS[region] | 0x800:016X}"
    base = found[region]
    update = [p for p in nsps if f"[{update_id}]" in p.name.upper()]
    if len(base) != 1:
        ids = " or ".join(f"[{pid:016X}] ({r})" for r, pid in game_profile.REGIONS.items())
        raise BuildError(f"Expected 1 base game NSP with {ids} in its name in {game_dir}, found {len(base)}.")
    if len(update) != 1:
        raise BuildError(f"Expected 1 update 1.5.0 NSP with [{update_id}] in its name in {game_dir}, "
                         f"found {len(update)}. The update is required.")
    if keys:
        keys = Path(keys).expanduser().resolve()
    else:
        keys = next((p for p in (game_dir / "prod.keys", ROOT / "prod.keys") if p.is_file()), None)
    if not keys or not keys.is_file():
        raise BuildError(f'prod.keys not found (looked in {game_dir} and {ROOT}). Put it there or pass --keys.')
    dlcs = [p for p in nsps if p not in base and p not in update]
    info(f"release: {region}" + ("" if region == "US" else " (not yet tested; please report how it runs)"))
    info(f"base   : {base[0].name}")
    info(f"update : {update[0].name}")
    info(f"DLCs   : {len(dlcs)}")
    info(f"keys   : {keys}")
    return base[0], update[0], keys


def given_inputs(base, update, keys):
    """Files named one by one (RecompLauncher's setup step): any file names; the update's ticket says its release
    when it is unpacked (game_update.py), and the base game is checked by the game itself when it starts."""
    files = []
    for what, value in (("base game NSP (--base)", base), ("update 1.5.0 NSP (--update)", update),
                        ("prod.keys (--keys)", keys)):
        if not value:
            raise BuildError(f"Missing the {what}.")
        path = Path(value).expanduser().resolve()
        if not path.is_file():
            raise BuildError(f"The {what} was not found: {path}")
        files.append(path)
    info(f"base   : {files[0]}")
    info(f"update : {files[1]}")
    info(f"keys   : {files[2]}")
    return tuple(files)


# ---- Build tools -----------------------------------------------------------------------------------------------

def tools_home():
    configured = os.environ.get("XDG_DATA_HOME", "")
    base = Path(configured) if configured and Path(configured).is_absolute() else Path.home() / ".local" / "share"
    return base / "FEW-Recomp"


def ensure_toolchain(tools_dir):
    """The pinned Zig/Clang, CMake, Ninja and Python, installed by HWDER's linux_setup under tools_dir/HWDER."""
    tools_dir.mkdir(parents=True, exist_ok=True)
    os.environ["XDG_DATA_HOME"] = str(tools_dir)  # linux_setup keeps its toolchain in $XDG_DATA_HOME/HWDER
    (tools_dir / "HWDER").mkdir(exist_ok=True)
    try:
        tc = linux_setup.install_toolchain(report, ROOT)
    except linux_setup.SetupError as e:
        raise BuildError(str(e))
    info(f"toolchain: {tc}")
    return Path(tc)


def ensure_linux_deps(tools_dir):
    deps = tools_dir / "linux-deps"
    if (deps / "BUILD-INFO.json").is_file() and (deps / "lib").is_dir():
        info(f"linux-deps: {deps}")
        return deps
    archive = tools_dir / DEPS_RELEASE["url"].rsplit("/", 1)[1]
    if not (archive.is_file() and linux_setup.file_sha256(archive) == DEPS_RELEASE["sha256"]):
        try:
            linux_setup.download(DEPS_RELEASE["url"], archive, DEPS_RELEASE["sha256"], DEPS_RELEASE["size"], report,
                                 "HWDER v0.3.5 Linux libraries")
        except linux_setup.SetupError as e:
            raise BuildError(str(e))
    unpack = tools_dir / ".unpack-deps"
    shutil.rmtree(unpack, ignore_errors=True)
    unpack.mkdir()
    linux_setup.safe_extract_tar(archive, unpack)
    source = unpack / DEPS_RELEASE["root"] / "linux-deps"
    if not (source / "BUILD-INFO.json").is_file():
        raise BuildError("The HWDER release has no linux-deps folder")
    shutil.rmtree(deps, ignore_errors=True)
    os.replace(source, deps)
    shutil.rmtree(unpack, ignore_errors=True)
    archive.unlink()
    info(f"linux-deps: {deps}")
    return deps


def ensure_third_party():
    """imgui, SPIRV-Headers and Vulkan-Headers at their pinned commits (git if present, else GitHub's archive)."""
    for name, repo, commit, marker in THIRD_PARTY:
        target = ROOT / "third_party" / name
        if (target / marker).is_file():
            continue
        info(f"{name}: downloading commit {commit[:12]}")
        shutil.rmtree(target, ignore_errors=True)
        if shutil.which("git"):
            target.mkdir(parents=True)
            git = ["git", "-C", str(target)]
            ok = (subprocess.run(git + ["init", "-q"]).returncode == 0
                  and subprocess.run(git + ["fetch", "-q", "--depth", "1", f"https://github.com/{repo}.git",
                                            commit]).returncode == 0
                  and subprocess.run(git + ["checkout", "-q", "FETCH_HEAD"]).returncode == 0)
            if ok and (target / marker).is_file():
                continue
            shutil.rmtree(target, ignore_errors=True)
        archive = ROOT / "third_party" / f".{name}.tar.gz"
        url = f"https://codeload.github.com/{repo}/tar.gz/{commit}"
        for attempt in range(4):
            try:
                request = urllib.request.Request(url, headers={"User-Agent": "FEW-Recomp-build"})
                with urllib.request.urlopen(request, timeout=60) as reply, open(archive, "wb") as out:
                    shutil.copyfileobj(reply, out)
                break
            except OSError as e:
                if attempt == 3:
                    raise BuildError(f"Could not download {name} ({e}). Check the internet connection.")
                time.sleep(2 ** attempt)
        unpack = ROOT / "third_party" / f".unpack-{name}"
        shutil.rmtree(unpack, ignore_errors=True)
        unpack.mkdir()
        linux_setup.safe_extract_tar(archive, unpack)
        inner = next(unpack.iterdir())
        os.replace(inner, target)
        shutil.rmtree(unpack, ignore_errors=True)
        archive.unlink()
        if not (target / marker).is_file():
            raise BuildError(f"{name} is missing {marker} after unpacking")


def build_env(tc, deps):
    paths = linux_setup.tool_paths(tc)
    env = {k: v for k, v in os.environ.items()
           if k not in ("LD_PRELOAD", "LD_LIBRARY_PATH", "PYTHONPATH", "PYTHONHOME", "CFLAGS", "CXXFLAGS", "LDFLAGS",
                        "CPATH", "C_INCLUDE_PATH", "CPLUS_INCLUDE_PATH", "LIBRARY_PATH", "PKG_CONFIG_PATH", "CC",
                        "CXX")}
    env.update({
        "PATH": os.pathsep.join([str(tc / "bin"), str(paths["cmake"].parent), str(paths["ninja"].parent),
                                 str(paths["python"].parent), "/usr/bin", "/bin"]),
        "CC": str(paths["cc"]), "CXX": str(paths["cxx"]), "HWDER_AR": str(paths["ar"]),
        "HWDER_RANLIB": str(paths["ranlib"]), "HWDER_LINUX_DEPS": str(deps),
        "ZIG_GLOBAL_CACHE_DIR": str(tc / "zig-cache"), "ZIG_LOCAL_CACHE_DIR": str(tc / "zig-cache"),
        "PYTHONNOUSERSITE": "1", "PYTHONUTF8": "1",
    })
    return env


# ---- Game code -------------------------------------------------------------------------------------------------

def run(command, env, log=None):
    """Runs `command`, showing its output (and appending it to `log`)."""
    with open(log, "a", encoding="utf-8") if log else open(os.devnull, "w") as sink:
        process = subprocess.Popen(command, cwd=str(ROOT), env=env, stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT, text=True, errors="replace", bufsize=1)
        for line in process.stdout:
            sys.stdout.write(line)
            sys.stdout.flush()
            sink.write(line)
        return process.wait()


def unpack_update(python, env, update, keys):
    out = ROOT / "data" / "update"
    stamp = out / "source.txt"
    st = update.stat()
    signature = f"{update}|{st.st_size}|{st.st_mtime_ns}"
    if stamp.is_file() and stamp.read_text(encoding="utf-8").strip() == signature and (out / "update.json").is_file():
        info("already unpacked")
        return out
    code = run([str(python), "-I", "-B", "-X", "utf8", "-c",
                "import sys; sys.path.insert(0, sys.argv[1]); import game_update as g; sys.exit(g.main(sys.argv[2:5]))",
                str(TOOLS), str(update), str(keys), str(out)], env)
    if code:
        raise BuildError("Could not unpack the update (the reason is on the 'update:' line above).")
    stamp.write_text(signature + "\n", encoding="utf-8")
    return out


def merge_found_entries(out_dir):
    """Addresses the game jumped to that were not known as functions (written by the runtime next to the game)
    join the tracked list before the code is translated again, as gerar-few.ps1 does."""
    found = out_dir / "extra_entries.txt"
    if not found.is_file():
        return
    tracked = TOOLS / "recomp" / "extra_entries.txt"
    old = [l.strip().lower() for l in tracked.read_text(encoding="utf-8").splitlines() if l.strip()]
    new = [l.strip().lower() for l in found.read_text(encoding="utf-8").splitlines()
           if l.strip() and l.strip().lower() not in old]
    if new:
        tracked.write_text("\n".join(sorted(set(old + new))) + "\n", encoding="utf-8")
        info("new entries: " + " ".join(new))
    found.unlink()


def translate(python, env, update_dir):
    code = run([str(python), "-B", "-X", "utf8", "-c",
                "import sys, runpy; sys.path.insert(0, sys.argv[1]); "
                "sys.argv = ['gen_main', '--exefs', sys.argv[2], '--out', sys.argv[3]]; "
                "runpy.run_module('recomp.gen_main', run_name='__main__')",
                str(TOOLS), str(update_dir / "exefs"), str(ROOT / "generated")], env)
    if code:
        raise BuildError("Translating the game code to C failed (see the output above).")


def compile_game(python, env, jobs, march):
    build = ROOT / "build" / "linux"
    build.mkdir(parents=True, exist_ok=True)
    log = build / "build.log"
    with open(log, "a", encoding="utf-8") as f:
        f.write(f"\n==== {time.strftime('%Y-%m-%d %H:%M:%S')} jobs={jobs} march={march or 'generic'}\n")
    code = run([str(python), "-B", "-X", "utf8", str(TOOLS / "build.py"), "--jobs", str(jobs), "--build-dir",
                str(build), "--generated-dir", str(ROOT / "generated"), "--define", f"HW_MARCH={march}",
                "--define", "HW_GAME_HOOKS=OFF"], env, log)
    exe = build / "few"
    if code or not exe.is_file():
        raise BuildError(f"Compiling failed (log: {log}). Run the script again to resume; if it stops at the same "
                         "place, try --jobs 2 and report the problem with that log.")
    return exe


# ---- Build window ----------------------------------------------------------------------------------------------
# tools/build_ui: a window with the steps and the progress, next to this terminal. tools/build.py compiles it in the
# background (half a minute: SDL comes prebuilt) and opens it when it is ready, so the build never waits for it. It
# only reads FEW_EVENTS: closing it changes nothing, and if it cannot be built or shown the build goes on here.

UI_BUILD = ROOT / "build" / "ui-linux"
EVENTS = ROOT / "build" / "build-events.jsonl"


def window_wanted(a):
    return (not a.no_ui and not a.tools_only and not os.environ.get("RECOMPLAUNCHER")
            and bool(os.environ.get("DISPLAY") or os.environ.get("WAYLAND_DISPLAY")))


def start_window_build(python, env):
    UI_BUILD.mkdir(parents=True, exist_ok=True)
    log = UI_BUILD / "ui-build.log"
    try:
        with open(log, "w", encoding="utf-8") as out:
            process = subprocess.Popen(
                [str(python), "-B", "-X", "utf8", str(TOOLS / "build.py"), "--build-ui", "--jobs", "2",
                 "--build-dir", str(UI_BUILD), "--open-events", str(EVENTS), "--watch-pid", str(os.getpid())],
                cwd=str(ROOT), env=env, stdout=out, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL)
        info(f"Build window: being compiled in the background, it opens by itself (log: {log})")
        return process
    except OSError as e:
        info(f"(the build window could not be compiled: {e}; the build goes on here)")
        return None


# ---- Game folder -----------------------------------------------------------------------------------------------

def write_settings(ini, values, defaults):
    """Sets `values` (paths) in hwder_settings.ini, adds `defaults` only where a key is missing, keeps the rest."""
    lines = ini.read_text(encoding="utf-8", errors="replace").splitlines() if ini.is_file() else []
    seen, out = set(), []
    for line in lines:
        key = line.split("=", 1)[0]
        if key in values:
            if key not in seen:
                out.append(f"{key}={values[key]}")
            seen.add(key)
        elif line.strip():
            out.append(line)
            seen.add(key)
    out += [f"{k}={v}" for k, v in {**defaults, **values}.items() if k not in seen]
    ini.write_text("\n".join(out) + "\n", encoding="utf-8")


LAUNCHER = """#!/bin/sh
# Starts Fire Emblem Warriors from its own folder (settings, saves, logs and shader caches live here).
# The Steam shortcut made by build-linux.sh runs this file.
cd "$(dirname -- "$0")" || exit 1
exec ./few "$@"
"""

DECK_ENV = """# Variables for the game (NAME=VALUE), read by few itself when it starts. A variable set in the environment wins.
# Frame overlap: the game prepares the next frame while the GPU draws the current one; holds 30 fps in dense fights
# on the Steam Deck. Delete the line to turn it off.
HWDER_FRAME_OVERLAP=1
"""


def publish(exe, out_dir, base, keys, update_dir, deck):
    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / "save").mkdir(exist_ok=True)
    if shutil.which("pgrep") and subprocess.run(["pgrep", "-x", "few"], capture_output=True).returncode == 0:
        raise BuildError("The game is running; close it and run the script again.")
    temporary = out_dir / ".few.tmp"
    shutil.copy2(exe, temporary)
    os.chmod(temporary, 0o755)
    os.replace(temporary, out_dir / "few")
    launcher = out_dir / "few-steam.sh"
    launcher.write_text(LAUNCHER, encoding="utf-8")
    os.chmod(launcher, 0o755)
    # The pipeline catalog (tools/pipeline_catalog): with it the game prepares, before its first frame, the graphics
    # it would otherwise compile in the middle of a battle. Metadata only; the code comes from the player's game.
    catalog = TOOLS / "pipeline_catalog" / "few-us-1.5.0.bin"
    if catalog.is_file():
        shutil.copyfile(catalog, out_dir / "pipeline_catalog.bin")
    if deck and not (out_dir / "hwder_env.txt").exists():
        (out_dir / "hwder_env.txt").write_text(DECK_ENV, encoding="utf-8")
    defaults = dict(COMMON_SETTINGS, **(DECK_SETTINGS if deck else {}))
    write_settings(out_dir / "hwder_settings.ini",
                   {"nsp_path": str(base), "keys_path": str(keys), "update_path": str(update_dir)}, defaults)
    if not os.access(out_dir / "few", os.X_OK):
        raise BuildError(f"{out_dir} does not allow running programs (a FAT/exFAT/NTFS drive, or mounted noexec). "
                         "Move this folder to the internal storage or an ext4/btrfs drive.")


def add_to_steam(out_dir, assume_yes):
    import steam_shortcut
    return steam_shortcut.add_to_steam(out_dir / "few", assume_yes=assume_yes)


# ---- Main ------------------------------------------------------------------------------------------------------

def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--game-dir", default=str(ROOT / "Game Files"),
                    help='folder with the base, update 1.5.0 and DLC NSPs (default: "Game Files")')
    ap.add_argument("--keys", default="", help="your prod.keys (default: in the game folder or next to this script)")
    ap.add_argument("--base", default="", help="the base game NSP itself, any name (with --update; skips --game-dir)")
    ap.add_argument("--update", default="", help="the update 1.5.0 NSP itself, any name (with --base)")
    ap.add_argument("--out", default=str(ROOT / "FireEmblemWarriors"), help="game folder (default: %(default)s)")
    ap.add_argument("--jobs", type=int, default=0, help="parallel compiles (default: from free RAM and CPUs)")
    deck = ap.add_mutually_exclusive_group()
    deck.add_argument("--deck", dest="deck", action="store_true", default=None,
                      help="Steam Deck build: Zen 2 code and the Deck's settings (default on a Deck)")
    deck.add_argument("--no-deck", dest="deck", action="store_false", help="generic build and PC settings")
    ap.add_argument("--march", default=None, help="CPU target (default: znver2 on a Deck, generic AVX2 elsewhere)")
    steam = ap.add_mutually_exclusive_group()
    steam.add_argument("--steam", dest="steam", action="store_true", default=None,
                       help="add the game to Steam without asking (closes and reopens Steam)")
    steam.add_argument("--no-steam", dest="steam", action="store_false", help="do not touch Steam")
    ap.add_argument("--launch", action="store_true", help="start the game when the build is done")
    ap.add_argument("--no-ui", action="store_true", help="no build window: the progress only in this terminal")
    ap.add_argument("--tools-only", action="store_true", help="only install the build tools")
    ap.add_argument("--tools-dir", default="", help="where the build tools go (default: ~/.local/share/FEW-Recomp)")
    a = ap.parse_args(argv)

    out_dir = Path(a.out).expanduser().resolve()
    tools_dir = Path(a.tools_dir).expanduser().resolve() if a.tools_dir else tools_home()
    on_deck = is_steam_deck()
    deck = on_deck if a.deck is None else a.deck
    march = a.march if a.march is not None else ("znver2" if deck else "")
    jobs = a.jobs or default_jobs()
    started = time.time()
    if not a.tools_only:
        log = open(LOG, "w", encoding="utf-8", errors="replace")
        log.write(f"build-linux.sh {' '.join(argv if argv is not None else sys.argv[1:])} - "
                  f"{time.strftime('%Y-%m-%d %H:%M:%S')}\n")
        sys.stdout, sys.stderr = Tee(sys.stdout, log), Tee(sys.stderr, log)
    if window_wanted(a):
        os.environ["FEW_EVENTS"] = str(EVENTS)  # read by tools/build.py too, for the compile progress
        build_events.EventLog(EVENTS).start("Fire Emblem Warriors", log=LOG)
    else:
        os.environ.pop("FEW_EVENTS", None)
    window_build = None
    try:
        step("Checking this computer", "check")
        check_machine(out_dir)
        info(f"{'Steam Deck' if on_deck else 'Linux PC'}, {os.cpu_count()} CPU threads, "
             f"{jobs} parallel compiles, CPU target {march or 'generic AVX2'}")
        if not a.tools_only:
            step("Your game files")
            if a.base or a.update:
                base, update, keys = given_inputs(a.base, a.update, a.keys)
            else:
                base, update, keys = find_inputs(a.game_dir, a.keys)
        step(f"Build tools (downloaded once, about 190 MB, kept in {tools_dir})", "tools")
        tc = ensure_toolchain(tools_dir)
        deps = ensure_linux_deps(tools_dir)
        ensure_third_party()
        if a.tools_only:
            print(f"TOOLCHAIN={tc}\nLINUX_DEPS={deps}")
            return 0
        env = build_env(tc, deps)
        python = linux_setup.tool_paths(tc)["python"]
        if os.environ.get("FEW_EVENTS"):
            window_build = start_window_build(python, env)
        step("Unpacking update 1.5.0 (data/update)", "update")
        update_dir = unpack_update(python, env, update, keys)
        merge_found_entries(out_dir)
        step("Translating the game's ARM64 code to C (generated/)", "translate")
        translate(python, env, update_dir)
        step(f"Compiling ({jobs} at a time; the first time takes hours, later runs only redo what changed)", "compile")
        exe = compile_game(python, env, jobs, march)
        step(f"Game folder: {out_dir}", "finish")
        publish(exe, out_dir, base, keys, update_dir, deck)
        info("few, few-steam.sh, hwder_settings.ini" + (", hwder_env.txt" if deck else "") + ", save/")
        if a.steam is not False:
            step("Steam shortcut")
            build_events.emit("progress", id="finish", fraction=-1,
                              text="Adding the game to Steam: answer in the terminal if it asks")
            if a.steam is None and not sys.stdin.isatty():
                info("skipped (no terminal to ask in); run again with --steam to add it")
            elif not add_to_steam(out_dir, True if a.steam else None):
                info("not added; you can run the script again with --steam later")
    except BuildError as e:
        print(f"\nERROR: {e}", file=sys.stderr, flush=True)
        build_events.emit("error", text=str(e))
        if not a.tools_only and not os.environ.get("RECOMPLAUNCHER"):  # the launcher keeps its own full log
            print(f"Everything shown here is also in {LOG}: attach that file if you report the problem.",
                  file=sys.stderr, flush=True)
        return 1
    except KeyboardInterrupt:
        print("\nStopped. Run the script again to continue where it stopped.", file=sys.stderr)
        build_events.emit("error", text="Stopped with Ctrl+C. Run the build again to continue where it stopped.")
        return 130
    build_events.emit("done", play=str(out_dir / "few-steam.sh"), folder=str(out_dir))
    minutes = (time.time() - started) / 60
    print(f"\nDone in {minutes:.0f} min. Play: {out_dir / 'few-steam.sh'}"
          + ("  (or 'Fire Emblem Warriors' in Steam, under Non-Steam)" if a.steam is not False else ""), flush=True)
    if a.launch:
        os.execv("/bin/sh", ["/bin/sh", str(out_dir / "few-steam.sh")])
    return 0


if __name__ == "__main__":
    sys.exit(main())
