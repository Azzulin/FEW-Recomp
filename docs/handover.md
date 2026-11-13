# Handover — estado e próximos passos

Atualizado em 2026-11-13, fim da terceira rodada: travamento da batalha resolvido, memória global nos shaders de
cálculo, build do Steam Deck. Leia antes de qualquer tarefa; atualize ao terminar uma sessão.

## Estado

O jogo **roda nativo no Windows** a partir de base US v0 + update 1.5.0 + 5 DLCs:

- boot, logos, vídeo de abertura (H.264) sem artefatos;
- criação de save, notas do update 1.5.0, menus de novo jogo;
- cutscene do prólogo e **a batalha do prólogo** com cenário, multidão, efeitos (inclusive as partículas da GPU:
  poeira, riscos de velocidade), diálogos, HUD, minimapa e KO;
- as 5 DLCs são listadas (`aoc:u`) e montadas pelo jogo (`OpenDataStorageByDataId`) no boot.

**O travamento da batalha está resolvido** (seção "Travamento na batalha"): 330 s de `battle_long.txt` com cache
frio, várias vezes, sem travar; antes travava sempre entre 100 e 150 s. Batalha a ~42 ms por quadro em média (o
jogo pede 33), com picos de ~300 ms quando surge um efeito novo (compilação de pipeline).

Tudo está no `main`. O ramo `wip/gpfifo-snapshot` foi incorporado (avanço rápido) e pode ser apagado.

**Steam Deck**: build Linux com `-march=znver2` feito no WSL e enviado ao cartão SD do Deck (seção "Steam Deck").

## Como trabalhar

| Tarefa | Comando |
|---|---|
| Gerar tudo e publicar | `.\gerar-few.ps1` (`-SoBuild` pula update e geração do C) |
| Teste sem supervisão | `powershell -File tools\run_test.ps1 -Seconds 120 -Script tools\input\prologue_battle.txt -Shots "60,100" -Tag x -Fresh` |
| Batalha longa (regressão do travamento) | `powershell -File tools\run_test.ps1 -Seconds 330 -Script tools\input\battle_long.txt -Shots "200" -Tag bl -Fresh` (apague antes `build\run\pipeline_cache.bin` e `pipeline_warmup.bin` para cache frio) |
| Perfil da thread da GPU | `-Env "HWDER_THREAD_STATS=1"`: a cada 120 quadros, fases (`dispatch`, `gmem`, `present`…), despachos e flushes por origem |
| Shaders e constant buffers | `-Env "HWDER_DUMP_SHADERS=1"`: `build\run\shader_<chave>_<estágio>.maxwell.txt` (SASS) e, em compute, `.cbufs.txt` (QMD + c0) |
| Build do Steam Deck | `.\gerar-deck.ps1` (WSL archlinux, `-march=znver2`) e `bash tools/deck/enviar-para-deck.sh` (seção "Steam Deck") |
| Prints | `build\shots\<tag>_<s>.png`; log em `build\run\hwder.log` |
| Ajustes extras no teste | `-Ini "res_scale=2.000"`, `-Env "HWDER_GPU_TRACE=3;HWDER_IPC_TRACE=1"` |
| Entradas descobertas em execução | o runtime grava `extra_entries.txt` ao lado do exe; `gerar-few.ps1` junta em `tools/recomp/extra_entries.txt` e regenera |
| Fatos do dump | `tools/inspect_dump.py`, `tools/module_census.py`, `tools/movie_info.py`, `tools/service_names.py` |

Testes rodam em `build\run` (save, ajustes e caches próprios), nunca em `Jogar no PC`, que é do jogador.
O toolchain é o privado do HWDER, em `%LOCALAPPDATA%\Packages\Claude_*\LocalCache\Local\HWDER\managed-tools\…`
(instalado pelo app do Claude); `gerar-few.ps1` o encontra sozinho ou instala um igual.

Roteiros de entrada (`tools/input/`, formato em `runtime/src/hid/input_script.h`): `new_game.txt` (save vazio até
o prólogo), `prologue_battle.txt` (luta curta), `battle_long.txt` (5 min de luta variada: ataques, especial,
despertar, esquiva, troca de personagem, câmera), `pause_menu.txt` (abre a pausa na batalha), `a_every_2s.txt`.

