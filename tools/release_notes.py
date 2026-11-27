#!/usr/bin/env python3
"""The GitHub release notes of one version: a fixed header (what to download, how to start) and the version's section
of CHANGELOG.md, so the notes are written once, in the changelog. Fails when CHANGELOG.md has no section for the
version ("## FEW Recomp v0.5.0 (date)"): write it before tagging. Used by .github/workflows/release.yml.

    python tools/release_notes.py v0.5.0 > notes.md
"""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parent.parent
REPO = "Azzulin/FEW-Recomp"

HEADER = """> [!IMPORTANT]
> **This release contains no game files, keys or executable.** You build the game on your own PC from your own
> legally dumped copy: the base game, update **1.5.0** and (optionally) the DLCs, plus your `prod.keys`.

### How to get it

| On | Download | Then |
|---|---|---|
| **Windows** | `FEW-Recomp-{v}-Windows-x64.zip` | Extract it into a new folder and open **FEW Recomp Setup.exe**: pick your files, press **Build the game**, then **Play**. |
| **Steam Deck / Linux** | `FEW-Recomp-{v}-Linux-x64.tar.gz` | Extract it on the internal storage and double-click `build-linux.sh` (choose *Execute*). |
| **[RecompLauncher](https://recomplauncher.com)** | (the launcher picks the right file) | Its setup asks for your files and builds the game. |

Step by step, settings and troubleshooting: [GUIDE.md](https://github.com/{repo}/blob/{v}/GUIDE.md).
`SHA256SUMS` lists the checksums of the two packages.

### What's new
"""


def section(changelog, version):
    """The body of '## FEW Recomp <version> ...' up to the next '## ' heading."""
    m = re.search(rf"^## FEW Recomp {re.escape(version)}(?=[ \t]|$).*?$\n(.*?)(?=^## |\Z)", changelog, re.M | re.S)
    return m.group(1).strip() if m else None


def notes(version, changelog=None):
    changelog = changelog if changelog is not None else (ROOT / "CHANGELOG.md").read_text(encoding="utf-8")
    body = section(changelog, version)
    if not body:
        raise SystemExit(f"release_notes: CHANGELOG.md has no '## FEW Recomp {version}' section; write it first "
                         "(rename 'Unreleased' to the version and date)")
    return HEADER.format(v=version, repo=REPO) + "\n" + body + \
        f"\n\nFull history: [CHANGELOG.md](https://github.com/{REPO}/blob/{version}/CHANGELOG.md).\n"


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    sys.stdout.reconfigure(encoding="utf-8")
    sys.stdout.write(notes(sys.argv[1]))
