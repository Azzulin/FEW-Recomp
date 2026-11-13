# Handover — estado e próximos passos

Atualizado em 2026-11-13, fim da segunda rodada sobre o travamento (parada a pedido do dono). Leia antes de
qualquer tarefa; atualize ao terminar uma sessão.

## Estado

O jogo **roda nativo no Windows** a partir de base US v0 + update 1.5.0 + 5 DLCs:

- boot, logos, vídeo de abertura (H.264) sem artefatos;
- criação de save, notas do update 1.5.0, menus de novo jogo;
- cutscene do prólogo e **a batalha do prólogo** com cenário, multidão, efeitos, diálogos, HUD, minimapa e KO;
- as 5 DLCs são listadas (`aoc:u`) e montadas pelo jogo (`OpenDataStorageByDataId`) no boot.

**Bloqueio atual: o jogo trava na batalha do prólogo** depois de ~30–50 s de luta (seção "Travamento na batalha").
Aconteceu na primeira sessão do dono e em 3 de 3 testes com `battle_long.txt` sem trace. **Ainda sem correção**:
a primeira candidata (copiar os pushbuffers no envio) foi testada e não resolve.

| Ramo | Conteúdo |
|---|---|
| `main` | estado bom conhecido (até a DLC e a escala travada em 1x) + documentação |
| `wip/gpfifo-snapshot` | `main` + cópia dos pushbuffers no envio (não resolve, inofensiva) + log detalhado do método em subcanal sem classe |

**`Jogar no PC\few.exe` hoje é o build do `wip/gpfifo-snapshot`** (o último publicado). Ele se comporta como o
`main`, inclusive no travamento. Para voltar ao `main`: `git switch main` e `.\gerar-few.ps1 -SoBuild`.

## Como trabalhar

| Tarefa | Comando |
|---|---|
| Gerar tudo e publicar | `.\gerar-few.ps1` (`-SoBuild` pula update e geração do C) |
| Teste sem supervisão | `powershell -File tools\run_test.ps1 -Seconds 120 -Script tools\input\prologue_battle.txt -Shots "60,100" -Tag x -Fresh` |
| Reproduzir o travamento | `powershell -File tools\run_test.ps1 -Seconds 330 -Script tools\input\battle_long.txt -Shots "200" -Tag bl -Fresh` (apague antes `build\run\pipeline_cache.bin` e `pipeline_warmup.bin` para cache frio) |
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

## Sessão de otimização (2026-11-13): o que os logs mostram

Fonte: a sessão do dono (`Jogar no PC\hwder.log`, 165 s, cache frio) e dois testes com `battle_long.txt`.

### Travamento na batalha

Assinatura, igual nas três ocorrências (dono aos 156 s; testes aos 148 s e 146 s):

```
[145.594] vk: slow: graphics pipeline creation took 329 ms
[145.594] gpu: slow: pushbuffer entry (27233) took 332 ms
[148.797] gpu: method 0xe2e on unbound subchannel 0
[149.829] vi: frame fence 1:37861 not reached after 1 s (value 37855) - still waiting
... (repete a cada 10 s, para sempre)
```

- Sempre logo depois de a thread da GPU ficar 100–350 ms parada (compilação síncrona de pipeline).
- Em seguida, um método que não faz sentido: subcanal 0 "sem classe" nos testes (o 0 é o 3D, sempre ligado), e
  `method 0x9ac on unbound subchannel 5` na sessão do dono (o NVN não usa o subcanal 5).
- A cerca do syncpoint 1 fica **exatamente 6 incrementos** atrás do que o `vi` espera; nada mais é apresentado.
- Não aparece espera de semáforo nem de syncpoint dentro do pushbuffer (`semaphore acquire … >100 ms`,
  `pushbuffer syncpoint wait`), nem erro de dispositivo Vulkan.

#### Hipótese 1, testada e descartada: memória de comandos reaproveitada

`gpu.cpp` guarda na fila só os endereços das entradas do GPFIFO e lê os comandos da memória do guest ao executar.
A ideia era que, com a thread da GPU parada, o jogo reaproveitasse a memória de comandos. Correção no branch
`wip/gpfifo-snapshot`: `channel_submit` copia as palavras de cada entrada no envio (exceto `no_prefetch`;
`HWDER_GPFIFO_LIVE=1` volta ao antigo). **Compilada e testada: travou do mesmo jeito**, aos 139 s,
`gpu: method 0x44 on unbound subchannel 0`, cerca 1:35934 parada em 35928 (6 atrás). Dessa vez sem engasgo longo
antes (o maior foi de 62 ms), então o engasgo não é condição necessária.

#### O que se sabe (2026-11-13)

- Dois canais de GPU (syncpoints 1 e 2), criados no boot; os dois ligam as classes no início (`SET_OBJECT`):
  subcanal 0 `B197` (3D), 1 `B1C0` (compute), 2 `A140` (inline), 3 `902D` (2D), 4 `B0B5` (DMA). Visto com
  `HWDER_GPU_TRACE=3`.
- Os envios com `FENCE_INCREMENT` reservam `2 + extra` valores de syncpoint (`nvdrv.cpp GpuChannel::submit`): 2
  do próprio envio (aplicados por `on_gpu_idle` ao fim de `execute_inner`) e `extra` que o pushbuffer incrementa
  (`SYNCPOINTB`). **Faltar 6 de uma vez** cabe num envio inteiro que não terminou (2 + 4 do pushbuffer). Se fosse
  só lixo no pushbuffer, os 2 finais ainda viriam. Isso aponta para a **thread da GPU parada dentro de um envio**
  ou para trabalho do Vulkan que nunca conclui (os `on_gpu_idle` só disparam quando a GPU real termina).
