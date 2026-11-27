"""Puts third_party/<name> at the commits gerar-few.ps1 pins (its $deps list, the one place they are written), from
GitHub's archives: no git needed. gerar-few.ps1 and build-linux.sh fetch the same commits themselves; this is for the
release workflow (.github/workflows/release.yml), which builds the setup window before anything else.

    python tools/fetch_deps.py            fetch what is missing
"""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools" / "recomplauncher"))
import package  # noqa: E402  (third_party_pins, fetch_third_party)


def main():
    cache = ROOT / "build" / ".third-party-cache"
    cache.mkdir(parents=True, exist_ok=True)
    for name, repo, commit, marker in package.third_party_pins():
        dest = ROOT / "third_party" / name
        if (dest / marker).is_file():
            print(f"{name}: present")
            continue
        for rel, (data, executable) in package.fetch_third_party(name, repo, commit, marker, cache).items():
            target = dest / rel
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
            if executable:
                target.chmod(0o755)
        print(f"{name}: {repo} @ {commit[:12]}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