## Correções feitas no runtime herdado (valem também para o HWDER)

1. **`TLDS` (texelFetch compacto)** não existia no tradutor de shaders: virava no-op, a neblina lia profundidade
   0 e a batalha saía como silhuetas roxas. `shader/emit_mem.cpp`.
2. **PPS do H.264 com uma referência a menos**: o NVDEC entrega `num_ref_idx_l{0,1}_default_active_minus1` e o
   HWDER subtraía 1 de novo. Vídeos com 2 referências (os do FEW) saíam com macroblocos trocados.
   `video/decoder.cpp`.
3. `fs/nsp.cpp`: com update, o ExeFS do update é o único (o FEW 1.5.0 troca o conjunto de módulos).
4. **Pushbuffer lido entrada por entrada** (`gpu/gpu.cpp process_pushbuffer`): o estado do método (método,
   subcanal, palavras que faltam) zerava a cada entrada do GPFIFO. No hardware o pushbuffer é um fluxo só, e os
   dados de um método podem continuar na entrada seguinte; agora o estado fica no canal (`Channel::pb`).

## Sessão de 2026-11-13: batalha (travamento, memória global, desempenho)

### Travamento na batalha (resolvido)

Assinatura antiga, em todas as ocorrências (sessão do dono aos 156 s; testes entre 100 e 150 s):

```
[148.797] gpu: method 0xe2e on unbound subchannel 0
[149.829] vi: frame fence 1:37861 not reached after 1 s (value 37855) - still waiting
... (repete a cada 10 s; minutos depois "vk: error -2 ... vkAllocateMemory" e "fatal: vulkan")
```

**Causa.** As partículas da batalha usam **draw indireto**: shaders de cálculo (culling e "append" com atômicos)
escrevem numa área de memória quantos vértices e instâncias desenhar, e o NVN chama a macro de desenho 25
(`topologia, contagem, instâncias, primeiro, instância base`) com os 4 últimos parâmetros vindos de uma **entrada
do GPFIFO que aponta direto para essa memória**. Duas falhas do runtime se somavam:

1. o leitor do pushbuffer zerava o estado a cada entrada e lia esses 4 parâmetros como cabeçalhos de comando
   ("method … on unbound subchannel"); a macro rodava com parâmetros a menos;
2. com o leitor corrigido, os parâmetros chegavam inteiros, mas eram lixo (`c5c5c6c6 …`): o tradutor ignorava
   `STG`/`RED`/`ATOM`, então ninguém escrevia aquela memória. O draw saía com ~3,3 bilhões de vértices em quads; a
   thread da GPU ficava presa convertendo quads em índices e a memória acabava.

Hipóteses descartadas no caminho: memória de comandos reaproveitada (cópia dos pushbuffers no envio: não muda nada
e ficaria errada com argumentos que a GPU escreve depois; removida) e GPU real presa num shader (não há
"GPU timeout waiting for serial" nem perda de dispositivo; o pós-morte mostra o Vulkan ocioso).

**Correção** (commits `ab64c97` e `9be5df8`):

- estado do pushbuffer por canal (item 4 acima);
- memória global no tradutor e cache de storage buffers no renderer (próxima seção): os argumentos agora vêm
  certos (`0 1 0 0` quando não há partícula, por exemplo);
- rede de segurança: uma chamada de macro com parâmetros vindos da memória é descartada se algum passar de 2^24,
  se as instâncias passarem de 65 536 ou se contagem × instâncias passar de 2^26 (`gpu.cpp flush_macro`); um draw
  com mais de 2^24 vértices é pulado e para a macro (`Macro::stop`). Logs: `3d: indirect macro …`,
  `3d: skipped a draw …`.

**Diagnóstico pós-morte** (fica ligado): quando a cerca de um quadro atrasa 1 s (e de novo aos 10 s), o `vi` chama
`gpu::dump_hang_state()`: onde cada thread da GPU está parada (`gpu/hang_state.h`, marcas `hang::Where`), envio e
entrada em execução, fila, seriais do Vulkan (próximo, enviado, timeline, concluído), callbacks pendentes e se o
lock do renderer está preso. Linhas `hang: …` no log. Com o processo vivo, `lldb -p <pid>` do toolchain do HWDER
(`…\managed-tools\…\llvm-mingw-…\bin\lldb.exe`, `thread backtrace all`) mostrou a pilha da thread "HWDER GPU".

