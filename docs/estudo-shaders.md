# Estudo: shaders prontos antes de jogar (2026-11-27)

## Implementado (2026-11-27)

O dono aprovou a fase 1 e a distribuição do catálogo no repositório (exceção no `AGENTS.md`). Feito:

- **Catálogo** `tools/pipeline_catalog/few-us-1.5.0.bin` (230 KB, 125 programas, 215 pipelines), gravado com os
  roteiros `battle_long`, `pause_menu`, `a_every_2s` e `prologue_battle` e somado com
  `tools/pipeline_catalog/catalog.py merge`. Copiado como `pipeline_catalog.bin` para a pasta do jogo por
  `gerar-few.ps1`, `few_build.py` e `gerar-deck.ps1`.
- **Fontes:** além dos `.spkg`, os terrenos das fases (`nx/stage/terrain/*-Terrain.bin.gz`) e os pós-efeitos
  (`nx/posteffect/*.kps*`), achados indexando a RomFS inteira (`FEW_CATALOG_SCAN_ALL=1`). Avisos de "unresolved" na
  gravação da batalha: de ~82 para 35.
- **Chave por época** (não mais o commit), **formato de profundidade da GPU** aplicado na importação (D24S8 → D32S8
  no AMD/Deck) e **abertura só com a tabela de arquivos** (16 ms; o conteúdo só é lido ao gravar).
- Corrigido de passagem: o `pipeline_catalog_test` não linkava (faltava `structurize.cpp` na lista de fontes).
- **Verificação** (caches apagados, só o catálogo, batalha longa): 125 de 125 programas achados, 215 de 215
  pipelines prontos antes do jogo começar, 190 dos 244 usados vieram do catálogo; na batalha, 0 quadros > 100 ms
  (sem catálogo: 5) e 24 desenhos pulados (sem catálogo: 561). O cache em disco do driver da NVIDIA já tinha esses
  pipelines, então o tempo de preparo medido (0,6 s) é otimista; frio, ~100 ms por pipeline (~20 s para os 215).
- **Falta:** mais rotas (os roteiros só alcançam o prólogo; o `a_every_2s` quase não sai dos menus), os 35 shaders
  ainda não achados (arquivos > 32 MB ou comprimidos), testar no Deck, preparo paralelo e a fase 3 abaixo.

## O estudo

Pedido do dono: estudar pré-compilar shaders para reduzir as travadas. Resumo: **dá, e o HWDER já traz quase tudo**.
Um catálogo de pipelines só com metadados, gravado por nós jogando, faz o jogo do jogador compilar antes de começar o
que antes compilava no meio da batalha. Um experimento no FEW tirou todas as travadas da batalha do prólogo cobertas
pelo catálogo. Falta cobertura (shaders fora de `nx/shader`), a chave de compatibilidade e uma decisão de política.

## O problema, medido

Batalha longa (`tools/input/battle_long.txt`, 330 s) no notebook de teste (NVIDIA), mesmo executável:

| | Cache frio (primeira vez) | Cache quente (segunda vez) |
|---|---|---|
| Pipelines compilados durante o jogo (> 30 ms) | 81, total 10,2 s, média 126 ms, pior 732 ms | 0 |
| Quadros > 100 ms na batalha (95–330 s) | 30, pior 716 ms | 0 |
| Desenhos pulados (efeito que "pipoca") | 1.319 | — |

Só a primeira vez trava: o `pipeline_warmup.bin` e o `pipeline_cache.bin` resolvem a segunda. O prólogo é uma fração
do jogo; cada fase, personagem ou efeito novo trava de novo na primeira vez. Hoje (`vk_pipelines.cpp`): quadros leves
(< 64 desenhos) e desenhos que leem render target compilam **na hora** (o quadro para 50–250 ms); os outros compilam
num worker e o desenho é pulado até ficar pronto. Toda criação é serializada por um mutex global (um driver caiu com
duas threads criando ao mesmo tempo).

## Onde estão os shaders do FEW

`nx/shader/` tem 81 `.spkg` (20 MB, magia `SPKG`, **sem compressão**) com **898 programas NVN crus** (cabeçalho
`0x12345678` + SPH), exatamente o que o indexador do catálogo do HWDER procura (`catalog_game_sources.cpp`). O HWDER
só conhecia quatro arquivos do Hyrule Warriors; no FEW basta aceitar `nx/shader/*.spkg` (mudança local de 6 linhas,
`few_shader_package()`, **ainda não publicada**). A leitura passa pelo patch do update 1.5.0.

## O catálogo do HWDER

