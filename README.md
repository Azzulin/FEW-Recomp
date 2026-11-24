# FEW Recomp

**[English](#english)** · **[Português](#português)**

## English

Static recompilation of *Fire Emblem Warriors* (Nintendo Switch, update 1.5.0) into a native x86-64 program with
Vulkan rendering, for **Windows** and the **Steam Deck**. The game's ARM64 code is translated to C ahead of time and
compiled with Clang; a runtime implements the Switch side (Horizon kernel, services, NVN/Maxwell GPU over Vulkan,
audio, video, file system). No emulator, no JIT.

> [!IMPORTANT]
> **No game files are included** — no dump, keys, translated code or executable, in the repository or in the
> releases. You need your own copy, legally obtained and dumped from your own console: the base game, update
> **1.5.0** and, optionally, the DLCs, plus your `prod.keys`. Instructions for getting those files will not be
> given. The game is built from your files, on your machine; the result is for your own use only.

### How to play

1. Download `Source code (zip)` from the latest [release](https://github.com/Azzulin/FEW-Recomp/releases) (or
   `git clone` this repository) and extract it to a short path, e.g. `C:\Games\FEW-Recomp`.
2. Create `Game Files\` inside it and put there your NSPs — base `[0100F15003E64000]`, update 1.5.0
   `[0100F15003E64800]`, DLCs — and your `prod.keys`.
3. Install [Git](https://git-scm.com/download/win) and [Python 3](https://www.python.org/downloads/windows/) if you
   don't have them, open PowerShell in the folder and run:
   ```powershell
   powershell -ExecutionPolicy Bypass -File .\gerar-few.ps1 -Launch
   ```
   The first build downloads pinned build tools (no admin, no Visual Studio), translates and compiles the game:
   roughly 45–90 minutes. Then play with `Jogar no PC\few.exe`.

Steam Deck build (compiled on the PC in WSL), settings, saves, updating and troubleshooting:
**[GUIDE.md](GUIDE.md)**.

### Status

- **Windows:** boot, videos, menus, save creation, cutscenes and battles, with the five DLCs mounted. Battles run
  at 60 fps in the game's own Performance mode (1280×720 like the Switch; 1080p optional) or 30 fps in Quality
  mode.
- **Steam Deck:** battles at 30 fps (the game's native pace) at 1280×720 with GPU particles; the densest fights can
  still dip below 30. 60 fps is an option in the F1 menu.
- **Known limits:** resolution scaling stays at 1×; HWDER's extras tied to *Hyrule Warriors* addresses (120 fps,
  ultrawide, LOD) are off; only the US release with update 1.5.0 is supported; shader stutter the first time each
  scene is seen. Progress notes (Portuguese): [docs/handover.md](docs/handover.md).

### Found a problem?

Open an issue at <https://github.com/Azzulin/FEW-Recomp/issues> with your hardware, what happened and the
`hwder.log` from the game folder ([how to report](GUIDE.md#9-reporting-a-problem)). Never attach game files, keys,
the built executable or saves.

### Credits

The runtime and recompiler come from [HWDER](https://github.com/blocker-3/HWDER) v0.3.5 (MIT), the port of
*Hyrule Warriors: Definitive Edition*: same engine (Koei Tecmo KTGL over NVN) and, in FEW update 1.5.0, the same
module set and almost the same SDK (3.7.0 vs 3.5.1). Third-party sources are fetched at pinned commits
([third_party/README.md](third_party/README.md)).

## Português

Recompilação estática de *Fire Emblem Warriors* (Switch, update 1.5.0) para um programa nativo x86-64 com Vulkan,
para **Windows** e **Steam Deck**. **Nenhum arquivo do jogo está incluído**: é preciso a sua própria cópia (base,
update 1.5.0, DLCs opcionais) e o seu `prod.keys`, extraídos do seu console.

Para jogar: baixe o código da última release (ou clone), crie `Game Files\` com os NSPs e o `prod.keys` e rode
`powershell -ExecutionPolicy Bypass -File .\gerar-few.ps1 -Launch`. O jogo fica em `Jogar no PC\few.exe`. Steam
Deck, ajustes e problemas comuns: [GUIDE.md](GUIDE.md) (em inglês).

Documentação técnica (pt-BR):

| Documento | Conteúdo |
|---|---|
| [docs/handover.md](docs/handover.md) | estado atual, como testar, pendências e armadilhas |
| [docs/few.md](docs/few.md) | fatos medidos no dump (módulos, hashes, SDK, RomFS, DLC, vídeos) |
| [docs/how-it-works.md](docs/how-it-works.md) | arquitetura herdada do HWDER (recompilador, kernel, serviços, GPU) |
| [docs/environment-variables.md](docs/environment-variables.md) | variáveis de diagnóstico (`HWDER_*`) |
| [AGENTS.md](AGENTS.md) | regras do projeto para quem desenvolve (pessoa ou agente) |

Os documentos `docs/playing.md`, `docs/releasing.md`, `docs/windows-setup*.md`, `docs/linux-release.md` e
`docs/antivirus.md` descrevem o *HWDER Setup* do projeto de origem, que não é usado aqui; ficam como referência.

## License

MIT ([LICENSE](LICENSE)), inherited from HWDER. Third-party code keeps its own license. The license grants nothing
about the game. Fan project, non-commercial, not affiliated with Nintendo or Koei Tecmo; *Fire Emblem Warriors* and
*Nintendo Switch* are trademarks of their respective owners.