### Memória global e atômicos (implementados)

- **Descritores**: em compute o NVN deixa os storage buffers no `c0` a partir de **0x310**, 16 bytes cada
  (`endereço baixo, endereço alto, tamanho, 0`). Medido nos dumps (`HWDER_DUMP_SHADERS`). Nos estágios gráficos a
  base segue o padrão de 0x100 por estágio (0x110 vértice … 0x510 fragmento), **não conferido**: nenhum shader
  gráfico do FEW usa memória global até aqui.
- **Codificações conferidas no dump**: `LDG`/`STG` dado 0–7, endereço Ra 8–15, deslocamento com sinal 20–43, `.E`
  no bit 45, tamanho 48–50; `ATOM` valor Rb 20–27, deslocamento 28–47, `.E` no bit 48, tipo 49–51, operação 52–55;
  `RED` valor 0–7, endereço 8–15, tipo 20–22, operação 23–25, deslocamento 28–47, `.E` no bit 48; `ATOMS` valor
  20–27, tipo 28–29, deslocamento em palavras 30–51, operação 52–55.
- **Tradutor** (`shader/emit_mem.cpp`): a varredura (`scan_global_memory`) acha os descritores que o programa lê;
  cada um vira um storage buffer (`uint data[]`, binding logo após os constant buffers). Cada acesso compara o
  endereço de 64 bits com o descritor e indexa em palavras; quando o registrador alto do endereço foi escrito no
  mesmo bloco a partir de `c[0][desc+4]` (o padrão do NVN: `MOV`/`IADD.X`), só aquele descritor é testado
  (`gmem_candidates`), senão todos. Operações: ADD, MIN, MAX, AND, OR, XOR, EXCH em 32 bits com e sem sinal. Faltam
  INC/DEC, CAS, 64 bits e float (logam `… unsupported (no-op)`). `S2R` 56–60 (máscaras de lane) implementados:
  sem eles o "append" por warp escrevia todas as lanes no mesmo lugar.
- **Renderer** (`gpu/vk/vk_resources.cpp gmem_bind`): um buffer host-visible com cache por início de faixa do
  guest. Enquanto a cópia da GPU tem escrita pendente (`pending` = serial do command buffer), todo bind a usa, e os
  despachos encadeados (contar, compactar, escrever argumentos) se enxergam; quando o command buffer conclui, a
  thread de espera copia de volta para a memória do guest. Antes de ler uma entrada do GPFIFO, a thread da GPU
  chama `sync_gpu_writes` e espera essa volta se a entrada cair numa faixa pendente (~3 vezes por quadro).
  `flush_region` faz o mesmo para leituras de DMA.

### Desempenho da batalha

Com as partículas vivas, o jogo passa de 2 para **~450–800 despachos de compute por quadro**, com um `WaitForIdle`
entre eles. Primeira versão: ~100 ms por quadro. Correções (thread da GPU de 97 para 34 ms por quadro):

| Custo | Antes | Correção |
|---|---|---|
| hash do código do compute a cada despacho (FNV byte a byte, até ~100 KB) | ~45 ms | chave por endereço e forma, hash uma vez por quadro (`compute_keys_`) |
| `WaitForIdle` do 3D virava um envio ao Vulkan (~1000 por quadro) | ~760 envios | só ordena a GPU, e as barreiras já fazem isso; `HWDER_WFI_FLUSH=1` volta ao antigo |
| recópia guest→GPU de storage buffer a cada bind | 26 ms | só quando o write-watch acusa escrita; uma cópia de volta por command buffer |

Resultado medido (`HWDER_THREAD_STATS=1`): `dispatch 4,5 ms`, `gmem 2,8 ms`, 2,2 envios por quadro, média de
~42 ms por quadro (33 pedidos), picos de ~300 ms em efeito novo.

### Taxa de quadros

- Menus a 60 fps estáveis. Na batalha **o jogo pede 30 Hz** (swap interval 2). Antes das partículas a batalha
  ficava em 33–35 ms; com elas, ~42 ms (pendência 1).
