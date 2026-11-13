# Handover — estado e próximos passos

Atualizado em 2026-11-13. Leia antes de qualquer tarefa; atualize ao terminar uma sessão.

## Estado

O jogo **roda nativo no Windows** a partir de base US v0 + update 1.5.0 + 5 DLCs:

- boot, logos, vídeo de abertura (H.264) sem artefatos;
- criação de save, notas do update 1.5.0, menus de novo jogo;
- cutscene do prólogo e **a batalha do prólogo** com cenário, multidão, efeitos, diálogos, HUD, minimapa e KO;
- as 5 DLCs são listadas (`aoc:u`) e montadas pelo jogo (`OpenDataStorageByDataId`) no boot;
- ritmo: o próprio jogo pede 30 fps na batalha (como no Switch) e 60 nos menus; o PC (i5-11400H + RTX 3050)
  sobra folga com 1.000–1.300 draws por quadro.

Testado só por roteiro automático até o começo do prólogo. **Ainda não houve uma sessão de jogo longa** com
controle na mão.

## Como trabalhar

| Tarefa | Comando |
|---|---|
| Gerar tudo e publicar | `.\gerar-few.ps1` (`-SoBuild` pula update e geração do C) |
| Teste sem supervisão | `powershell -File tools\run_test.ps1 -Seconds 120 -Script tools\input\prologue_battle.txt -Shots "60,100" -Tag x -Fresh` |
| Prints | `build\shots\<tag>_<s>.png`; log em `build\run\hwder.log` |
| Entradas descobertas em execução | o runtime grava `extra_entries.txt` ao lado do exe; `gerar-few.ps1` junta em `tools/recomp/extra_entries.txt` e regenera |
| Fatos do dump | `tools/inspect_dump.py`, `tools/module_census.py`, `tools/movie_info.py`, `tools/service_names.py` |

Testes rodam em `build\run` (save, ajustes e caches próprios), nunca em `Jogar no PC`, que é do jogador.
O toolchain é o privado do HWDER, em `%LOCALAPPDATA%\Packages\Claude_*\LocalCache\Local\HWDER\managed-tools\…`
(instalado pelo app do Claude); `gerar-few.ps1` o encontra sozinho ou instala um igual.

## Correções feitas no runtime herdado (valem também para o HWDER)

1. **`TLDS` (texelFetch compacto)** não existia no tradutor de shaders: virava no-op, a neblina lia profundidade
   0 e a batalha saía como silhuetas roxas. `shader/emit_mem.cpp`.
2. **PPS do H.264 com uma referência a menos**: o NVDEC entrega `num_ref_idx_l{0,1}_default_active_minus1` e o
   HWDER subtraía 1 de novo. Vídeos com 2 referências (os do FEW) saíam com macroblocos trocados.
   `video/decoder.cpp`.
3. `fs/nsp.cpp`: com update, o ExeFS do update é o único (o FEW 1.5.0 troca o conjunto de módulos).

## Pendências, por prioridade

1. **Jogar de verdade** (o dono): campanha, History Mode com as DLCs, troca de personagem, 2 jogadores.
   Anotar onde algo quebra; o log fica em `Jogar no PC\hwder.log`.
2. **Instruções de shader sem tradução** (hoje no-op, aparecem no log como `[shader] unimplemented`):
   `LDG`/`STG` (memória global), `ATOM`/`ATOMS`/`RED` (atômicos), `SUST` (escrita em imagem), `AL2P` + `IPA`
   indexado, `S2R SR 57`. Nenhum defeito visível foi ligado a elas ainda; provavelmente efeitos em compute.
   Referência permitida: tradutor do Ryujinx (MIT). yuzu/suyu são GPL: só ler.
3. **Escala de resolução > 1x**: travada em 1x (`texture_cache.cpp res_scale_allowed`). Em 2x a composição cobre
   só o quadrante superior esquerdo. Diagnóstico provável, não confirmado: passes (compute, ou fetches a partir de
   `gl_FragCoord`) que endereçam alvos escalados em texels do guest. O caminho é o do escalonador do Ryujinx:
   `gl_FragCoord` no espaço do guest e compute escalado. O `TLDS` já escala as coordenadas das texturas marcadas
   (`PushConstants::scaled_textures`). Confirmar primeiro com `HWDER_VK_FRAME_DUMP` em 2x
   (`HWDER_ALLOW_RES_SCALE=1`).
4. **Ganchos do jogo** (`HW_GAME_HOOKS=OFF`): 120 fps, ultrawide e LOD do HWDE dependem de símbolos. Achar as
   funções equivalentes do KTGL no FEW (`tools/recomp/games/hwde/symbols.txt` como guia; `remap_symbols.py`
   compara corpos de função) e só então religar.
5. **Nomes do save**: o jogo grava `system` em `save/0100F15003E64000/<usuário>/`; os do progresso ainda não foram
   vistos. Ajustar `HW_GAME_SAVE_MAIN/SIDE` em `game_profile.h` (só afetam o seletor de save do menu F1).
6. **Steam Deck / Linux**: o caminho do HWDER (`tools/linux_setup.py`, build no WSL) ainda não foi adaptado.

## Armadilhas já encontradas

- O primeiro dump era o japonês v0 (`0100EAE003A5A000`): foi descartado. O alvo é o US com update.
- `-Filter` do `Get-ChildItem` não entende colchetes de nome de NSP: use `Where-Object { $_.Name -like '*`[Base`]*' }`.
- Windows PowerShell 5.1: `$PSScriptRoot` vazio em valores padrão de `param()`; aspas duplas dentro de
  here-string quebram argumentos de `git commit -m` (use `-F arquivo`).
- O guarda de segurança da sessão bloqueia `Remove-Item` com curinga; apague por caminho literal.
- O autoplay do HWDE (`HWDER_AUTOPLAY`) é roteiro de menu do Hyrule Warriors; use `HWDER_INPUT_SCRIPT`.
