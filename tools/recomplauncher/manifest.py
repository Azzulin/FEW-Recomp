#!/usr/bin/env python3
"""recomplauncher.json for RecompLauncher (https://recomplauncher.com/sdk-files/guide.md), one per release file.

    python tools/recomplauncher/manifest.py --os windows|linux [--out FILE]
    python tools/recomplauncher/manifest.py --write-root      the repository's recomplauncher.json (Windows layout)

Each release file (tools/recomplauncher/package.py) carries the manifest of its system at its top: the launcher
tries `prepare.program` in order, so one manifest naming both a .bat and a .sh would run the wrong one somewhere
(a .bat runs through Proton on Linux).

How it plays from the launcher:
- inputs: the base game NSP (US or Japanese), update 1.5.0, up to five DLC NSPs and the shared prod.keys. NSPs
  have no content check the launcher knows (its checks are for discs, .nup packages and exefs folders), so the
  setup step checks them itself and says why when a file is wrong.
- prepare: few-prepare.bat / few-prepare.sh build the game from the update (hours the first time; nothing is
  downloaded but pinned, SHA-256 checked tools). The base NSP is read while playing (its RomFS is the game's
  data), so start also gets {input:game} and the launcher keeps asking for the copy.
- start: the built game, with the player's files passed in its HWDER_* variables, so moving the game folder or
  the files never leaves a stale path in hwder_settings.ini. DLCs are linked into {install}/dlc before each start.
"""
import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
SCHEMA = "https://recomplauncher.com/schema/recomplauncher.json"
REPO = "Azzulin/FEW-Recomp"
DLC_COUNT = 5  # the game has five add-on packages (docs/few.md)

LAYOUT = {
    "windows": {
        "prepare": "few-prepare.bat",
        "game_dir": "Jogar no PC",
        "program": "Jogar no PC/few.exe",
        "built": "Jogar no PC/few.exe",
        "system": {"windows": {"version": "10"}},
    },
    "linux": {
        "prepare": "few-prepare.sh",
        "game_dir": "FireEmblemWarriors",
        "program": "FireEmblemWarriors/few-steam.sh",
        "built": "FireEmblemWarriors/few",
        "system": {"linux": {"glibc": "2.35"}},
    },
}


def manifest(system):
    lay = LAYOUT[system]
    nsp = [{"type": "file", "extensions": ["nsp"]}]
    inputs = [
        {"id": "game", "label": "Your Fire Emblem Warriors base game (.nsp, v0): the US release "
                               "[0100F15003E64000] or the Japanese one [0100EAE003A5A000].",
         "formats": nsp},
        {"id": "update", "label": "Update 1.5.0 (.nsp, v327680) of the same release: [0100F15003E64800] (US) or "
                                  "[0100EAE003A5A800] (Japanese).",
         "role": "update", "formats": nsp},
    ]
    inputs += [{"id": f"dlc{i}", "label": f"DLC {i} (.nsp, optional)", "role": "dlc", "optional": True,
                "formats": nsp, "into": "dlc"} for i in range(1, DLC_COUNT + 1)]
    inputs.append({"id": "keys", "label": "prod.keys from your own console", "role": "key",
                   "shared": "switch.prod-keys"})
    game_dir = "{install}/" + lay["game_dir"]
    return {
        "$schema": SCHEMA,
        "format": 1,
        "name": "Fire Emblem Warriors Recomp",
        "repo": REPO,
        "inputs": inputs,
        "prepare": {
            "program": [lay["prepare"]],
            # One argument per file, flag and path together (the launcher's rule for inputs).
            "args": ["--game={input:game}", "--update={input:update}", "--keys={input:keys}"],
            "label": "Building the game from your update (the first time takes about an hour on a PC and a few "
                     "hours on a Steam Deck)",
            "done": ["{install}/" + lay["built"]],
        },
        "start": {
            "program": [lay["program"]],
            "env": {
                "HWDER_NSP": "{input:game}",
                "HWDER_KEYS": "{input:keys}",
                "HWDER_UPDATE": "{install}/data/update",
                "HWDER_AOC_DIR": "{install}/dlc",
                "HWDER_NO_PRELAUNCH": "1",
            },
        },
        "saves": [game_dir + "/save"],
        "folders": {
            "game": [{"path": game_dir}],
            "saves": [{"path": game_dir + "/save"}],
            "logs": [{"path": game_dir}],
        },
        "exit_codes": {"0": "ok"},
        "system": lay["system"],
    }


def write(system, out):
    Path(out).write_text(json.dumps(manifest(system), indent=2, ensure_ascii=False) + "\n", encoding="utf-8",
                         newline="\n")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--os", choices=sorted(LAYOUT))
    ap.add_argument("--out", help="file to write (default: stdout)")
    ap.add_argument("--write-root", action="store_true", help="write ROOT/recomplauncher.json (Windows layout)")
    a = ap.parse_args()
    if a.write_root:
        write("windows", ROOT / "recomplauncher.json")
        return 0
    if not a.os:
        ap.error("--os or --write-root")
    if a.out:
        write(a.os, a.out)
    else:
        print(json.dumps(manifest(a.os), indent=2, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
