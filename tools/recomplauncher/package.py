#!/usr/bin/env python3
"""Release files for RecompLauncher, built from a clean checkout in one command (no compiler, no game files).

    python tools/recomplauncher/package.py --version v0.3.0 [--out dist] [--os windows linux] [--allow-dirty]

Writes, for the GitHub release of that tag (names as the launcher expects: system and processor in the name):
    FEW-Recomp-<version>-Windows-x64.zip       FEW-Recomp-<version>-Linux-x64.tar.gz
Each holds one folder, FEW-Recomp-<version>/, with:
- the project's tracked files (git ls-files: code, scripts and docs only; the game is never in the repository);
- the third-party sources the build needs, at the commits gerar-few.ps1 pins (SDL on Windows only: Linux takes it
  from HWDER's prebuilt linux-deps), so the player needs no git;
- RELEASE (the version, which the setup step uses to tell an old build from a new one);
- recomplauncher.json for that system (tools/recomplauncher/manifest.py) at the top, next to its setup step
  (few-prepare.bat / few-prepare.sh). Nothing in it is game code: the setup step builds the game on the player's
  machine from their own files.
Upload both to the release, marked as a full release (pre-releases are betas in the launcher).
"""
import argparse
import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess
import sys
import tarfile
import time
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
import manifest  # noqa: E402

NAME = "FEW-Recomp"
SYSTEMS = {"windows": "Windows-x64.zip", "linux": "Linux-x64.tar.gz"}
SETUP_EXE = "FEW Recomp Setup.exe"  # the Windows setup window (tools/build_ui), built by the release workflow
LINUX_SKIPS = {"SDL"}  # Linux links HWDER's prebuilt static SDL3 (tools/linux/few_build.py)
# Never in a release, whatever git says (AGENTS.md: dumps, keys and anything derived from them).
FORBIDDEN = re.compile(r"(\.(nsp|nsz|xci|nca|keys|tik)$|^(generated|data|build|Game Files|Jogar no PC|"
                       r"FireEmblemWarriors|Envio Deck|save)/|(^|/)recompdata/)", re.I)
TIMESTAMP = time.mktime((2026, 1, 1, 0, 0, 0, 0, 0, -1))  # fixed mtime for the bundled third-party files


def git(*args):
    return subprocess.run(["git", "-C", str(ROOT), *args], capture_output=True, check=True).stdout


def tracked():
    """(path, bytes, executable) of every file in the index, as the repository stores it (LF line endings, whatever
    the checkout's core.autocrlf), with CRLF for *.bat as .gitattributes asks; mode 100755 keeps its executable bit."""
    entries = []
    for entry in git("ls-files", "-s", "-z").split(b"\0"):
        if not entry:
            continue
        meta, path = entry.split(b"\t", 1)
        mode, blob = meta.split()[:2]
        if mode != b"160000":  # not a submodule
            entries.append((path.decode("utf-8"), blob.decode(), mode == b"100755"))
    batch = subprocess.run(["git", "-C", str(ROOT), "cat-file", "--batch"], check=True, capture_output=True,
                           input=b"".join(blob.encode() + b"\n" for _, blob, _ in entries)).stdout
    out, at = [], 0
    for path, blob, executable in entries:
        header_end = batch.index(b"\n", at)
        size = int(batch[at:header_end].split()[2])
        data = batch[header_end + 1:header_end + 1 + size]
        at = header_end + 1 + size + 1
        if path.lower().endswith(".bat"):
            data = data.replace(b"\r\n", b"\n").replace(b"\n", b"\r\n")
        out.append((path, data, executable))
    return out


def third_party_pins():
    """(name, owner/repo, commit, marker) from gerar-few.ps1's $deps, the one place the commits are written."""
    text = (ROOT / "gerar-few.ps1").read_text(encoding="utf-8")
    pins = re.findall(r"@\('([\w-]+)',\s*'https://github\.com/([^']+?)\.git',\s*'([0-9a-f]{40})',\s*'([^']+)'\)", text)
    if len(pins) < 4:
        raise SystemExit("package: could not read the third_party pins from gerar-few.ps1")
    return [(n, r, c, m.replace("\\", "/")) for n, r, c, m in pins]


def fetch_third_party(name, repo, commit, marker, cache):
    """{relative path: (bytes, executable)} of one pinned commit (GitHub's archive of it, cached in `cache`)."""
    archive = cache / f"{name}-{commit}.tar.gz"
    if not archive.is_file():
        url = f"https://codeload.github.com/{repo}/tar.gz/{commit}"
        print(f"  {name}: downloading {repo} @ {commit[:12]}", flush=True)
        request = urllib.request.Request(url, headers={"User-Agent": "FEW-Recomp-package"})
        with urllib.request.urlopen(request, timeout=120) as reply, open(str(archive) + ".part", "wb") as out:
            shutil.copyfileobj(reply, out)
        os.replace(str(archive) + ".part", archive)
    files = {}
    with tarfile.open(archive) as tar:
        for member in tar.getmembers():
            parts = PurePosixPath(member.name).parts[1:]  # drop "<repo>-<commit>/"
            if not parts or not member.isfile():
                continue  # folders are implied; links are left out (none is needed to build)
            rel = "/".join(parts)
            if ".." in parts or rel.startswith("/"):
                raise SystemExit(f"package: unsafe path in {name}: {member.name}")
            files[rel] = (tar.extractfile(member).read(), bool(member.mode & 0o111))
    if marker not in files:
        raise SystemExit(f"package: {name} @ {commit[:12]} has no {marker}")
    return files


