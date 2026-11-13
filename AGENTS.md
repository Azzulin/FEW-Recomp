# Fire Emblem Warriors recomp — instruções para agentes

Recompilação estática de *Fire Emblem Warriors* (Switch, ARM64) para x86-64 nativo, sobre o runtime do HWDER
(Hyrule Warriors: DE). Projeto pessoal: o dono usa a própria cópia do jogo.

## Comece por aqui

1. [docs/handover.md](docs/handover.md): estado, como testar, pendências e armadilhas. Atualize ao terminar.
2. [README.md](README.md): o que é e como gerar.
3. [docs/few.md](docs/few.md): fatos medidos no dump (módulos, hashes, SDK, RomFS, DLC, vídeos). Fonte de verdade sobre o jogo.
4. [docs/how-it-works.md](docs/how-it-works.md): arquitetura herdada do HWDER (recompilador, kernel, serviços, GPU).

## Decisões tomadas

- Alvo: **US base v0 (RomFS) + update 1.5.0 (ExeFS inteiro) + DLCs**. O código recompilado é só o ExeFS do update
  (SDK 3.7.0, 5 módulos, layout do HWDE). A base v0 (SDK 1.6.0, 8 módulos) não é recompilada.
- Windows primeiro; Steam Deck depois (o HWDER já tem o caminho Linux).
- Identidade do jogo num lugar só: `runtime/include/hwder/game_profile.h` (C++) e `tools/game_profile.py` (Python).
- Ganchos do HWDE (`runtime/src/game`, `runtime/src/interp`) desligados por `HW_GAME_HOOKS=OFF` até existirem
  símbolos do FEW em `tools/recomp/symbols.txt`. Os símbolos do HWDE ficam em `tools/recomp/games/hwde/` como
  referência (mesma engine), nunca como endereços válidos aqui.

## Fronteira de conteúdo do jogo

- Dump e chaves em `Game Files/` e `prod.keys` na raiz: ignorados. **Valores de chave nunca aparecem em saída, log,
  comando ou arquivo.**
- Tudo derivado do dump vai para `data/`, `generated/`, `build/`, `Jogar no PC/` (ignorados).
- Nunca commitar C gerado, ExeFS/RomFS, executável do jogo, saves ou caches.

## Código

- C gerado nunca é editado à mão: mude o gerador (`tools/recomp/`) e gere de novo.
- Patches de jogo são ganchos por símbolo (`symbols.txt` + `runtime/src/game`), nunca edição de saída.
- Copiar só código MIT/BSD/ISC/zlib/Apache-2.0. yuzu/suyu/Ryujinx (GPL): só ler para entender comportamento.

## Idioma

Documentação em português (pt-BR). Código e identificadores em inglês, como no HWDER. Mensagens de commit são frases
simples, sem prefixos de tipo.
