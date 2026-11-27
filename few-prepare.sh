#!/bin/sh
# RecompLauncher's setup step on Linux and the Steam Deck: builds the game from the player's own files, without a
# window. The launcher runs it (recomplauncher.json, made by tools/recomplauncher/manifest.py); to build by hand,
# use build-linux.sh instead.
here="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
if ! command -v python3 >/dev/null 2>&1; then
    echo "python3 was not found. SteamOS, Bazzite, Fedora, Ubuntu and Arch include it: install it with your package"
    echo "manager (for example: sudo apt install python3), then press Set Up again."
    exit 1
fi
exec python3 -u "$here/tools/recomplauncher/prepare.py" "$@"
