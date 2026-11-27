# FEW Recomp

**[English](#english)** · **[Português](#português)**

## English

Static recompilation of *Fire Emblem Warriors* (Nintendo Switch, update 1.5.0) into a native x86-64 program with
Vulkan rendering, for **Windows**, **Linux** and the **Steam Deck**. The game's ARM64 code is translated to C ahead of
time and compiled with Clang; a runtime implements the Switch side (Horizon kernel, services, NVN/Maxwell GPU over Vulkan,
audio, video, file system). No emulator, no JIT.

> [!IMPORTANT]
> **No game files are included** — no dump, keys, translated code or executable, in the repository or in the
> releases. You need your own copy, legally obtained and dumped from your own console: the base game, update
> **1.5.0** and, optionally, the DLCs, plus your `prod.keys`. Instructions for getting those files will not be
> given. The game is built from your files, on your machine; the result is for your own use only.

### How to play

You need your NSPs — base `[0100F15003E64000]`, update 1.5.0 `[0100F15003E64800]`, DLCs (optional) — and your
`prod.keys`. The Japanese release (`[0100EAE003A5A000]`, update `[0100EAE003A5A800]`) is also accepted: a player
reports it running in Japanese at 60 fps.

**Windows:** download **`FEW-Recomp-<version>-Windows-x64.zip`** from the latest
[release](https://github.com/Azzulin/FEW-Recomp/releases), extract it and open **FEW Recomp Setup.exe**. Pick the
folder with your files (it finds the base, the update, the DLCs and the keys by themselves), press **Build the game**,
then **Play**.

**Steam Deck / Linux:** download `FEW-Recomp-<version>-Linux-x64.tar.gz`, put your files in a `Game Files` folder
inside it and double-click `build-linux.sh` (choose *Execute*), or run `bash build-linux.sh`. The game ends up in
`FireEmblemWarriors/` with a Steam shortcut.

Nothing is installed on the system and no admin/`sudo` is needed: pinned build tools are downloaded once and checked
by SHA-256. The first build takes about 20–45 minutes on a desktop PC and longer on a Steam Deck; a window shows its
steps, progress and time left, with a **Play** button at the end. It resumes if interrupted, and later updates only
rebuild what changed.

**Step-by-step guide, including a Steam Deck walkthrough for people who never used Desktop Mode:
[GUIDE.md](GUIDE.md).** It also covers building the Deck version on a Windows PC, settings, saves, updating and
troubleshooting.

**RecompLauncher:** from v0.3.0 on, the releases also carry Windows and Linux files that
[RecompLauncher](https://recomplauncher.com) can install and set up (it builds the game from your files, as above);
listing in its catalog is requested. See [GUIDE.md, section 5D](GUIDE.md#5d-with-recomplauncher).

### Status

- **Windows:** boot, videos, menus, save creation, cutscenes and battles, with the five DLCs mounted. Battles run
  at 60 fps in the game's own Performance mode (1280×720 like the Switch; 1080p optional) or 30 fps in Quality
  mode.
- **Linux:** built natively, like the Deck version, for any distribution with glibc 2.35+ (SteamOS, Bazzite, Arch,
  Fedora, Ubuntu 22.04+) with `build-linux.sh`; not yet played end to end outside the Deck.
- **Steam Deck:** battles at 30 fps (the game's native pace) at 1280×720 with GPU particles; the densest fights can
  still dip below 30. 60 fps is an option in the F1 menu.
- **Known limits:** resolution scaling stays at 1×; HWDER's extras tied to *Hyrule Warriors* addresses (120 fps,
  ultrawide, LOD) are off; update 1.5.0 is required; the US release is the one tested here, and the Japanese release
  (also sold in Asia) is reported working by a player; shader stutter the first time each
  scene is seen. Progress notes (Portuguese): [docs/handover.md](docs/handover.md).

### Project focus

This port is different from a typical PC port in a few ways:

- **Performance first, without straining the Steam Deck.** The main goal has been a steady frame rate on handheld
  hardware with the game left intact: no effects are removed to hit a target. On the Deck the game runs at its
  native pace (30 fps battles, as on the Switch) at the screen's 1280×720, with the GPU particles on. The work that
  got there is mostly about doing less per frame rather than pushing the hardware harder:
  - the CPU and GPU work on consecutive frames at the same time (frame overlap) instead of waiting for each other;
  - only the memory pages the game changed are sent to or read back from the GPU;
  - compute dispatches are chained without full barriers, and the shader translator follows AMD's 64-lane waves;
  - the Deck build is compiled for its Zen 2 CPU, with AVX2 `memcmp`/`bcmp`;
  - shaders compile in the background and are cached, so stutter fades with play;
  - the game's own lower resolution step (1120×630) is used in heavy scenes.

  60 fps stays available as an option for those who prefer it over battery life and heat.
- **Built on another game's runtime.** The runtime and recompiler are [HWDER](https://github.com/blocker-3/HWDER)'s,
  made for *Hyrule Warriors: Definitive Edition*, which uses the same Koei Tecmo engine. Only the code of update
  1.5.0 is recompiled. HWDER's patches that depend on *Hyrule Warriors* addresses are off until this game's own
  symbols are mapped.
- **You build it, on your machine.** Nothing of the game is distributed: each player's computer builds the game
  from their own dump. That's why the instructions focus on making the build itself simple, including on the
  Steam Deck with no other computer.

**Where it's heading: mobile devices.** The current interest is taking the project further onto portable and
mobile devices. Handheld PCs (Steam Deck and similar x86-64 devices on Linux or Windows) are the path that exists
today. **There is no Android version:** it hasn't been started, and I don't have a device able to run it for
testing. If you have the hardware and want to help (testing or porting), open an issue.

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
para **Windows**, **Linux** e **Steam Deck**. **Nenhum arquivo do jogo está incluído**: é preciso a sua própria cópia
(base, update 1.5.0, DLCs opcionais) e o seu `prod.keys`, extraídos do seu console. A versão testada é a dos EUA;
a japonesa (vendida também na Ásia) é aceita e um jogador a relata rodando em japonês a 60 fps. O primeiro build leva
uns 20–45 minutos num PC de mesa (mais no Steam Deck), com uma janela que mostra as etapas, o progresso e o tempo
restante.

Para jogar no **Windows**: baixe o `FEW-Recomp-<versão>-Windows-x64.zip` da última release, extraia e abra o
**FEW Recomp Setup.exe**; escolha a pasta com os seus arquivos (ele acha a base, o update, as DLCs e as chaves), clique
em **Build the game** e depois em **Play**. No **Steam Deck e Linux**: baixe o `FEW-Recomp-<versão>-Linux-x64.tar.gz`,
crie `Game Files` com os NSPs e o `prod.keys` e rode `build-linux.sh` (o jogo fica em `FireEmblemWarriors/` e ganha
atalho na Steam). Passo a passo, ajustes e problemas comuns: [GUIDE.md](GUIDE.md) (em inglês).

**Foco do projeto:** desempenho estável sem forçar o Steam Deck e sem cortar efeitos do jogo. No Deck: o ritmo
nativo (batalhas a 30 fps) a 1280×720, com as partículas da GPU. Para isso, o trabalho foi reduzir o que se faz por
quadro, não exigir mais do hardware:
- sobreposição de quadros (CPU e GPU trabalham ao mesmo tempo);
- envio à GPU só das páginas de memória que mudaram;
- código compilado para o Zen 2;
- shaders em segundo plano e em cache.

O runtime vem do HWDER (mesma engine) e cada jogador gera o jogo a partir do próprio dump. **Próximo interesse:
dispositivos móveis.** Os portáteis x86-64 (Steam Deck e afins) já funcionam. **Não existe versão para Android:**
não foi iniciada e não tenho aparelho capaz de rodá-la para testar; quem tiver e quiser ajudar, abra uma issue.

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
