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

Fase 1: gerar o C do update 1.5.0, compilar e rodar até o primeiro problema de boot. Os extras do HWDER que dependem
de endereços do Hyrule Warriors (120 fps, ultrawide, LOD do cenário) estão desligados (`HW_GAME_HOOKS=OFF`).
DLC ainda não é servida ao jogo (`aoc:u` responde "nenhuma").

## Arquivos do jogo

Coloque em `Game Files\` (ignorado pelo git):

```
Game Files\
  Fire Emblem Warriors [0100F15003E64000][v0][Base][US].nsp
  Fire Emblem Warriors [0100F15003E64800][v327680][1.5.0][UPD][US].nsp
  Fire Emblem Warriors [...][0100F15003E6500x][US][...].nsp      # DLCs (ainda não usadas)
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

## Licença

MIT (ver `LICENSE`), herdada do HWDER. *Fire Emblem Warriors* pertence à Nintendo e à Koei Tecmo.
