#!/usr/bin/env bash
# Fire Emblem Warriors for Linux and SteamOS: builds the game from your own files, as a normal user, with nothing
# installed on the system. Run it from a terminal in this folder:
#
#     bash build-linux.sh                    build (or update) and add the game to Steam
#     bash build-linux.sh --launch           ... and start it
#     bash build-linux.sh --help             all options
#
# Put your NSPs (base, update 1.5.0, DLCs) and prod.keys in "Game Files" next to this script first.
# Step-by-step guide: GUIDE.md (section "Linux and Steam Deck").
set -eu
here="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"

if ! command -v python3 >/dev/null 2>&1; then
    echo "ERROR: python3 was not found. SteamOS, Bazzite, Fedora, Ubuntu and Arch include it; install it with your" >&2
    echo "       package manager (for example: sudo apt install python3) and run this again." >&2
    exit 1
fi

# The first build takes hours: keep the computer from sleeping meanwhile (the build stops when it sleeps).
# Skipped when systemd-inhibit is missing or not allowed (e.g. over SSH on SteamOS).
if [ -z "${FEW_INHIBITED:-}" ] && command -v systemd-inhibit >/dev/null 2>&1 \
   && systemd-inhibit --what=sleep:idle --who=FEW-Recomp --why=check true >/dev/null 2>&1; then
    export FEW_INHIBITED=1
    exec systemd-inhibit --what=sleep:idle --who="FEW-Recomp" --why="Building Fire Emblem Warriors" \
        python3 -u "$here/tools/linux/few_build.py" "$@"
fi
exec python3 -u "$here/tools/linux/few_build.py" "$@"
