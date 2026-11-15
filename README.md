# Fire Emblem Warriors — recompilação estática para PC

Port nativo de *Fire Emblem Warriors* (Switch) para Windows x64, gerado a partir da cópia do próprio dono. O código
ARM64 do jogo é traduzido para C antes de rodar e compilado com Clang; o sistema do Switch (kernel Horizon, serviços,
GPU Maxwell, áudio, vídeo, sistema de arquivos) é implementado pelo runtime. Sem emulador, sem JIT.

O runtime vem do [HWDER](https://github.com/blocker-3/HWDER) v0.3.5 (MIT), o port de *Hyrule Warriors: Definitive
Edition*: mesma engine (Koei Tecmo KTGL sobre NVN) e, no update 1.5.0 do Fire Emblem Warriors, o mesmo conjunto de
módulos e quase o mesmo SDK (3.7.0 contra 3.5.1). Detalhes em [docs/few.md](docs/few.md) e
[docs/how-it-works.md](docs/how-it-works.md).

**Este repositório não contém nada do jogo**: nem dump, nem chaves, nem código traduzido, nem executável gerado.

## Estado

Roda nativo no Windows e no Steam Deck: boot, vídeos, menus, criação de save, cutscenes e a batalha do prólogo
(com as partículas da GPU), com as cinco DLCs montadas pelo jogo. No PC a batalha roda a 60 fps no modo
Desempenho do próprio jogo, ligado pelo runtime (`fps60=1`, a 720p como no Switch; `fps60_1080p=1` para 1080p).
No Steam Deck a batalha roda no modo Qualidade (30 fps, o ritmo nativo do FEW) a 720p, com as partículas; a luta
densa fica em 26–28 fps, com quedas a ~20 fps nos trechos mais pesados (trabalho em andamento, ver o handover). Os extras do HWDER que dependem de endereços do
Hyrule Warriors (120 fps, ultrawide, LOD do cenário) estão desligados (`HW_GAME_HOOKS=OFF`) e a escala de resolução
fica em 1x por enquanto. Detalhes, pendências e armadilhas em [docs/handover.md](docs/handover.md).

## Arquivos do jogo

Coloque em `Game Files\` (ignorado pelo git):

```
Game Files\
  Fire Emblem Warriors [0100F15003E64000][v0][Base][US].nsp
  Fire Emblem Warriors [0100F15003E64800][v327680][1.5.0][UPD][US].nsp
  Fire Emblem Warriors [...][0100F15003E6500x][US][...].nsp      # DLCs: lidas da mesma pasta
prod.keys                                                     # na raiz ou em Game Files\
```

## Gerar e jogar (Windows)

```powershell
.\gerar-few.ps1          # update -> C -> compila -> publica em ".\Jogar no PC"
.\gerar-few.ps1 -Launch  # idem e abre
```

Usa o toolchain privado do HWDER (Python, LLVM-MinGW, CMake, Ninja), sem Visual Studio e sem admin; se ele não
existir na máquina, o script instala um igual (`tools/managed_tools.py`). Saídas: `data\update` (update
desempacotado), `generated\` (C), `build\pc` (objetos), `Jogar no PC\few.exe`. Tudo ignorado pelo git.

## Steam Deck

```powershell
.\gerar-deck.ps1                          # depois do gerar-few.ps1: build Linux no WSL para o Zen 2 do Deck
bash tools/deck/enviar-para-deck.sh       # Git Bash: envia por SSH à pasta Games do cartão SD
```

Compila no WSL (archlinux) com o toolchain Linux do HWDER (glibc 2.35) e `-march=znver2`, monta
`Envio Deck\FireEmblemWarriors` (jogo, NSPs e chaves por hardlink, update, ajustes) e envia. No Deck, uma vez:
`sh instalar-no-deck.sh` na pasta do jogo cria o atalho na Steam. No Deck as partículas da GPU ficam desligadas
(`HWDER_GMEM=0` no lançador): com elas a batalha cai para ~3,5 fps; sem elas roda a 30 fps. Detalhes em
[docs/handover.md](docs/handover.md#steam-deck).

## Licença

MIT (ver `LICENSE`), herdada do HWDER. *Fire Emblem Warriors* pertence à Nintendo e à Koei Tecmo.
