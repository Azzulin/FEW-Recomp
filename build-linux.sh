#!/usr/bin/env bash
# Fire Emblem Warriors for Linux and SteamOS: builds the game from your own files, as a normal user, with nothing
# installed on the system. Double-click this file in the file manager (choose "Execute"), or run it from a terminal
# in this folder:
#
#     bash build-linux.sh                    build (or update) and add the game to Steam
#     bash build-linux.sh --launch           ... and start it
#     bash build-linux.sh --help             all options
#
# Put your NSPs (base, update 1.5.0, DLCs) and prod.keys in "Game Files" next to this script first.
# Step-by-step guide: GUIDE.md (section "Linux and Steam Deck").
set -eu
here="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"

# Started by a double-click (no terminal, but a desktop): reopen in a terminal window so the progress, the questions
# and any error can be seen. The window stays open at the end until Enter is pressed.
# Never under RecompLauncher, which runs its setup step without a window (few-prepare.sh).
if [ -z "${FEW_IN_TERMINAL:-}" ] && [ -z "${RECOMPLAUNCHER:-}" ] && [ ! -t 0 ] && [ ! -t 1 ] \
   && { [ -n "${DISPLAY:-}" ] || [ -n "${WAYLAND_DISPLAY:-}" ]; }; then
    export FEW_IN_TERMINAL=1
    script="$here/build-linux.sh"
    if command -v konsole >/dev/null 2>&1; then
        exec konsole --workdir "$here" -e bash "$script" "$@"
    elif command -v gnome-terminal >/dev/null 2>&1; then
        exec gnome-terminal --working-directory="$here" -- bash "$script" "$@"
    elif command -v kgx >/dev/null 2>&1; then
        exec kgx --working-directory="$here" -- bash "$script" "$@"
    elif command -v xfce4-terminal >/dev/null 2>&1; then
        exec xfce4-terminal --working-directory="$here" -x bash "$script" "$@"
    elif command -v x-terminal-emulator >/dev/null 2>&1; then
        exec x-terminal-emulator -e bash "$script" "$@"
    elif command -v xterm >/dev/null 2>&1; then
        exec xterm -e bash "$script" "$@"
    fi
    # No terminal program found: build anyway; the output goes to build-linux.log.
fi

if ! command -v python3 >/dev/null 2>&1; then
    echo "ERROR: python3 was not found. SteamOS, Bazzite, Fedora, Ubuntu and Arch include it; install it with your" >&2
    echo "       package manager (for example: sudo apt install python3) and run this again." >&2
    [ -n "${FEW_IN_TERMINAL:-}" ] && { printf '\nPress Enter to close this window.'; read -r _ || true; }
    exit 1
fi

# The first build takes hours: keep the computer from sleeping meanwhile (the build stops when it sleeps).
# Skipped when systemd-inhibit is missing or not allowed (e.g. over SSH on SteamOS).
build() {
    if command -v systemd-inhibit >/dev/null 2>&1 \
       && systemd-inhibit --what=sleep:idle --who=FEW-Recomp --why=check true >/dev/null 2>&1; then
        systemd-inhibit --what=sleep:idle --who="FEW-Recomp" --why="Building Fire Emblem Warriors" \
            python3 -u "$here/tools/linux/few_build.py" "$@"
    else
        python3 -u "$here/tools/linux/few_build.py" "$@"
    fi
}

if [ -n "${FEW_IN_TERMINAL:-}" ]; then
    status=0
    build "$@" || status=$?
    if [ "$status" -eq 0 ]; then
        printf '\nFinished. Press Enter to close this window.'
    else
        printf '\nThe build stopped (see the message above). Double-click build-linux.sh again to continue where it\n'
        printf 'stopped. Press Enter to close this window.'
    fi
    read -r _ || true
    exit "$status"
fi
build "$@"
