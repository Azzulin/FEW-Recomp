# FEW Recomp — build and play guide

Step-by-step instructions to build *Fire Emblem Warriors* from **your own copy** and play it on Windows or the
Steam Deck. Nothing of the game is in this repository or in the releases: your PC builds the game from your dump.

> [!IMPORTANT]
> You need your own legally obtained copy of the game, dumped from your own Nintendo Switch, and the `prod.keys`
> from that same console. This project does not provide game files or keys and will not explain how to get them.
> What you build contains the game and is for your own use only: do not share it.

- [1. Requirements](#1-requirements)
- [2. Get the source](#2-get-the-source)
- [3. Put your game files in place](#3-put-your-game-files-in-place)
- [4. Build and play on Windows](#4-build-and-play-on-windows)
- [5. Settings, saves and logs](#5-settings-saves-and-logs)
- [6. Steam Deck](#6-steam-deck)
- [7. Updating to a newer version](#7-updating-to-a-newer-version)
- [8. Troubleshooting](#8-troubleshooting)
- [9. Reporting a problem](#9-reporting-a-problem)

## 1. Requirements

**To build (Windows PC):**

- Windows 10 or 11, x64.
- [Git for Windows](https://git-scm.com/download/win) (the build script fetches four pinned libraries with it).
- [Python 3](https://www.python.org/downloads/windows/) on the `PATH`, only for the first run: it installs the
  private build tools (Python 3.13, LLVM-MinGW, CMake, Ninja and five Python packages, each checked against a fixed
  SHA-256) into `%LOCALAPPDATA%\HWDER`. No Visual Studio, no admin rights, no system changes. If you already use
  [HWDER](https://github.com/blocker-3/HWDER), its tools are reused.
- About **5 GB** free next to the repository (unpacked update, generated C, objects), plus your NSPs (~19 GB with
  all DLCs). Internet access the first time (~400 MB of tools).
- Time: the first build translates and compiles the whole game; expect roughly **45–90 minutes**, depending on the
  CPU. Later builds only redo what changed.

**To play:**

- Windows PC: x86-64 CPU with AVX2 and a GPU with a Vulkan 1.2+ driver. Tested on an RTX 3050 laptop.
- Steam Deck (SteamOS 3.x): built on the PC (see [section 6](#6-steam-deck)); the Deck itself compiles nothing.

**Game files (all US, from your own console):**

| File | Title ID | Version |
|---|---|---|
| Base game | `0100F15003E64000` | v0 |
| Update | `0100F15003E64800` | **1.5.0** (v327680) — required: the recompiled code is the update's |
| DLCs (optional) | `0100F15003E65001` … `0100F15003E65005` | any |
| `prod.keys` | — | from the same console |

Other regions or update versions are not supported yet.

## 2. Get the source

Either clone it:

```powershell
git clone https://github.com/Azzulin/FEW-Recomp.git
cd FEW-Recomp
```

or download `Source code (zip)` from the [Releases](https://github.com/Azzulin/FEW-Recomp/releases) page and
extract it to a folder **with a short path and no special characters** (for example `C:\Games\FEW-Recomp`).

## 3. Put your game files in place

Create a folder named `Game Files` in the repository root and put your NSPs and keys there:

```
FEW-Recomp\
  Game Files\
    Fire Emblem Warriors [0100F15003E64000][v0][Base][US].nsp
    Fire Emblem Warriors [0100F15003E64800][v327680][1.5.0][UPD][US].nsp
    Fire Emblem Warriors [...][0100F15003E65001][US].nsp     <- DLCs, optional, any name
    prod.keys                                                <- here or in the repository root
```

The scripts find each file by the **title ID in square brackets** in its name, so keep the `[0100F15003E64000]`
and `[0100F15003E64800]` tags. Everything in `Game Files` (and `prod.keys` anywhere) is ignored by git and is never
uploaded. NSPs and keys are read where they are; they are not copied.

## 4. Build and play on Windows

Open **PowerShell** in the repository folder and run:

```powershell
powershell -ExecutionPolicy Bypass -File .\gerar-few.ps1 -Launch
```

(`-ExecutionPolicy Bypass` is only needed because the script is not signed. Leave out `-Launch` to build without
starting the game.)

The script:

1. checks your files (it stops with a message if the base, the update or `prod.keys` is missing);
2. fetches the pinned third-party sources into `third_party\` if they are missing;
3. finds or installs the private build tools;
4. unpacks update 1.5.0 into `data\update`;
5. translates the game's ARM64 code to C in `generated\`;
6. compiles in `build\pc` and publishes **`Jogar no PC\few.exe`** with a `hwder_settings.ini` pointing at your
   files.

If it stops halfway (closed window, sleep, error), run it again: it resumes. Afterwards, start the game with
`Jogar no PC\few.exe` (you can make a shortcut or add it to Steam as a non-Steam game).

Useful options:

| Command | What it does |
|---|---|
| `.\gerar-few.ps1 -Jogo D:\Switch\FEW` | read the NSPs from another folder instead of `Game Files` |
| `.\gerar-few.ps1 -Keys D:\keys\prod.keys` | use keys from another place |
| `.\gerar-few.ps1 -Jobs 4` | fewer parallel compiles (less RAM; each one can take ~0.7 GB) |
| `.\gerar-few.ps1 -SoBuild` | skip unpacking and translation; only recompile |

## 5. Settings, saves and logs

Everything lives in `Jogar no PC\`:

| What | Where |
|---|---|
| Saves | `Jogar no PC\save\` — back this folder up |
| Settings | `Jogar no PC\hwder_settings.ini` (also editable from the in-game **F1** menu) |
| Log | `Jogar no PC\hwder.log` |
| Shader caches | `pipeline_cache.bin`, `pipeline_warmup.bin` (stutter goes down as they fill up) |

In game:

- **F1** (or holding View/Select on a controller) opens the runtime menu.
- **F2** toggles 60 fps battles on and off.

Main `hwder_settings.ini` keys (they take effect on the next start):

| Key | Default (PC) | Meaning |
|---|---|---|
| `fps60` | `1` | battles at 60 fps (the game's Performance mode). `0` = Quality mode, 30 fps, the game's native pace |
| `fps60_1080p` | `0` | render 60 fps battles at 1920×1080 instead of 1280×720 |
| `fps30_720p` | `0` | render 30 fps battles at 1280×720 |
| `show_fps` | `0` | fps counter |

Extra debug switches for advanced users: [docs/environment-variables.md](docs/environment-variables.md). They can
also go in a `hwder_env.txt` file next to the game (`NAME=VALUE`, one per line).

## 6. Steam Deck

The Linux binary is compiled on your Windows PC inside WSL, then copied to the Deck together with your game files.
This takes the Windows build (section 4) plus about **1.5–2 hours** of compiling the first time.

### 6.1 One-time WSL setup

1. Install WSL with a Linux distro. Arch Linux is what the project uses; Ubuntu 22.04/24.04 also works:
   ```powershell
   wsl --install -d Ubuntu-24.04
   ```
   Give WSL enough memory for the parallel compiles (10 GB is what was tested): in `%USERPROFILE%\.wslconfig`,
   ```ini
   [wsl2]
   memory=10GB
   ```
2. Install the HWDER v0.3.5 Linux build tools and libraries inside WSL (as root, in `/root/hwder`):
   ```powershell
   wsl -d Ubuntu-24.04 -u root
   ```
   ```bash
   mkdir -p /root/hwder/release && cd /root/hwder/release
   curl -LO https://github.com/blocker-3/HWDER/releases/download/v0.3.5/HWDER-linux-x86_64-v0.3.5.tar.gz
   tar xzf HWDER-linux-x86_64-v0.3.5.tar.gz
   XDG_DATA_HOME=/root/hwder/data python3 HWDER-linux-x86_64-v0.3.5/tools/linux_setup.py toolchain
   exit
   ```
   This gives `/root/hwder/release/HWDER-linux-x86_64-v0.3.5/linux-deps` (static SDL3, FFmpeg, libcrypto) and the
   Zig/Clang toolchain in `/root/hwder/data/HWDER/toolchain/`. The result runs on any SteamOS 3 (glibc 2.35).

### 6.2 Build the Deck folder

After `gerar-few.ps1` has finished at least once:

```powershell
powershell -ExecutionPolicy Bypass -File .\gerar-deck.ps1 -Distro Ubuntu-24.04
```

(`-Distro` defaults to `archlinux`. `-March ""` builds for any AVX2 Linux PC instead of the Deck's Zen 2 CPU.)

It produces **`Envio Deck\FireEmblemWarriors\`** (~20 GB): the `few` binary, your NSPs and keys (as hardlinks, so
they take no extra space on the PC), the unpacked update, Deck settings, a Steam launcher and an installer. The
folder has no fixed paths; you can put it anywhere on the Deck.

### 6.3 Copy it to the Deck

Copy the whole `FireEmblemWarriors` folder to the Deck — for example to `Games/` on the SD card — with any method
you like (USB drive, network share, `scp`, KDE Connect). The SD card or drive must be **ext4** (SteamOS's default
format): exFAT/FAT cannot hold executable files.

Optional, over SSH from Git Bash (resumes interrupted copies and checks SHA-256 on both sides): enable SSH on the
Deck, then

```bash
DECK_HOST=deck@<deck-ip> DECK_KEY=~/.ssh/<your-key> bash tools/deck/enviar-para-deck.sh
```

It copies to `Games/FireEmblemWarriors` on the first SD card that has a `Games` folder.

### 6.4 First start on the Deck

In **Desktop Mode**, open the `FireEmblemWarriors` folder in Dolphin, press **F4** (terminal) and run once:

```bash
sh instalar-no-deck.sh
```

It marks the game as executable and adds **Fire Emblem Warriors** to Steam (Steam closes and reopens). Back in
Game Mode, the game is in the library under *Non-Steam*. Run the script again if you move the folder.

On the Deck, battles run at 30 fps (the game's native pace) with the 3D at 1280×720; the F1 menu has the 60 fps
option. Saves, settings and `hwder.log` are inside the game folder.

## 7. Updating to a newer version

```powershell
git pull
powershell -ExecutionPolicy Bypass -File .\gerar-few.ps1
```

(or extract the new source zip over the old folder, keeping `Game Files`, `data`, `generated`, `build` and
`Jogar no PC`). Only what changed is rebuilt. Your saves in `Jogar no PC\save` are kept. For the Deck, run
`gerar-deck.ps1` again and copy only the new `few` and `few-steam.sh` (the Deck keeps its own `hwder_settings.ini`
and `save\`).

## 8. Troubleshooting

| Message / symptom | What to do |
|---|---|
| `running scripts is disabled on this system` | Run the script with `powershell -ExecutionPolicy Bypass -File .\gerar-few.ps1` |
| `Esperava 1 NSP da base [0100F15003E64000]` / `do update [0100F15003E64800]` | The base or update NSP is missing from `Game Files`, there are two of them, or the name lacks the title ID in brackets |
| `prod.keys nao encontrado` | Put `prod.keys` in `Game Files` or the repository root, or pass `-Keys <path>` |
| `Sem toolchain privado e sem Python no sistema` | Install Python 3 from python.org (tick *Add to PATH*) and run again |
| `Falha ao desempacotar o update` | Your keys don't match the dump, or the update is not 1.5.0 |
| `Compilacao falhou` | Run again (it resumes). If it keeps failing at the same file, try `-Jobs 4` (low RAM) and report it |
| `O jogo esta aberto` | Close `few.exe` before rebuilding |
| Game window closes at start | Read `Jogar no PC\hwder.log`; update your GPU driver; check Vulkan support |
| Stutter the first minutes | Shaders compiling; it improves as the caches fill up |
| Deck: `falta linux-deps` / `falta o toolchain do HWDER` | Redo [6.1](#61-one-time-wsl-setup) in the same distro you pass to `-Distro` |
| Deck: `o cartão não aceita arquivos executáveis` | The card is exFAT/FAT; use an ext4 card or the internal storage |

The script messages are in Portuguese; the left column above shows the start of each one.

## 9. Reporting a problem

Open an issue at <https://github.com/Azzulin/FEW-Recomp/issues> with:

- where you play (Windows PC with CPU/GPU/driver, or Steam Deck), and the commit or release you built;
- what you did and what happened (a screenshot or a short clip helps);
- `hwder.log` from the game folder.

**Never attach game files, keys, `few.exe`/`few` or saves.** The log contains none of them.
