# FEW Recomp — build and play guide

Step-by-step instructions to build *Fire Emblem Warriors* from **your own copy** and play it on Windows, Linux or
the Steam Deck. Nothing of the game is in this repository or in the releases: your computer builds the game from
your dump.

> [!IMPORTANT]
> You need your own legally obtained copy of the game, dumped from your own Nintendo Switch, and the `prod.keys`
> from that same console. This project does not provide game files or keys and will not explain how to get them.
> What you build contains the game and is for your own use only: do not share it.

**Pick your path:**

| I want to play on… | Do this |
|---|---|
| a Windows PC | [1](#1-what-you-need) → [2](#2-get-the-source) → [3](#3-put-your-game-files-in-place) → [4](#4-windows) |
| a Steam Deck, building on the Deck itself (no other computer needed) | [1](#1-what-you-need) → [5A](#5a-steam-deck-step-by-step) |
| a Linux PC (Bazzite, Arch, Fedora, Ubuntu, …) | [1](#1-what-you-need) → [2](#2-get-the-source) → [3](#3-put-your-game-files-in-place) → [5B](#5b-linux-pc) |
| a Steam Deck, building on a Windows PC (faster) | [4](#4-windows) first, then [5C](#5c-steam-deck-built-on-a-windows-pc) |

Then: [6. Settings, saves and logs](#6-settings-saves-and-logs) · [7. Updating](#7-updating-to-a-newer-version) ·
[8. Troubleshooting](#8-troubleshooting) · [9. Reporting a problem](#9-reporting-a-problem)

## 1. What you need

**Your game files (US or Japanese release, from your own console):**

| File | US title ID (in the file name) | Japanese title ID | Version |
|---|---|---|---|
| Base game | `[0100F15003E64000]` | `[0100EAE003A5A000]` | v0 |
| Update | `[0100F15003E64800]` | `[0100EAE003A5A800]` | **1.5.0** (v327680) — required |
| DLCs (optional) | `[0100F15003E65001]` … `[0100F15003E65005]` | `[0100EAE003A5B001]` … `[0100EAE003A5B005]` | any |
| `prod.keys` | — | — | from the same console |

The scripts recognise the base game and the update by the **title ID in square brackets** in the file name, so keep
those tags (for example `Fire Emblem Warriors [0100F15003E64000][v0][Base][US].nsp`), and use the base, update and
DLCs of one release only. The **US release is the tested one**. The **Japanese release** (also the one sold in
Asia) is accepted but **not tested yet**: the game runs in Japanese, and the build says whether your update's code is
identical to the tested US one. Please report how it runs. Other releases and other update versions are not
supported. `.nsz`/`.xci` files are not accepted: use `.nsp`.

**Windows PC:** Windows 10/11 x64, [Git](https://git-scm.com/download/win), [Python 3](https://www.python.org/downloads/windows/)
(tick *Add python.exe to PATH* in the installer), about 5 GB free plus your NSPs (~19 GB with all DLCs), and a GPU
with a Vulkan 1.2+ driver. First build: roughly 45–90 minutes.

**Linux PC or Steam Deck:** an x86-64 CPU with AVX2, a Vulkan GPU driver (Mesa RADV, NVIDIA, Intel), `python3`
(already on SteamOS, Bazzite, Fedora, Ubuntu and Arch), about **8 GB free on a Linux drive** (internal storage, or an
ext4/btrfs SD card — not exFAT/FAT/NTFS), and internet the first time (~190 MB of build tools). **Nothing is installed
on the system and no `sudo` is needed**: the build tools (Zig/Clang, CMake, Ninja, Python) are downloaded once into
`~/.local/share/FEW-Recomp`, each file checked against a fixed SHA-256. The first build takes a few hours on a
Steam Deck and less on a desktop PC; later updates only redo what changed.

## 2. Get the source

Download **Source code (zip)** (or `.tar.gz`) from the [Releases](https://github.com/Azzulin/FEW-Recomp/releases)
page and extract it, or clone it:

```bash
git clone https://github.com/Azzulin/FEW-Recomp.git
```

Windows: extract to a short path such as `C:\Games\FEW-Recomp`. Linux: anywhere on a Linux drive, for example
`~/Games/FEW-Recomp`.

## 3. Put your game files in place

Create a folder named **`Game Files`** inside the source folder and put your NSPs and keys there:

```
FEW-Recomp/
  build-linux.sh            (Linux / Steam Deck)
  gerar-few.ps1             (Windows)
  Game Files/
    Fire Emblem Warriors [0100F15003E64000][v0][Base][US].nsp
    Fire Emblem Warriors [0100F15003E64800][v327680][1.5.0][UPD][US].nsp
    Fire Emblem Warriors [...][0100F15003E65001][US].nsp      <- DLCs, optional
    prod.keys                                               <- here or next to the scripts
```

Your NSPs and keys are read where they are, never copied or uploaded. On Linux they can also stay in another folder
(even on an exFAT SD card): pass it with `--game-dir`.

## 4. Windows

Open **PowerShell** in the source folder (in Explorer: *File → Open Windows PowerShell*, or type `powershell` in the
address bar) and run:

```powershell
powershell -ExecutionPolicy Bypass -File .\gerar-few.ps1 -Launch
```

(`-ExecutionPolicy Bypass` only because the script is not signed. Leave out `-Launch` to build without starting.)

The script checks your files, fetches four pinned libraries, installs its private build tools (Python 3.13,
LLVM-MinGW, CMake, Ninja; no admin, no Visual Studio, into `%LOCALAPPDATA%\HWDER`), unpacks update 1.5.0, translates
the game's ARM64 code to C, compiles it and publishes **`Jogar no PC\few.exe`** ("Play on PC"). If it stops halfway,
run it again: it resumes. Play with `Jogar no PC\few.exe` — make a shortcut or add it to Steam as a non-Steam game.

| Option | What it does |
|---|---|
| `-Jogo D:\Switch\FEW` | read the NSPs from another folder |
| `-Keys D:\keys\prod.keys` | keys from another place |
| `-Jobs 4` | fewer parallel compiles (less RAM) |
| `-SoBuild` | only recompile (skip unpacking and translating) |

## 5A. Steam Deck, step by step

Everything happens on the Deck, in Desktop Mode. Keep it **plugged in**. You'll type a few commands: a USB or
Bluetooth keyboard helps, or press **Steam + X** for the on-screen keyboard.

1. **Desktop Mode:** press the Steam button → *Power* → *Switch to Desktop*.
2. **Get the source:** open the release page in the browser, download **Source code (tar.gz)** and open
   your *Downloads* folder in **Dolphin** (the file manager). Right-click the file → *Extract* → *Extract archive
   here*. Move the extracted folder to your home folder (`/home/deck`) and rename it `FEW-Recomp` (optional, but the
   commands below assume it).
   > Use the internal storage, or an SD card formatted by SteamOS (ext4). A card formatted as exFAT on a PC cannot
   > hold the game program; the script tells you if that is the case.
3. **Your game files:** inside `FEW-Recomp`, create a folder `Game Files` (right-click → *Create New* → *Folder*)
   and copy your NSPs and `prod.keys` into it (from a USB drive, an SD card or over the network). Or leave the NSPs
   where they are and use `--game-dir` in step 5.
4. **Build without typing anything:** in Dolphin, open the `FEW-Recomp` folder and double-click **`build-linux.sh`**,
   then choose **Execute**. A terminal window opens and shows the build; leave it open until it says *Finished*
   (or shows an error), then press Enter to close it. Skip step 5. If double-clicking opens the file as text instead,
   right-click it → *Properties* → *Permissions* → tick *Is executable*, and try again; or use step 5.
5. **Or build from a terminal:** open the `FEW-Recomp` folder in Dolphin and press **F4** (or right-click → *Open
   Terminal Here*). A terminal panel opens in that folder. Type

   ```bash
   bash build-linux.sh
   ```

   and press Enter. If your NSPs are elsewhere (for example on the SD card), add the folder in quotes — you can drag
   the folder from Dolphin into the terminal to paste its path:

   ```bash
   bash build-linux.sh --game-dir "/run/media/deck/<card>/Switch/Fire Emblem Warriors"
   ```

   The script downloads its tools (about 190 MB; the progress line shows the speed and the time left), unpacks the
   update, translates and compiles the game. The first time this takes **a few hours** on the Deck: leave it plugged
   in with the terminal open (the script keeps the Deck from sleeping). If it gets interrupted, run it again: it
   continues where it stopped, downloads included. Everything it prints is also saved in `build-linux.log`.
   If you have a Windows PC, building there for the Deck ([section 5C](#5c-steam-deck-built-on-a-windows-pc)) is faster.
6. **Add to Steam:** at the end it asks *Close Steam now?* — type `y` and Enter. Steam closes, the game is added
   as **Fire Emblem Warriors** (with artwork), and Steam opens again.
7. **Play:** double-click *Return to Gaming Mode* on the desktop. The game is in your library under **Non-Steam**.

On the Deck the game uses its own Steam Deck settings: battles at 30 fps (the game's native pace) at 1280×720, with
an option for 60 fps. See [section 6](#6-settings-saves-and-logs) for the menu and buttons. The game, its saves and
its log are in `FEW-Recomp/FireEmblemWarriors/`.

## 5B. Linux PC

Requirements are in [section 1](#1-what-you-need); nothing needs `sudo`. In a terminal, in the source folder (after
[step 3](#3-put-your-game-files-in-place)):

```bash
bash build-linux.sh            # build, then offer to add the game to Steam
bash build-linux.sh --launch   # same, then start the game
```

The game ends up in **`FireEmblemWarriors/`** in the source folder: start it with `./FireEmblemWarriors/few-steam.sh`
or from Steam (*Non-Steam* games). It runs on any distribution with glibc 2.35 or newer; native Steam and Flatpak
Steam are both supported. Immutable systems (Bazzite, SteamOS) need nothing extra.

| Option | What it does |
|---|---|
| `--game-dir DIR` | folder with your NSPs (default: `Game Files`) |
| `--keys FILE` | your `prod.keys` (default: in the game folder or next to the script) |
| `--out DIR` | where the game folder goes (default: `FireEmblemWarriors` in the source folder) |
| `--jobs N` | parallel compiles (default: from free RAM; each can take up to ~0.9 GB) |
| `--steam` / `--no-steam` | add to Steam without asking (closes and reopens it) / don't touch Steam |
| `--deck` / `--no-deck` | force the Steam Deck build and settings on or off (detected automatically) |
| `--march CPU` | compile for one CPU family, e.g. `znver3` (default: generic AVX2; `znver2` on a Deck) |
| `--launch` | start the game when the build is done |

Files it creates (all ignored by git): `data/update` (the unpacked update), `generated/` (C code), `build/linux/`
(compiler output and `build.log`), `FireEmblemWarriors/` (the game) and `~/.local/share/FEW-Recomp` (build tools,
shared by every copy of the source). To remove everything, delete those.

## 5C. Steam Deck, built on a Windows PC

Faster than building on the Deck, but the ~20 GB folder has to be copied to the Deck afterwards.

1. Build for Windows first ([section 4](#4-windows)).
2. Install WSL with Ubuntu (once; it asks you to create a Linux user):
   ```powershell
   wsl --install -d Ubuntu-24.04
   ```
   The compile needs memory: if your PC has 16 GB, put this in `%USERPROFILE%\.wslconfig` and run `wsl --shutdown`:
   ```ini
   [wsl2]
   memory=10GB
   ```
3. Build the Deck folder:
   ```powershell
   powershell -ExecutionPolicy Bypass -File .\gerar-deck.ps1 -Distro Ubuntu-24.04
   ```
   The first run downloads the Linux build tools inside WSL by itself and compiles for the Deck's CPU (Zen 2): about
   1.5–2 hours. The result is **`Envio Deck\FireEmblemWarriors\`** ("Deck package"): the game, your NSPs and keys
   (hardlinks: no extra space on the PC), the unpacked update, Deck settings and an installer. It has no fixed paths.
4. Copy the whole `FireEmblemWarriors` folder to the Deck (internal storage or an ext4 SD card), for example to
   `Games/` on the SD card, with a USB drive, a network share, `scp` or KDE Connect. Optional, over SSH from Git Bash
   (resumes interrupted copies and checks SHA-256):
   `DECK_HOST=deck@<deck-ip> DECK_KEY=~/.ssh/<your-key> bash tools/deck/enviar-para-deck.sh`.
5. On the Deck, in Desktop Mode, open the folder in Dolphin, press **F4** and run once:
   ```bash
   sh instalar-no-deck.sh
   ```
   It adds **Fire Emblem Warriors** to Steam (Steam closes and reopens). Back in Gaming Mode it's under *Non-Steam*.

## 6. Settings, saves and logs

The game keeps everything in its own folder — `Jogar no PC\` on Windows, `FireEmblemWarriors/` on Linux and the Deck:

| What | File |
|---|---|
| Saves | `save/` — back this folder up |
| Settings | `hwder_settings.ini` (also changed from the **F1** menu) |
| Log | `hwder.log` |
| Shader caches | `pipeline_cache.bin`, `pipeline_warmup.bin` (stutter goes down as they fill up) |

In game:

- **F1** — or holding **View** (Select) on a controller — opens the runtime menu (graphics options, fps counter).
- **F2** turns 60 fps battles on and off. On the Deck, map a back button to F2 in Steam's controller settings if you
  want it.

Main `hwder_settings.ini` keys (they apply on the next start):

| Key | PC default | Deck default | Meaning |
|---|---|---|---|
| `fps60` | `1` | `0` | battles at 60 fps (the game's Performance mode); `0` = 30 fps Quality mode, the native pace |
| `fps60_1080p` | `0` | `0` | 60 fps battles at 1920×1080 instead of 1280×720 |
| `fps30_720p` | `0` | `1` | 30 fps battles at 1280×720 instead of 1920×1080 |
| `res_630p` | `0` | `1` | at 1280×720, use the game's lower 1120×630 step in heavy scenes |
| `show_fps` | `0` | `1` | fps counter |

Debug switches for advanced users: [docs/environment-variables.md](docs/environment-variables.md). They can go in a
`hwder_env.txt` file in the game folder (`NAME=VALUE`, one per line); the Deck build has one with
`HWDER_FRAME_OVERLAP=1`.

## 7. Updating to a newer version

Get the new source over the old one (`git pull`, or extract the new archive into the same folder, replacing files)
and build again — `gerar-few.ps1` on Windows, `bash build-linux.sh` on Linux and the Deck. Only what changed is
rebuilt, and your saves and settings are kept. `Game Files`, `data`, `generated`, `build` and the game folder must
stay where they are.

For a Deck folder built on Windows (5C): run `gerar-deck.ps1` again and copy only `few` and `few-steam.sh` to the
Deck; it keeps its own settings and saves.

## 8. Troubleshooting

**Windows** (the script's messages are in Portuguese; the left column shows how each one starts):

| Message / symptom | What to do |
|---|---|
| `running scripts is disabled on this system` | Use `powershell -ExecutionPolicy Bypass -File .\gerar-few.ps1` |
| `Esperava 1 NSP da base …` / `… do update …` | Base or update NSP missing, duplicated, or its name lacks the title ID in brackets |
| `prod.keys nao encontrado` | Put `prod.keys` in `Game Files` or next to the script, or pass `-Keys` |
| `Sem toolchain privado e sem Python no sistema` | Install Python 3 (with *Add to PATH*) and run again |
| `Falha ao desempacotar o update` | The line starting with `update:` just above says why (wrong file, a key missing from `prod.keys`, or not update 1.5.0) |
| `Compilacao falhou` | Run again (it resumes); if it keeps failing, try `-Jobs 4` and report it |
| `O jogo esta aberto` | Close `few.exe` before rebuilding |

**Linux and Steam Deck:**

| Message / symptom | What to do |
|---|---|
| `python3 was not found` | Install Python 3 with your package manager (SteamOS and Bazzite already have it) |
| `... is on a exfat drive` / `does not allow running programs` | Move the source folder to the internal storage (`/home/deck`) or an ext4/btrfs drive; NSPs can stay where they are with `--game-dir` |
| `Not enough free space` | Free about 8 GB on that drive |
| `Expected 1 base game NSP …` / `… update 1.5.0 NSP …` | Check the file names have the title IDs in brackets and the files are in `Game Files` (or `--game-dir`) |
| `Could not download …` | Check the internet connection and run again: the download continues where it stopped |
| A download seems stuck | The progress line shows the speed and the time left; a dropped connection is retried by itself. If you close the window, the next run continues the download |
| `Could not unpack the update` | The line starting with `update:` just above says why: not the US update 1.5.0, a key missing from `prod.keys` (dump the keys again), or a damaged NSP (copy it again) |
| Double-clicking `build-linux.sh` opens it as text | Right-click → *Properties* → *Permissions* → *Is executable*, or run `bash build-linux.sh` in a terminal |
| `Compiling failed` / `Killed` / `signal 9` during the compile | Out of memory: close other programs and run `bash build-linux.sh --jobs 2` (it resumes). Log: `build/linux/build.log` |
| The build stopped when the screen turned off | Run the command again; it continues. Keep the Deck plugged in |
| Not in Steam | Sign in to Steam once, then `bash build-linux.sh --steam` |
| The game closes right away | Run `./FireEmblemWarriors/few-steam.sh` in a terminal to see the error, and read `FireEmblemWarriors/hwder.log` |

**Both:**

| Symptom | What to do |
|---|---|
| Stutter in the first minutes or in new scenes | Shaders compiling; it improves as the caches fill up |
| Black screen or crash at start | Update the GPU driver; read `hwder.log` |

## 9. Reporting a problem

Open an issue at <https://github.com/Azzulin/FEW-Recomp/issues> with:

- where you play (Windows with CPU/GPU/driver, Linux distribution and GPU, or Steam Deck) and the release or commit;
- what you did and what happened (a screenshot or short clip helps);
- `hwder.log` from the game folder, and for build problems the end of the script's output (on Linux, the whole
  output is in `build-linux.log` next to the script, and the compiler's in `build/linux/build.log`).

**Never attach game files, keys, the built `few`/`few.exe` or saves.** The logs contain none of them.