- Nenhuma espera do runtime registrou atraso (`semaphore acquire … >100 ms`, `pushbuffer syncpoint wait`), e não
  há erro de dispositivo Vulkan no log. Uma espera sem log, ou um shader em laço infinito na GPU real, explicariam.
- `HWDER_GPU_TRACE=3` é pesado demais: o jogo cai para 6–9 fps e não chega à batalha em 200 s (teste inútil para
  o travamento). Não usar para isto.

#### Hipótese 2 (a mais forte agora): compute com memória global e atômicos como no-op

`LDG`, `RED`, `ATOM` e `SUST` aparecem pela primeira vez durante a batalha (141 s na sessão do dono, junto com
três despachos de 41–48 ms). Um compute shader que espera um contador atômico mudar e sempre lê 0 pode ficar em
laço na GPU; o fence do Vulkan não conclui, os `on_gpu_idle` não disparam, o syncpoint para. Implementação em
"Pendências", item 2.

#### Próxima sessão: plano

1. **Diagnóstico pós-morte, barato** (antes de qualquer correção): quando o `vi` registra
   `frame fence … not reached after 1 s`, registrar o estado da thread da GPU: se está executando (qual envio,
   syncpoint, entradas, quantos `extra`), tamanho da fila, quantos `on_gpu_idle` estão pendentes e se o último
   fence do Vulkan foi sinalizado (`vkGetFenceStatus` ou valor do semáforo de timeline). Isso separa
   "thread da GPU presa no runtime" de "GPU real presa num shader".
2. Rodar `battle_long.txt` **sem trace** (cache frio) com o build do `wip/gpfifo-snapshot`, que já registra canal e
   classes no método sem classe.
3. Se for a GPU real presa num compute: implementar memória global e atômicos (item 2 das pendências). Teste
   rápido possível antes disso: pular os despachos de compute cujo shader tenha `ATOM`/`RED`/`LDG` não traduzidos
   e ver se o travamento some (diagnóstico, não correção: imagem errada não é aceitável como solução).
4. Se for a thread da GPU presa no runtime: o pós-morte mostra onde; olhar `flush_commands`/`on_gpu_idle` em
   `gpu/vk`.

### Taxa de quadros

- Menus a 60 fps estáveis. Batalha a ~30 fps: `display: … avg 33–35 ms … limiter 32.6 ms` com a thread da GPU
  ocupada só 6–7 ms por quadro (1.100–1.300 draws). **É o jogo pedindo 30 Hz** (swap interval 2), não falta de
  desempenho do PC.
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
  pré-aquecimento os compila na abertura seguinte; os engasgos diminuem com o uso. O travamento, não.
- Outros números da sessão: `skipped_draws` 486 (draws pulados enquanto o pipeline compila em segundo plano),
  `vk: 120 draws into already-presented buffers` (o quadro apresentado antes de terminar).

## Pendências, por prioridade

1. **Travamento na batalha**: seguir o plano em "Próxima sessão" acima. Quando resolver, juntar ao `main`,
   publicar e pedir ao dono uma sessão longa.
2. **Memória global e atômicos no tradutor** (`LDG`/`STG`/`ATOM`/`ATOMS`/`RED`, também `SUST`, `AL2P` + `IPA`
   indexado, `S2R SR 57`; hoje no-op). Notas de projeto, nada implementado:
   - O caminho de storage buffer do renderer (`vk_resources.cpp`, laço `storage_buffers`) **copia** o buffer do
     guest a cada draw/despacho e só devolve o resultado ao fim da GPU: dois despachos que se comunicam pelo mesmo
     buffer veriam dado velho. Nunca foi usado (o tradutor não gera storage buffers).
   - Melhor caminho: importar a memória do guest como `VkBuffer` com `VK_EXT_external_memory_host` (já habilitada
     em `display.cpp`, sem uso; `vkGetMemoryHostPointerPropertiesEXT` já está no loader), com cache por faixa de
     páginas e barreira de memória depois de despachos que escrevem.
   - No shader, sem análise de fluxo de dados (abordagem do Ryujinx, MIT): comparar o endereço com os 16
     descritores {endereço 64 bits, tamanho} que o NVN deixa no `c0` e acessar um array de storage buffers com
     índice dinâmico (+1 elemento de rascunho para "nenhum"). Isso pede um campo `count` em `StorageBufferUse`
     (`shader.h`), no layout (`vk_pipelines.cpp`), na ligação (`vk_resources.cpp`) e no cache de pipelines
     (`pipeline_warmup_cache.cpp`, subir `kVersion`).
   - **A conferir no dump antes de usar** (lembrados do Ryujinx, não verificados): base dos descritores no `c0`
     por estágio 0x110 (vértice), +0x100 por estágio (0x210 TCS, 0x310 TES, 0x410 GS, 0x510 fragmento),
     0x310 em compute; campos: `LDG`/`STG` endereço em Ra (64 bits com o bit 45 `.E`), imediato com sinal nos bits
     20–43, tamanho nos bits 48–50; `ATOM` Rb 20–27, imediato de 20 bits em 28, tamanho 49–51, operação 52–55,
     `.E` no bit 48; `RED` valor em 0–7, tamanho 20–22, operação 23–25, imediato em 28; `ATOMS` deslocamento de 22
     bits em 30 (×4), tamanho 28–29, operação 52–55.
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
7. **Steam Deck / Linux**: o caminho do HWDER (`tools/linux_setup.py`, build no WSL) ainda não foi adaptado.

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
