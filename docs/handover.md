# Handover — estado e próximos passos

Atualizado em 2026-11-15. **Próxima sessão: comece pela seção "Retomar aqui".** Leia antes de qualquer tarefa;
atualize ao terminar uma sessão.

## Retomar aqui (2026-11-15, tarde)

Objetivo do dono: batalha a 30 fps **estáveis no Steam Deck com as partículas ligadas** ("não quero comprometer o
jogo pela estabilidade"). Nada de desligar efeito (`HWDER_GMEM=0` não é solução).

Estado, medido no Deck com o build que está no cartão (commit `3b4e15d`): a luta densa do `battle_long.txt` saiu
de ~11 fps para **26–28 fps na maior parte do trecho** (mediana de 33,3 ms: quadros a 30 fps com alguns a 20) e
**~19–20 fps na janela mais pesada** (cerca de 650 despachos de compute por quadro). Detalhes na seção "Sessão de
2026-11-15".

O que falta para os 30 fps estáveis é **a CPU e a GPU trabalharem ao mesmo tempo**. Hoje, por quadro e em série:
jogo ~5–6 ms → gravação ~9–11 ms → GPU 10–31 ms → cópia de volta ~4 ms. Na janela pesada a GPU sozinha leva
~31 ms, então qualquer coisa em série passa de 33 ms.

Passo a passo:

1. **Deck livre**: ligado, no Modo Jogo, sem jogo aberto (o teste se recusa a rodar com um jogo aberto). Conferir
   o SSH: `ssh -i ~/.ssh/<chave-do-deck> deck@<ip-do-deck> uptime`. O Deck suspende sozinho e some da rede.
2. **Build do Deck**: `.\gerar-deck.ps1` (incremental, ~3 min) e
   `ARQUIVOS="few" bash tools/deck/enviar-para-deck.sh` (o envio confere o SHA-256).
3. **Medir a batalha** (~5 min; save vazio até o prólogo e luta longa):
   `DECK_FRESH=1 DECK_PACE=1 DECK_INPUT=tools/input/battle_long.txt bash tools/deck/testar-no-deck.sh 230`.
   As janelas de 145 a 230 s são a luta densa. As lutas variam entre execuções: rodar duas vezes e olhar a
   pior janela. Referências em `build/deck_pace_*.log`.
4. **Sobreposição de quadros** (`HWDER_FRAME_OVERLAP=1`, experimental, desligada por padrão): o jogo só entrega o
   quadro N+1 depois que os syncpoints do quadro N disparam, e o runtime só os dispara quando a GPU termina o
   quadro N (`on_gpu_idle`). Com a opção, eles disparam quando o quadro N está gravado e enviado e o trabalho até
   o ponto de sincronização anterior terminou: a GPU desenha N enquanto o jogo e a thread de comandos preparam
   N+1. No PC com GPU lenta simulada (`HWDER_GPU_SLOW_MS=30;HWDER_GPU_SLOW_AFTER=120`): 12,6 fps em série contra
   mediana de 33 ms com a opção. **Dois problemas em aberto antes de ligar**:
   - a cópia de volta de `gmem` deixa de rodar: `gmem_bind` pula a cópia de um command buffer quando outro, mais
     novo, já escreveu o buffer (`pending != serial`), e com sobreposição sempre há um mais novo. A memória do
     guest para de receber os dados das partículas (os vértices delas saem dessa memória). Plano: uma cópia de
     volta por command buffer com `readback` próprio (2–3 por buffer), espelho em memória da CPU, fusão em três
     vias por palavra (o que o guest escreveu e ainda não subiu não é sobrescrito) e upload das diferenças mesmo
     com escrita pendente;
   - travadas periódicas de ~300 ms (`display: slow frame`), ainda sem causa: capturar uma com
     `HWDER_TIMELINE=<s>` (eventos novos: `sync wait`, `cv wait`, `addr wait`, `woke`, `ev wait`, `vk fence`,
     `vk deferred`).
5. **Depois**: tempo de GPU na janela pesada (`DECK_ENV="HWDER_GPU_TIMING=5"`): passe de iluminação `e789f3db`
   (~7,7 ms), "other" (~6 ms, inclui a cópia de volta de ~30 MB na GPU), `a3e216c7` (~3,4 ms).
6. **Fechar a sessão**: commitar e atualizar este arquivo (estado, medidas, pendências).

## Estado

O jogo **roda nativo no Windows** a partir de base US v0 + update 1.5.0 + 5 DLCs:

- boot, logos, vídeo de abertura (H.264) sem artefatos;
- criação de save, notas do update 1.5.0, menus de novo jogo;
- cutscene do prólogo e **a batalha do prólogo** com cenário, multidão, efeitos (inclusive as partículas da GPU:
  poeira, riscos de velocidade), diálogos, HUD, minimapa e KO;
- as 5 DLCs são listadas (`aoc:u`) e montadas pelo jogo (`OpenDataStorageByDataId`) no boot.

**Batalha a 60 fps no PC** (desde 2026-11-14): o jogo roda no seu próprio modo Desempenho (gancho
`Display_SetMode`), a 1280×720 por padrão (`fps60_1080p=1` renderiza a 1920×1080). Menus e cutscenes em tempo real
seguem como o jogo quer (60 e 30). Medido com `HWDER_PACE_TRACE=1` e `tools/pace_summary.py` no `battle_long.txt`
(RTX 3050 Laptop, sem `HWDER_GPU_TIMING`), antes ~24 fps a 30 Hz pedidos:

| Trecho | 720p (padrão) | 1080p |
|---|---|---|
| luta leve (85–165 s) | 59 fps, mediana 16,7 ms | 57–59 fps, mediana 16,8 ms |
| luta densa (165–230 s) | 50–54 fps, mediana 17,0–17,5 ms, 19–28% > 20 ms | 41–48 fps, mediana 18,6–22 ms, 37–65% > 20 ms |

A 1080p a luta densa é limitada pela GPU; a 720p o que sobra são quadros perdidos avulsos (pendência 2). Daí o
padrão 720p, que é também o do próprio modo Desempenho do jogo.

O travamento da batalha da terceira rodada segue resolvido.

**Steam Deck**: o cartão SD tem o build de 2026-11-15 à tarde (`3b4e15d`: partículas ligadas, 30 fps a 720p,
despachos encadeados, cópias de `gmem` só do que mudou, `memcmp` vetorizado). Luta densa: 26–28 fps na maior parte
e ~19–20 fps na janela mais pesada; antes, ~11 fps (seção "Retomar aqui"). **No Deck o padrão é 30 fps**
(`fps60=0`, modo Qualidade): decisão do dono, porque no FEW 30 fps é o ritmo nativo da batalha no Switch (a lógica
segue `g_FrameRate`), ao contrário do HWDE; 60 fps fica como opção no menu.

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
| Ritmo de quadros (fps real) | `-Env "HWDER_PACE_TRACE=1"`: uma linha `pace: present dt …` por quadro apresentado; resumir por janela |
| Onde a thread de comandos gasta | `-Env "HWDER_SAMPLE_US=1000"` e `python tools\symbolize_samples.py build\run\hwder.log.samples.txt "Jogar no PC\few.exe"` |
| Laço de quadros (jogo × thread da GPU) | `-Env "HWDER_TIMELINE=<s>"`: 0,5 s de eventos em `build\run\hwder.log.timeline.txt` (µs) |
| Validar todos os shaders (WSL) | rodar com `HWDER_DUMP_SHADERS=1`, depois `wsl -d archlinux -u root -- bash "/mnt/c/…/tools/shadertest/wsl_validate.sh"` |
| Engenharia reversa do `main` | `wsl -d archlinux -u root -- python3 "/mnt/c/…/tools/gxref.py" callers 0x…` (ver o cabeçalho) |
| Modo 30 fps (Qualidade) / 720p | `-Ini "fps60=0"` / `-Ini "fps60_1080p=0"`; 30 fps a 720p: `-Ini "fps30_720p=1"` |
| GPU lenta simulada (ritmo do Deck no PC) | `-Env "HWDER_GPU_SLOW_MS=30;HWDER_GPU_SLOW_AFTER=120"`: segura cada command buffer terminado 30 ms, a partir de 120 s |
| Um quadro de despachos de compute | `-Env "HWDER_DISPATCH_TRACE=<s>"`: linhas `dtrace:` (programa, grade, faixas lidas/escritas, uploads) |

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

## Sessão de 2026-11-14: 60 fps na batalha com as partículas (interrompida)

Pedido do dono: batalha a 60 fps como o HWDER faz no HWDE, **sem desligar partículas**; testar no PC/WSL e levar ao
Deck depois. Commitado em partes (de `231cdd4` em diante); o `few.exe` de `Jogar no PC` e o `few` de `Envio Deck`
têm tudo.

### 1. Controle de fluxo estruturado no tradutor de shaders (falta validar no Deck)

- `runtime/src/shader/structurize.cpp` (novo): todo programa com mais de um bloco vira seleções e laços SPIR-V
  aninhados (Ramsey, "Beyond Relooper", ICFP 2022, sobre a árvore de dominadores), em vez do laço+`switch`. Ajustes
  para GPU: nó fora de um laço cujo dominador está dentro é posto **depois** do laço (reconvergência, barreiras); o
  Block mais interno em volta de um Loop divide o merge com ele (saída de laço = `break` simples); saltos para fora
  de mais de um construto guardam um código numa variável e são repassados de merge em merge. Grafo irredutível:
  `StructureFailed` e o tradutor refaz com o dispatcher (log `structured control flow failed`).
  `HWDER_SHADER_DISPATCHER=1` força o dispatcher antigo (comparação). O `kVersion` do `pipeline_warmup.bin` foi
  para 3 (os SPIR-V antigos não são reaproveitados: a primeira batalha volta a compilar).
- Por que o dispatcher era lento no AMD: o NIR transforma o `OpSwitch` numa cadeia linear de `if` (~400–600 casos
  nos shaders das partículas), percorrida a cada salto entre blocos. A pressão de registradores **não** era o
  problema: o estimador (`tools/shadertest/spv_pressure.py`) dá o mesmo pico de valores vivos (31–136) nas duas
  formas e média menor na estruturada.
- Validação: os 170 programas despejados da batalha do prólogo (98 fs, 57 vs, 15 cs) traduzem estruturados, sem
  nenhum fallback; `spirv-val --target-env vulkan1.1` passa em todos menos 1 (`d17e7155f43e7e87`, `TEX` com AOFFI
  não constante, problema antigo, pendência 4). No PC (NVIDIA) a batalha renderiza igual (capturas conferidas).
  **Ainda não rodou no RADV**: as estatísticas do compilador do Deck (VGPR, spill, tempo) dependem do drm-shim do
  Mesa (abaixo).
- Ferramentas: o dump (`HWDER_DUMP_SHADERS=1`) agora grava também `shader_<chave>.<vs|fs|cs…>.bin` (SPH + código);
  `tools/shadertest/wsl_validate.sh` traduz e valida todos no WSL; o `shadertest` compila no Linux e aceita grupo e
  memória compartilhada (`one a.bin out.spv lx ly lz smem`); `listing_to_bin.py` refaz o binário a partir de uma
  listagem antiga; `wsl_bench.sh` + `radv_stats.cpp` compilam com o RADV para o VanGogh e imprimem as estatísticas.
- **RADV sem Deck**: o Mesa 26 tirou o dispositivo nulo do `RADV_FORCE_FAMILY`; agora é o drm-shim `amdgpu_noop`
  (`LD_PRELOAD` + `AMDGPU_GPU_ID=vangogh`), que o Arch não empacota. No WSL `archlinux` já estão instalados
  `vulkan-radeon` 26.2.6, `meson`, `python-mako` etc. e o fonte em `/root/mesa/mesa-26.2.6`; **configurar e compilar
  o shim foi bloqueado pelo classificador de permissões da sessão**: o dono precisa autorizar (ou rodar ele mesmo
  os comandos do cabeçalho de `wsl_bench.sh`).

### 2. Modo Desempenho do próprio jogo: 60 Hz na batalha

- Achado por engenharia reversa (`tools/gxref.py`, novo); fatos em [few.md](few.md#modos-de-exibição-e-taxa-de-quadros).
  O jogo já sabe rodar a batalha a 60 Hz com a lógica no passo certo (grava `60/intervalo` todo quadro).
- Ganchos (os primeiros do FEW, `runtime/src/game/few_display_mode.cpp`, sempre compilados, fora do
  `HW_GAME_HOOKS`): `Display_SetMode` troca o modo 0 por 1 (e 3 por 4) quando o byte `sys+0xa6b2` está desligado e,
  com `fps60_1080p`, grava o id de configuração 0 (1920×1080) no lugar do 3 (1280×720); `DynRes_Update` devolve
  sempre o topo do grupo (sem as quedas falsas para 1120×630). O `symbols.txt` ganhou os nomes da cadeia toda.
- Ajustes: `fps60` (padrão 1) e `fps60_1080p` (padrão 0, medido acima) no `hwder_settings.ini`, caixas "60 fps battles" e
  "1080p at 60 fps" no menu (valem no próximo início). Desligar `fps60` volta ao modo Qualidade (30 fps).
- Efeito colateral aceito: no modo 1 a distância de desenho de personagens é 1500 em vez de 2000
  (`Display_ModelDrawDistance`; um gancho devolveria 2000 se fizer falta).
- No log: `few: display mode 0 -> 1 (60 fps battles, ...)`, `display: swap interval 1` na batalha e `2` só na
  cutscene em tempo real.

### 3. Thread de comandos da GPU (o gargalo no PC a 60 Hz)

Medidas (`battle_long.txt`, 60 Hz pedidos): o timeline mostrou a thread de comandos nunca ociosa e os quadros
pesados (949 entradas) levando 20–22 ms nela; o amostrador novo mostrou **56% do tempo de parede parada em
`gmem_sync`**.

- **Argumentos dos draws indiretos sem espera** (`vk_renderer.h sync_gpu_writes`): antes de ler uma entrada do
  GPFIFO escrita por shader de cálculo, a thread esperava a GPU terminar e copiar de volta (~2 vezes por quadro,
  cada uma esvaziando a GPU). Agora lê o que a última cópia de volta deixou (1–2 quadros de atraso, coerente com os
  vértices das partículas, que vêm das mesmas cópias). Luta leve de 40 para 58 fps; partículas conferidas nas
  capturas. `HWDER_INDIRECT_SYNC=1` volta a esperar. As leituras de DMA (`flush_region`) continuam esperando. O
  caminho certo de vez continua sendo o draw indireto na GPU (pendência 3).
- **Dados embutidos em bloco** (`gpu.cpp put_method_run`): `LoadInlineData` do I2M (classes 3D, compute e I2M) e
  `CB_DATA` do 3D eram tratados palavra por palavra, cada uma com um `OpTimer` (2× `QueryPerformanceCounter`),
  `vector::insert` de 4 bytes e, no `CB_DATA`, um `invalidate_region` com mutex (`i2m_data` era ~30% do tempo
  ocupado). Agora uma cópia e uma invalidação por sequência (`i2m_data_bulk`, `cbuf_data_bulk`); o caminho por
  palavra fica para o resto. Com isso a luta densa foi de ~30 para 53–59 fps (720p).
- Medido com `HWDER_GPU_TIMING` (que distorce os fps): GPU real ocupada ~9,5 ms por quadro a 720p e ~18,8 ms a
  1080p na luta densa ("outside passes" de 8 para 16 ms). Sem o timing, 1080p não segura 60 na luta densa (tabela
  em "Estado").

### 4. Ferramentas e correções avulsas

- **Amostrador no Windows** (`host/sampler.cpp`): `HWDER_SAMPLE_US=1000` agora funciona no Windows (relógio de
  parede, suspende a thread e desenrola 12 quadros; cada relatório cobre os últimos 10 s). O `few.exe` tem DWARF,
  que o DbgHelp não lê: `python tools/symbolize_samples.py build/run/hwder.log.samples.txt "Jogar no PC/few.exe"`
  resolve com o `llvm-symbolizer` do toolchain (sem inlines) e grava `.sym.txt`. Funções de DLL do sistema saem
  pelo export mais próximo (um "memcmp" ali era o `memcpy` do upload).
- **eShop** (`service/applet.cpp`): um item de menu abre o applet 0x14 (`ShowShopPage`); o stub não devolvia dados
  e o jogo abortava com `svcBreak` (aconteceu num teste, aos 39 s do `battle_long`). Agora fecha na hora como os
  applets web (`ShopPageReturnValue` zerado). Dois `battle_long` completos depois disso sem queda.
- `tools/gxref.py`: referências a strings e endereços, chamadores, escritas em campo, `MOV #imm`, tabelas. Roda no
  Python do WSL (`pacman -S python-capstone python-lz4`, já instalados).

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
- **Buffers device-local** (desde o build do Deck): a memória do guest entra por cópia no anel de staging
  (`gmem_upload`, só quando o write-watch acusa escrita) e volta por uma cópia na GPU para um buffer host-cached,
  gravada quando o command buffer é enviado (`gmem_record_copybacks`, gancho `g_before_submit_gmem`); a thread de
  espera copia de lá para o guest. No PC os piores quadros da batalha caíram de ~300 para ~80 ms.
- **Barreiras em torno de despachos** (`vk_sched.cpp dispatch_barrier_before/after`): finas (transferência →
  compute antes; escrita de compute → leitores depois) em vez de duas barreiras completas por despacho.
  `HWDER_DISPATCH_FULL_BARRIERS=1` volta às completas.

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
  ficava em 33–35 ms; com elas, ~42 ms (resolvido na sessão de 2026-11-14).
- Para 60 fps na batalha havia dois caminhos: (a) uma opção de taxa de quadros do próprio jogo; (b) os relógios
  de frame do KTGL, como o AoC-Recomp fez. **Resolvido em 2026-11-14 pelo caminho (a)**: o jogo tem o modo
  Desempenho e o gancho `Display_SetMode` o liga (seção "Sessão de 2026-11-14").

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
  O C gerado leva ~1,5–2 h com 10 jobs (o Clang do Zig leva ~2x o tempo do Windows nos arquivos grandes; os 184
  `rc_main_*` são 80% do trabalho; ~0,7 GB por compilação, o WSL tem 10 GB). Depois, só o runtime recompila. O
  `-march=znver2` não pesa na compilação (8,8 s contra 9,3 s do genérico num arquivo de 1 MB). O script mantém o
  Windows acordado durante o build: em espera, o WSL para junto. Primeiro build (2026-11-13): `few` de 675 MB,
  exige só `libc`/`libm`/`ld-linux`, glibc máxima 2.35.
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
  3.9.5, kernel 7.2, APU AMD Custom 0405, RADV). Atalho na Steam: no Modo Desktop, na pasta,
  `sh instalar-no-deck.sh` (fecha e reabre a Steam). O write-watch no Linux usa `userfaultfd` + `PAGEMAP_SCAN`
  (kernel 6.7+).
- **Teste de abertura** pelo PC: `bash tools/deck/testar-no-deck.sh [segundos]` roda o `few` no Deck com a janela
  oculta e sem som (`HWDER_OFFSCREEN`, `HWDER_MUTE`, `DISPLAY=:1`, o Xwayland dos jogos no Modo Jogo, que tem
  `STEAM_MULTIPLE_XWAYLANDS=1`) e resume o `hwder.log`. Não roda se houver jogo aberto.
- O Deck **suspende sozinho** em poucos minutos sem uso, e aí some da rede (nem ARP): peça ao dono para acordá-lo
  antes de enviar ou testar.
- **Medido no Deck (2026-11-13)**: abre em 4,5 s, menus e abertura a 60 fps cravados, update e DLCs carregados,
  write-watch ativo, sem erro. **Batalha com as partículas da GPU: ~3,5 fps** (270 ms por quadro). O tempo de GPU
  (`HWDER_GPU_TIMING=5`, cada despacho é um segmento `disp`) vai quase todo para três shaders de cálculo de ~12 mil
  instruções e ~300–480 desvios (`shader_a49eaa020c32da6e`, `abbede64c0dec758`, `13be8c9400da3059`): ~0,7 ms por
  despacho de **um grupo só**, ~700 despachos por quadro. Não é barreira (trocá-las por barreiras finas não mudou o
  Deck) nem tipo de memória (buffers device-local também não). Causa provável: o laço+`switch` do tradutor com todos
  os registradores do guest vivos estoura os registradores no compilador da AMD (ACO) e despeja em scratch; a NVIDIA
  aguenta. Saída rápida: `HWDER_GMEM=0` (memória global como no-op: sem partículas da GPU, a validação dos draws
  indiretos descarta os argumentos de lixo). Correção tentada: o tradutor estruturado (sessão de 2026-11-14), falta
  medir no Deck (pendência 1).
- **Com `HWDER_GMEM=0` a batalha roda a 30 fps no Deck** (~33,5 ms por quadro, o que o jogo pede; thread da GPU
  ~3,5 ms por quadro), com uma pausa única de ~2,5 s quando a primeira batalha começa (compilação dos shaders de
  cálculo pelo ACO). Por isso o `few-steam.sh` do pacote exporta `HWDER_GMEM=0` (a menos que já venha definido) e o
  `steam_shortcut.py` preserva esse lançador ao criar o atalho.
- O atalho na Steam **foi criado** em 2026-11-15 pelo SSH, com o Deck no Modo Jogo e nenhum jogo aberto:
  `sh instalar-no-deck.sh --yes` na pasta do jogo (fecha a Steam; a sessão do Modo Jogo a reabre sozinha; capa
  baixada). Na biblioteca há outra entrada "Fire Emblem Warriors", a da versão de 3DS pelo Azahar (EmuDeck): a do
  recomp aponta para `few-steam.sh`. Os testes deixaram `pipeline_cache.bin`/`pipeline_warmup.bin` aquecidos na pasta.
- Envio só de alguns arquivos: `ARQUIVOS="few few-steam.sh" bash tools/deck/enviar-para-deck.sh` (sem conferir os
  ~19 GB de NSPs).

### Medido no Deck (2026-11-15, build `0b31f52e…`, partículas ligadas)

`battle_long.txt` pelo SSH (`DECK_PACE=1`), resumo de `tools/pace_summary.py`:

| Trecho | 30 fps (`fps60=0`, 1080p, padrão) | 60 fps (`fps60=1`, 720p) |
|---|---|---|
| menus, abertura | 56–59 fps | igual |
| luta leve (104–124 s) | 28,8 fps, mediana 33,3 ms (segura os 30) | — |
| luta densa (164–230 s) | 10–11 fps, mediana 88–99 ms | ~12–14 fps (73–81 ms por quadro, média do `display: 300 frames`) |

Antes (laço+`switch`): ~3,5 fps (270 ms). O tradutor estruturado deu ~3x, mas a luta densa ainda fica longe dos
30. A resolução pesa pouco (720p a 60 Hz só ~15 ms melhor por quadro): o custo é das partículas. O log enche de
`vk: slow: wait_serial took 50 ms` (a CPU esperando a GPU). Próximo passo: `HWDER_GPU_TIMING=5` no Deck para ver
quanto cada despacho dos três shaders grandes custa agora (era ~0,7 ms com um grupo só), e as estatísticas do RADV
(drm-shim) para saber se ainda há despejo em scratch. Enquanto isso, `HWDER_GMEM=0` volta aos 30 fps sem partículas.

### Por que a batalha pesa no Deck (2026-11-15)

Medido no Deck com `HWDER_GPU_TIMING=5`, `HWDER_GPU_TIMING_DRAWS=1` (novo: tempo por draw, por par de shaders) e
`HWDER_TIMELINE`, comparando com o PC:

- **Não são os shaders de cálculo das partículas**: com o tradutor estruturado, ~700 despachos custam ~15 ms por
  quadro (0,02 ms cada; grupo único de 64). O RADV (drm-shim) compila os três grandes sem nenhum despejo (VGPRs
  32–80, scratch 0).
- **Um draw de tela cheia** (`vs 51e397089218a4aa`, `fs e789f3db3ec877a2`, iluminação por tiles: lê do tile a
  contagem de luzes e percorre a lista com `TLDS`) levava **13,5 ms no Deck e 0,98 ms no PC** (14×; o hardware
  explica ~4×). O shader em si compila bem no ACO (1106 instruções, 80 VGPRs, sem despejo): o laço dava mais voltas.
- **Causa**: o tradutor supunha warps de 32 lanes e a AMD roda waves de 64. `SR_LANEID`, as máscaras `SR_*MASK`,
  `VOTE` (só a componente 0 da votação) e `SHFL` erravam na segunda metade da wave, e os atômicos agregados por warp
  do NVN (contagem por `popc(vote)`, vaga por `popc(vote & LTMASK)`) montavam listas erradas. Corrigido em
  `72ed05f`: as operações de warp agem dentro da metade de 32 lanes (`warp_lane`, `warp_ballot` em `program.cpp`).
  Na NVIDIA nada muda (medido: luta densa 52–55 fps, igual). **Falta medir no Deck** (o SSH parou de responder no
  meio da sessão).
- DCC: as imagens de cor eram `MUTABLE_FORMAT` sem lista de formatos, o que no RADV (GFX10.3) desliga a compressão.
  Agora levam o par sRGB/UNORM (`1389a87`); no Deck a passada pesada mudou pouco (18,8 → 18,2 ms), mas é o uso
  certo da API. Única reinterpretação vista na batalha inteira: RGBA8 UNORM lido como sRGB.
- **CPU e GPU em série**: o jogo entrega o quadro inteiro de uma vez (3 entradas do GPFIFO), a thread de comandos
  grava ~13,5 ms com a GPU parada e só então envia (1 envio por quadro); o jogo espera os syncpoints do quadro
  anterior para entregar o próximo. Enviar em pedaços durante a gravação deve recuperar boa parte desses 13,5 ms
  (cuidado: cada envio leva as cópias de volta da memória global).
- As barras brancas em pilares e arcos (capturas `dcc_*`, `nofl_*`) existem com e sem a lista de formatos: problema
  antigo, ainda não investigado.

## Sessão de 2026-11-15: batalha densa no Deck

Medido no Deck (luta densa do `battle_long.txt`): **~10,6 fps** com a correção de warp (`72ed05f`). O passe
`e789f3db` mudou pouco (16,0 → 14,9 ms): as listas de luz longas não eram o principal. A linha do tempo
(`HWDER_TIMELINE`) mostra, por quadro e em série: a thread de comandos grava ~14 ms com a GPU parada, a GPU trabalha
~60 ms e o jogo leva ~5 ms para entregar o quadro seguinte. **O gargalo é a GPU**: passes ~27–40 ms, despachos e
"other" ~25 ms.

Três correções, todas sem tirar efeito:

1. **"720p at 30 fps"** (`fps30_720p`, `settings.h low_res_30`). Padrão desligado no PC e **ligado no Deck**: o
   `.ini` do `gerar-deck.ps1`, o `few-steam.sh` e o `testar-no-deck.sh` acrescentam as chaves que faltam. O gancho
   `Display_SetMode` grava no modo Qualidade o id de configuração do modo Desempenho (0 → 3, 3 → 8): 30 fps a
   1280×720, o resto igual ao Qualidade. A tela do Deck tem 800 linhas. Medido no Deck: passes 40 → 27,5 ms, luta
   densa 10,6 → ~11,5 fps (sozinho não basta).
2. **Uploads de `gmem` só do que mudou** (`vk_resources.cpp gmem_upload_changed`). A cópia de volta de um buffer
   escrito pela GPU também é uma escrita na memória do guest: o write-watch acusava o buffer inteiro e o bind
   seguinte o reenviava inteiro. ~150 buffers de partículas (até ~1 MB cada, **~31 MB por quadro**) faziam guest →
   GPU → guest todo quadro. Agora o `readback` serve de espelho: é igual à cópia da GPU enquanto nada está
   pendente (`mirror_bytes`). Ele é comparado com a memória do guest em blocos de 4 KiB e só o que difere sobe.
   Medido no PC com `HWDER_DISPATCH_TRACE`: 139 uploads no quadro, **0 bytes diferentes**.
3. **Despachos encadeados sem barreira** (`vk_draw.cpp do_dispatch`; `Recorder::chain` em `vk_common.h` e
   `vk_sched.cpp`). Cada despacho esperava o anterior esvaziar a GPU. No rastreio de um quadro, de 432 despachos só
   ~45 dependem de um anterior por faixa de storage buffer. Agora a barreira entra em quatro casos:
   - o despacho lê ou escreve uma faixa que outro da cadeia escreveu;
   - o despacho escreve uma faixa que outro da cadeia lê;
   - o programa usa imagens;
   - antes de qualquer outro comando. `Recorder::record` fecha a cadeia; `record_raw` (despachos e timestamps)
     não fecha.

   `HWDER_DISPATCH_SERIAL=1` volta à barreira após cada despacho (A/B).

No PC (RTX 3050, 1080p, modo 30 fps) a imagem da batalha sai correta com as três. O fps no PC não serviu de A/B: as
duas execuções tiveram lutas diferentes (53 contra 119 KO no mesmo instante).

### Medições no Deck (tarde)

Luta densa do `battle_long.txt` (janelas de 20 s entre 145 e 230 s), 30 fps a 720p:

| Build | fps | Mediana por quadro |
|---|---|---|
| Antes (720p) | ~11,5 | ~87 ms |
| Despachos em série (`HWDER_DISPATCH_SERIAL=1`) | 11–12 | ~88 ms |
| Despachos encadeados | 14–18 | 59–73 ms |
| + `memcmp` vetorizado e cópia de volta só do que mudou (`3b4e15d`) | 26–28; pior janela 19–20 | 33,3 ms; pior janela ~56 ms |

- **O encadeamento rende**: o tempo de GPU do quadro caiu de ~60 para ~31 ms (`HWDER_GPU_TIMING`: passes 23,4 ms,
  fora de passes 7,9 ms). Os uploads só do que mudou, sozinhos, não apareceram no fps do Deck.
- **`memcmp` de um byte por vez** (`runtime/src/host/memcmp_x86.cpp`): o toolchain Zig liga no executável o
  compiler-rt dele, que traz `memcmp` e `bcmp` fracos, e uma definição no executável vence a da glibc. As duas
  comparam um byte por iteração. O amostrador (`HWDER_SAMPLE_US=500`) mostrou **~35% da thread de comandos** nesse
  laço: geometria, espelho de `gmem`, chaves de cache. As definições novas (AVX2, 32 bytes por passo, ocultas, só
  no Linux) foram testadas contra uma referência byte a byte em 3 milhões de casos. No relatório do amostrador os
  endereços são deslocamentos no arquivo: somar 0x1000000 para achar o símbolo com `nm`/`addr2line` no `few` do
  WSL (`/root/few/build-deck/few`). O `memcpy` do compiler-rt é SSE de 16 bytes e ficou como está.
- **Cópia de volta só das páginas que mudaram** (`gmem_bind`, no `defer`): dos ~30–33 MB que os shaders de
  partículas reescrevem por quadro, **~120–130 KiB mudam**. Escrever tudo sujava o write watch do buffer inteiro e
  fazia o bind seguinte comparar o buffer todo. A thread de espera leva ~4 ms nessa comparação (eram ~10,6 ms com
  o `memcmp` lento).
- Linha do tempo (`HWDER_TIMELINE`) com o build `3b4e15d`, quadro de 33 ms: jogo ~6 ms, gravação ~9–11 ms (eram
  ~18), GPU ~10 ms nesse instante, cópia de volta ~4 ms. `gpu thread busy` caiu de ~21 para ~9 ms por quadro.
- **Por que em série**: o jogo espera os syncpoints do quadro (7 incrementos por quadro) antes de entregar o
  seguinte, e o runtime só os incrementa com a GPU ociosa (`gpu.cpp`, `renderer()->on_gpu_idle`). É o desenho do
  próprio jogo: no Switch também é CPU e depois GPU.

Ferramentas novas: `HWDER_GPU_SLOW_MS=<ms>` (+ `HWDER_GPU_SLOW_AFTER=<s>`) segura cada command buffer terminado,
como se a GPU fosse mais lenta: reproduz no PC o ritmo do Deck; `HWDER_FRAME_OVERLAP=1` (experimental, seção
"Retomar aqui"); eventos de espera do guest e da thread de espera na linha do tempo.

Ferramenta nova: `HWDER_DISPATCH_TRACE=<s>` loga um quadro inteiro de despachos e de uploads de `gmem` (linhas
`dtrace:`), a partir de <s> segundos depois do primeiro despacho. Cada despacho mostra programa, grade, faixas de
storage buffer lidas e escritas e imagens. A análise de dependências foi um script Python simples sobre essas
linhas, procurando sobreposição de faixas `w`/`r`.

## Pendências, por prioridade

1. **Batalha densa a 30 fps no Deck com as partículas** (o objetivo do dono): falta a janela mais pesada
   (~19–20 fps). Nesta ordem:
   - sobreposição de quadros (`HWDER_FRAME_OVERLAP`): refazer a cópia de volta de `gmem` e achar as travadas de
     ~300 ms (seção "Retomar aqui", passo 4);
   - tempo de GPU: passe de iluminação `e789f3db` (~7,7 ms a 720p no Deck, contra ~1 ms a 1080p no PC), "other"
     (~6 ms), `a3e216c7` (~3,4 ms).

   Para os shaders: estatísticas do RADV pelo drm-shim (`tools/shadertest/radv_stats.cpp`, que agora aceita o par
   `vs.spv+fs.spv`).
2. **Custo por despacho na CPU** (gravação ~9–11 ms por quadro no Deck): `do_dispatch` monta `GuestEnv` com
   `std::function` e lê o QMD a cada despacho; `bind_resources`, `AddressSpace::translate` e `hash_program` são os
   seguintes no amostrador. Medir com `DECK_ENV="HWDER_SAMPLE_US=500"` (relatório em
   `<pasta do Deck>/hwder.log.samples.txt`).
3. **Draw indireto na GPU** (substitui a leitura atrasada dos argumentos): macro 25 com parâmetros vindos de memória
   pendente vira `vkCmdDrawIndexedIndirect` lendo o buffer de `gmem` (quads viram índices de um buffer fixo; um
   despacho pequeno converte `{contagem, instâncias, primeiro, instância base}` no comando indexado). Exige também
   os vértices das partículas vindos do buffer da GPU, não da cópia no guest.
4. **O que falta da memória global**: atômicos INC/DEC, CAS, 64 bits e float; base dos descritores nos estágios
   gráficos (não conferida); `SUST` (escrita em imagem, no-op), `AL2P` + `IPA` indexado, `TEX` com AOFFI não
   constante (o único programa que falha no `spirv-val`). Todos logam uma vez com `[shader]`.
5. **Escala de resolução > 1x**: travada em 1x (`texture_cache.cpp res_scale_allowed`). Em 2x a composição cobre
   só o quadrante superior esquerdo. Diagnóstico provável, não confirmado: passes (compute, ou fetches a partir de
   `gl_FragCoord`) que endereçam alvos escalados em texels do guest. Confirmar com `HWDER_VK_FRAME_DUMP` em 2x
   (`HWDER_ALLOW_RES_SCALE=1`).
6. **Engasgos de compilação** (seção "Engasgos") e o 120 fps do HWDE (`HW_GAME_HOOKS`: achar no FEW as funções
   equivalentes do quadro do KTGL; os ganchos novos mostram o caminho com `gxref.py`).
7. **Nomes do save**: o jogo grava `system` e `scenario0` em `save/0100F15003E64000/<usuário>/`. Ajustar
   `HW_GAME_SAVE_MAIN/SIDE` em `game_profile.h` (só afetam o seletor de save do menu F1).
8. **Steam Deck: medição** como a do HWDER (fora deste repositório).

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
- O `battle_long.txt` é cego ao estado do jogo: quando o jogo fica mais rápido, os toques de menu e a cutscene mudam
  de lugar no tempo (a cutscene em tempo real já caiu em 40–60 s e em 120–140 s em execuções diferentes) e um toque
  chegou a abrir a eShop. Compare execuções pelo conteúdo (capturas), não só pelo relógio. A luta densa de verdade
  é o trecho 165–230 s.
- `HWDER_GPU_TIMING` e `HWDER_THREAD_STATS` (profiler por fase) custam caro: não meça fps com eles ligados.
- `wsl.exe -- bash -c '...'` passa pela shell padrão e perde aspas e variáveis: escreva um script e rode
  `wsl -d archlinux -u root -- bash /mnt/c/.../script.sh` (com `MSYS_NO_PATHCONV=1` no Git Bash).
- O ExeFS desempacotado fica em `data/update/exefs` (o `tools/gaddr.py` antigo ainda aponta para `data/exefs`).