- Para 60 fps na batalha há dois caminhos: (a) uma opção de taxa de quadros do próprio jogo, se existir, que só
  aparece depois do prólogo (a pausa do prólogo não tem menu, conferido com `pause_menu.txt`); (b) os relógios de
  frame do KTGL, como o AoC-Recomp fez (três relógios; mexer só no intervalo acelera o jogo). (b) depende dos
  símbolos do FEW (pendência "Ganchos do jogo").

### Engasgos

- Primeira vez que um efeito aparece: `graphics pipeline creation took 100–330 ms`, que congela o quadro. É uma
  escolha do HWDER (`vk_pipelines.cpp`, `synchronous = … || st_.draws < 64 || draw_samples_rt_`): draws que leem
  render target compilam de forma síncrona para não mostrar um quadrado preto. `HWDER_ASYNC_RT_PIPELINES=1` troca
  por assíncrono (com risco de falha visual de um quadro).
- Os caches em `Jogar no PC` (`pipeline_cache.bin`, `pipeline_warmup.bin`) crescem a cada sessão e o
  pré-aquecimento os compila na abertura seguinte; os engasgos diminuem com o uso. Na primeira vez, os shaders de
  cálculo grandes da batalha levavam segundos para compilar até `gmem_candidates` (um descritor por acesso).
- Outros números da sessão: `skipped_draws` 486 (draws pulados enquanto o pipeline compila em segundo plano),
  `vk: 120 draws into already-presented buffers` (o quadro apresentado antes de terminar).

## Steam Deck

O binário Linux é compilado no PC (WSL2 `archlinux`) e vai pronto para o cartão SD; o Deck não compila nada.