def contents(system, version, cache, setup_exe=None):
    """{path inside the folder: (bytes, executable)} for one system's release file. setup_exe: the built
    FEW Recomp Setup.exe (tools/build_ui, no game code), added to the Windows file at the top of the folder."""
    out = {}
    for path, data, executable in tracked():
        if path == "recomplauncher.json" or (path.startswith("third_party/") and path != "third_party/README.md"):
            continue
        if FORBIDDEN.search(path):
            raise SystemExit(f"package: refusing to ship {path}")
        if system == "windows" and path == "few-prepare.sh" or system == "linux" and path == "few-prepare.bat":
            continue
        out[path] = (data, executable)
    for name, repo, commit, marker in third_party_pins():
        if system == "linux" and name in LINUX_SKIPS:
            continue
        for rel, item in fetch_third_party(name, repo, commit, marker, cache).items():
            out[f"third_party/{name}/{rel}"] = item
    out["RELEASE"] = (version.encode() + b"\n", False)
    if system == "windows" and setup_exe:
        out[SETUP_EXE] = (Path(setup_exe).read_bytes(), True)
    out["recomplauncher.json"] = (json.dumps(manifest.manifest(system), indent=2, ensure_ascii=False).encode() + b"\n",
                                  False)
    for required in ("recomplauncher.json", manifest.LAYOUT[system]["prepare"], "tools/recomplauncher/prepare.py"
                     if system == "linux" else "tools/recomplauncher/prepare.ps1"):
        if required not in out:
            raise SystemExit(f"package: {required} is missing")
    return out


def write_zip(files, top, target):
    with zipfile.ZipFile(target, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for path in sorted(files):
            data, executable = files[path]
            info = zipfile.ZipInfo(f"{top}/{path}", date_time=(2026, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = ((0o755 if executable else 0o644) | 0o100000) << 16
            z.writestr(info, data)


def write_tar(files, top, target):
    with tarfile.open(target, "w:gz", compresslevel=9, format=tarfile.PAX_FORMAT) as tar:
        folders = sorted({str(PurePosixPath(p).parent) for p in files} - {"."})
        for folder in [top] + [f"{top}/{f}" for f in folders]:
            info = tarfile.TarInfo(folder)
            info.type, info.mode, info.mtime = tarfile.DIRTYPE, 0o755, TIMESTAMP
            tar.addfile(info)
        for path in sorted(files):
            data, executable = files[path]
            info = tarfile.TarInfo(f"{top}/{path}")
            info.size, info.mode, info.mtime = len(data), 0o755 if executable else 0o644, TIMESTAMP
            tar.addfile(info, io.BytesIO(data))


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--version", required=True, help="the release tag, like v0.3.0")
    ap.add_argument("--out", default=str(ROOT / "dist"))
    ap.add_argument("--os", nargs="+", choices=sorted(SYSTEMS), default=sorted(SYSTEMS))
    ap.add_argument("--allow-dirty", action="store_true",
                    help="package even with uncommitted changes (what is staged in the index is what ships; tests)")
    ap.add_argument("--setup-exe", help=f'the built setup window, shipped in the Windows file as "{SETUP_EXE}"')
    a = ap.parse_args(argv)
    if not re.fullmatch(r"v\d+\.\d+\.\d+([-.][0-9A-Za-z.]+)?", a.version):
        ap.error("--version looks like v0.3.0")
    if git("status", "--porcelain", "--untracked-files=no").strip() and not a.allow_dirty:
        raise SystemExit("package: the checkout has uncommitted changes; commit them (or pass --allow-dirty)")
    out = Path(a.out)
    cache = out / ".third-party-cache"
    cache.mkdir(parents=True, exist_ok=True)
    top = f"{NAME}-{a.version}"
    sums = []
    for system in a.os:
        files = contents(system, a.version, cache, a.setup_exe)
        target = out / f"{NAME}-{a.version}-{SYSTEMS[system]}"
        (write_zip if system == "windows" else write_tar)(files, top, target)
        digest = hashlib.sha256(target.read_bytes()).hexdigest()
        print(f"{target.name}: {len(files)} files, {target.stat().st_size / 1e6:.1f} MB, sha256 {digest}", flush=True)
        sums.append(f"{digest} *{target.name}")
    (out / "SHA256SUMS").write_text("\n".join(sums) + "\n", encoding="ascii", newline="\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