`pipeline_catalog.bin` (`docs/pipeline-cache.md`, `gpu/vk/pipeline_catalog.*`): para cada programa, o arquivo da
RomFS, o deslocamento, o tamanho e o **SHA-256** dos bytes do shader, as opções de tradução e o estado fixo de cada
pipeline. **Nenhum código de shader, SPIR-V nem cache nativo.** Grava-se com
`HWDER_PIPELINE_CATALOG_RECORD=<escopo>`; com o arquivo ao lado do executável, a abertura acha cada shader na cópia
do jogador, confere o SHA-256, traduz, compila tudo num worker e só então começa (tela "Preparing graphics
programs"). Cada pipeline preparado ainda passa pela conferência completa do programa ao vivo antes de ser usado.

## Experimento (2026-11-27, só local)

A: cache frio, gravando (`HWDER_PIPELINE_CATALOG_RECORD=prologue-battle`). B: cache frio de novo, só com o catálogo
de A (170 KB: 86 programas, 162 pipelines).

| | A, sem catálogo | B, com catálogo |
|---|---|---|
| Programas do catálogo achados na cópia do jogo | — | 86 de 86 (0,2 s para indexar os 898) |
| Pipelines prontos antes do primeiro quadro | 0 | 162 de 162 (em 4,8 s) |
| Usados durante o jogo | — | 148 (`adopted`) |
| Quadros > 100 ms na batalha (140–330 s) | 5 | **0** |
| Desenhos pulados | 561 | **51** (−91%) |

Ressalva: o cache em disco do driver da NVIDIA já tinha esses pipelines (rodadas anteriores), então A não mostra os
126 ms por pipeline do cache realmente frio; num PC ou Deck novo os 162 levariam uns 16 s na abertura (serializados).

## O que falta

1. **Cobertura.** Em A, 82 avisos `stage … unresolved in supported local resources`: ~38% dos pipelines da rodada
   vêm de shaders que não estão nos `.spkg` (B ainda gravou 92 pipelines novos). Achar o contêiner deles: procurar o
   SHA-256 desses estágios no resto da RomFS (`.g1m`/`.gz` de modelos e efeitos são os candidatos) e acrescentar
   esses arquivos ao indexador.
2. **Chave de compatibilidade.** `catalog_build()` inclui `HWDER_COMMIT`: qualquer build novo invalida o catálogo.
   Trocar por uma época de ABI (como o `warmup_compatibility()` já fez, pelo mesmo motivo) e confiar na conferência
   por programa. Lição do Odyssey: uma versão deles saiu com 63% da semente inútil depois de mudar o tradutor; ter
   na release uma checagem de quanto do catálogo ainda resolve (eles param a release com mais de 5% perdido).
3. **Gravar rotas.** Os roteiros de `tools/input/` cobrem menus, prólogo e batalha; o resto do jogo precisa de mais
   rotas (história, mapas, personagens, DLCs). Como o arquivo só tem metadados, catálogos de jogadores podem ser
   somados. Falta uma ferramenta de união (`warmup::merge` já existe para o cache privado).
4. **Tempo de preparo.** Serializado, a ~100 ms por pipeline: 2.000 pipelines ≈ 3–4 min na primeira abertura.
   Opções: liberar workers paralelos onde o driver aguenta (RADV, NVIDIA) mantendo o mutex para os outros; ou fazer o
   preparo no fim do build, como 7ª etapa da janela de progresso (a máquina do build é a do jogo).
5. **Política.** O catálogo é derivado do dump (nomes de arquivos, deslocamentos, SHA-256 dos shaders, estado de
   pipeline), mas não tem conteúdo do jogo, como o `tools/recomp/symbols.txt` que já é versionado. O `AGENTS.md`
   manda deixar fora do git tudo que vem do dump e os caches: **decisão do dono** se o catálogo entra no repositório,
   só nas releases, ou não é distribuído.

## Alternativas consideradas

- **Compilação toda assíncrona** (`HWDER_ASYNC_RT_PIPELINES=1`): tira a parada do quadro, mas o efeito some por um
  ou dois quadros (já foi uma caixa preta num ataque). Não resolve, troca o defeito.
- **`VK_EXT_graphics_pipeline_library`**: compilar cada estágio uma vez (os 898 programas, sem depender do estado) e
  ligar o pipeline em < 1 ms na hora do desenho; cobre até combinações nunca gravadas. RADV (Deck) e NVIDIA têm. É o
  fim do problema, mas é uma mudança grande no renderer.
- **Distribuir `pipeline_warmup.bin`**: proibido, tem SPIR-V traduzido do jogo.

## Recomendação

Fase 1 (dias): o indexador dos `.spkg` (feito, local), a chave por época, achar os shaders que faltam, gravar o
prólogo e as primeiras batalhas com os roteiros, e publicar o catálogo nas releases ao lado do jogo, com a checagem
de cobertura no empacotamento. Fase 2: preparo paralelo e etapa na janela de build; rotas e catálogos de jogadores.
Fase 3, se ainda houver travadas: graphics pipeline library.