- **Build** (`.\gerar-deck.ps1`, depois de um `gerar-few.ps1`): `tools/deck/build-deck-wsl.sh` copia `runtime`,
  `tools`, `third_party` e o C gerado (`generated\`, o mesmo do Windows: só caminhos relativos) para
  `/root/few/src` no ext4 do WSL e chama `build.py` com o toolchain Linux do HWDER v0.3.5, já instalado em
  `/root/hwder/data/HWDER/toolchain/<id>` (Zig/Clang mirando glibc 2.35, CMake, Ninja), as dependências
  estáticas da release Linux oficial (`/root/hwder/release/HWDER-linux-x86_64-v0.3.5/linux-deps`: SDL3, FFmpeg,
  libcrypto) e `-DHW_MARCH=znver2` (o Zig passa `-target-cpu znver2`). Saída: `/root/few/build-deck/few`.
  O C gerado leva ~1 h com 6 jobs (o WSL tem 10 GB; ~470 MB por compilação); depois, só o runtime recompila.
- **Pasta** `Envio Deck\FireEmblemWarriors` (ignorada pelo git), sem caminho fixo: `few`, NSP base e as 5 DLCs e
  as chaves por hardlink (o jogo pega o `.nsp` de nome menor, que é o da base, e acha as DLCs na mesma pasta),
  `update\` (hardlinks de `data\update`; `update_path=update` no `.ini`), `hwder_settings.ini`, `few-steam.sh`,
  `instalar-no-deck.sh` (atalho na Steam via `tools/steam_shortcut.py`), `LEIA-ME.txt`. ~19,8 GB.
  `-SoDados` monta sem o binário, para ir enviando enquanto o WSL compila.
- **Envio** (Git Bash): `bash tools/deck/enviar-para-deck.sh` (sem argumento: `Games/FireEmblemWarriors` no primeiro
  cartão com pasta `Games`). Por SSH com `~/.ssh/<chave-do-deck>`, usuário `deck`, IP `<ip-do-deck>` (DHCP;
  `DECK_HOST=deck@<ip>`). Só o que mudou, retoma arquivo cortado, confere SHA-256 nos dois lados, mantém o `.ini`
  do Deck (`ENVIAR_INI=1` sobrescreve). O Deck precisa estar acordado; o SteamOS nega `systemd-inhibit` por SSH.
- **No Deck**: `/run/media/deck/<cartao>/Games/FireEmblemWarriors` (SD ext4, SteamOS
  3.9.5, APU AMD Custom 0405). Atalho na Steam: no Modo Desktop, na pasta, `sh instalar-no-deck.sh` (fecha e
  reabre a Steam). O write-watch no Linux usa `userfaultfd` + `PAGEMAP_SCAN` (kernel 6.7+, o SteamOS tem).

## Pendências, por prioridade

1. **Ritmo da batalha**: ~42 ms por quadro em média para 33 pedidos, e picos de ~300 ms. Pistas, em ordem:
   (a) a fase `present` do perfil (~20 ms por quadro; o perfil soma a thread do `vi`, que apresenta, à da GPU):
   ver se é espera do apresentador;
   (b) as esperas por escrita de shader (`vk: waiting for shader writes …`, ~3 por quadro, cada uma esvazia a GPU):
   o caminho de vez é o do yuzu, desenhar o indireto na própria GPU (`vkCmdDrawIndirect` lendo o buffer onde o
   compute escreveu), mas a macro 25 desenha quads, que hoje viram índices na CPU; (c) compilação de pipeline
   síncrona (seção "Engasgos").
2. **O que falta da memória global**: atômicos INC/DEC, CAS, 64 bits e float; base dos descritores nos estágios
   gráficos (não conferida); `SUST` (escrita em imagem, no-op), `AL2P` + `IPA` indexado, `TEX` com AOFFI não
   constante. Todos logam uma vez com `[shader]`.
3. **60 fps na batalha** (seção "Taxa de quadros").
4. **Escala de resolução > 1x**: travada em 1x (`texture_cache.cpp res_scale_allowed`). Em 2x a composição cobre
   só o quadrante superior esquerdo. Diagnóstico provável, não confirmado: passes (compute, ou fetches a partir de
   `gl_FragCoord`) que endereçam alvos escalados em texels do guest. O caminho é o do escalonador do Ryujinx:
   `gl_FragCoord` no espaço do guest e compute escalado. O `TLDS` já escala as coordenadas das texturas marcadas
   (`PushConstants::scaled_textures`). Confirmar primeiro com `HWDER_VK_FRAME_DUMP` em 2x
   (`HWDER_ALLOW_RES_SCALE=1`).
5. **Ganchos do jogo** (`HW_GAME_HOOKS=OFF`): 120 fps, ultrawide e LOD do HWDE dependem de símbolos. Achar as
   funções equivalentes do KTGL no FEW (`tools/recomp/games/hwde/symbols.txt` como guia; `remap_symbols.py`
   compara corpos de função) e só então religar.
6. **Nomes do save**: o jogo grava `system` em `save/0100F15003E64000/<usuário>/`; os do progresso ainda não foram
   vistos. Ajustar `HW_GAME_SAVE_MAIN/SIDE` em `game_profile.h` (só afetam o seletor de save do menu F1).
7. **Steam Deck**: medir a batalha no aparelho (o Zen 2 a 2,4–3,5 GHz é mais lento que o i5-11400H por núcleo; a
   thread da GPU, 34 ms por quadro no PC, é o gargalo provável) e montar a medição como a do HWDER
   (fora deste repositório).

## Armadilhas já encontradas

- O primeiro dump era o japonês v0 (`0100EAE003A5A000`): foi descartado. O alvo é o US com update.
- `-Filter` do `Get-ChildItem` não entende colchetes de nome de NSP: use `Where-Object { $_.Name -like '*`[Base`]*' }`.
- Windows PowerShell 5.1: `$PSScriptRoot` vazio em valores padrão de `param()`; aspas duplas dentro de
  here-string quebram argumentos de `git commit -m` (use `-F arquivo`); `-File` passa listas como texto
  (`run_test.ps1` recebe `-Shots "10,20"`).
- O guarda de segurança da sessão bloqueia `Remove-Item` com curinga; apague por caminho literal.
- O autoplay do HWDE (`HWDER_AUTOPLAY`) é roteiro de menu do Hyrule Warriors; use `HWDER_INPUT_SCRIPT`.
- Um build que falha deixa o `few.exe` anterior no lugar: `run_test.ps1` testa o que estiver publicado. Confira a
  saída de `gerar-few.ps1` (`ERRO: Compilacao falhou`) antes de tirar conclusões de um teste.
- Os caches de pipeline em `build\run` são reaproveitados entre testes; para medir engasgos de primeira vez,
  apague-os.
- Um branch criado antes de um commit de documentação não tem os roteiros novos: `run_test.ps1` falha em
  `Resolve-Path`. Rebaseie o branch sobre o `main` antes de testar.
- `gerar-few.ps1` publica o build da árvore atual, seja qual for o branch: anote qual branch está em
  `Jogar no PC`.
