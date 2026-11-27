# Pipeline cache, warmup and catalog

## No FEW (2026-11-27)

O catálogo descrito abaixo é usado pelo FEW e vai com o jogo: `tools/pipeline_catalog/few-us-1.5.0.bin`, copiado como
`pipeline_catalog.bin` para a pasta do jogo por `gerar-few.ps1`, `build-linux.sh` (`few_build.py`) e `gerar-deck.ps1`.
Na primeira abertura o jogo prepara esses pipelines antes do primeiro quadro; o que não está no catálogo segue o
caminho de sempre (compilado na hora e guardado no `pipeline_warmup.bin`). Estudo e medições:
[estudo-shaders.md](estudo-shaders.md). Diferenças em relação ao protótipo do HWDER:

- **Fontes:** `catalog_game_sources.cpp` (`few_shader_package()`) aceita os arquivos que têm programas NVN crus no
  US 1.5.0, achados indexando a RomFS inteira com `FEW_CATALOG_SCAN_ALL=1` (investigação: todo arquivo de até
  32 MB exceto vídeo, áudio e textura; 1.378 programas em 6.858 arquivos, 11 s): `nx/shader/*.spkg` (81 pacotes sem
  compressão, 898 programas), `nx/stage/terrain/*-Terrain.bin.gz` (19 terrenos de fase, 447; o `.gz` não é gzip) e
  `nx/posteffect/*.kps`/`.kps3` (33). Lidos através do patch do update 1.5.0.
- **Abertura:** usar um catálogo só precisa da tabela de arquivos (`index(false)`): cada programa é lido no
  deslocamento gravado e conferido pelo SHA-256. Ler e varrer o conteúdo dos arquivos só acontece ao gravar.
- **Compatibilidade:** `catalog_build()` é uma época de ABI, não mais o commit: o catálogo vale entre releases. Cada
  programa é traduzido de novo e só entra se o resultado for igual ao gravado (`expected_translation`); mudar o
  tradutor custa cobertura, nunca correção. Mude a época se o formato ou o sentido de um campo mudar.
- **Outra GPU:** as chaves guardam o formato Vulkan do render target. Na importação, `attachment_format_for_this_gpu()`
  troca D24S8 por D32S8 onde a GPU não tem D24S8 (AMD, Steam Deck). Grave em NVIDIA ou Intel (que têm D24S8).
- **Gravar e somar:** cada rodada com `HWDER_PIPELINE_CATALOG_RECORD=<escopo>` substitui o `pipeline_catalog.bin` da
  pasta (sem um catálogo lá, para não importar). Some as rodadas com
  `python tools/pipeline_catalog/catalog.py merge tools/pipeline_catalog/few-us-1.5.0.bin <atual> <novas>...` e veja o
  conteúdo com `... catalog.py info <arquivo>`. Limites do formato: 512 programas e 8.192 pipelines.
- **Lacuna conhecida:** alguns shaders da batalha do prólogo ainda não estão em nenhum desses arquivos (`stage …
  unresolved in supported local resources` no log da gravação): devem estar em arquivos maiores que 32 MB ou
  comprimidos. Esses continuam compilando na primeira vez.

## Original HWDER notes

How HWDER keeps compiled graphics pipelines between runs. For what a player sees (hitches in the first
minutes on a fresh install) see [Playing HWDER](playing.md#the-first-minutes-on-a-fresh-install-can-hitch).

## Pipeline warmup (`pipeline_warmup.bin`)

Successful graphics pipelines are recorded with their shader code and binding metadata in
`pipeline_warmup.bin`. A compatible file prebuilds those recipes on the single pipeline
worker before the startup screen and guest boot. Warmed results are reused only after
fresh live translation exactly matches the saved shader and metadata. Absent or incompatible
files leave startup lazy. `HWDER_PIPELINE_PREWARM=0` disables early prebuilding for comparison
while continuing to record successful live pipelines. The file is disposable; delete it to
start fresh. Logs report startup costs, completion before the first live draw, adoption,
mismatches, failures and recording. Performance and visual acceptance require matched runs.

## Metadata pipeline catalog prototype

`pipeline_catalog.bin` is a separate metadata-only format for collecting the pipelines observed on a named stage/route. Set `HWDER_PIPELINE_CATALOG_RECORD=sealed-grounds` for a collection run. Successful requested graphics pipelines periodically export their shader resource locators, SHA256 identities, translation settings and complete static pipeline state. The scope name describes the run; it does not promise complete stage or game coverage. No game shader code, SPIR-V, native pipeline cache or key material is exported. Keep `pipeline_warmup.bin` private: it contains compiled shader code and is **not** this catalog.

With a compatible catalog beside the executable, startup first lets you select your game, then resolves the original programs from **your selected local NSP or extracted RomFS**, verifies their contents and translates/compiles locally. The graphics preparation screen reports progress and waits for known jobs before guest boot. Every warmed result still requires the existing fresh full live-program/static-state verification before use. Missing, incompatible, changed or unsupported sources retain normal private-cache/lazy compilation. `HWDER_PIPELINE_CATALOG=0` disables catalog import; `HWDER_PIPELINE_PREWARM=0` disables startup prebuilding while recording remains available.

This first prototype supports raw NVN programs in four actual shader archives: `data/action/shader/Normal.bin.gz`, `data/deferred/DeferredShader.bin.gz`, `data/shadow/ShadowShader.bin.gz`, and `data/stage/common/StageShader.bin.gz`. Their shader chunks are directly readable despite the `.gz` filenames. Other compressed/relocated resource variants, VertexA merging and translator callbacks requiring uncaptured constant-buffer/texture context are deliberately unresolved. Collection logs report these gaps; no previously generated shader cache substitutes for source resolution. Catalog compatibility includes the compiled build/ABI, HWDE title and game version; catalogs must be regenerated when these change.

Offline checks and a read-only source proof tool use the same required build lock:

```
python tools/build.py --target pipeline_catalog_test
build\release\pipeline_catalog_test.exe
python tools/build.py --target pipeline_catalog_tool
build\release\pipeline_catalog_tool.exe "your-game.nsp" "your-prod.keys" "build\release"
```

The tool reads the user's game and prints metadata counts without writing shader or game bytes. Its optional trailing arguments `catalog.bin build-tag game-version` verify every catalog program by fresh local resolution/translation without persisting compiled output. The synthetic tests generate their own tiny programs in memory and cover format integrity, compatibility, bounds, deterministic identity/roundtrip and safe unresolved cases. Runtime evidence and exact one-stage coverage are recorded separately in `../docs/graphics-coordination/catalog-agent.md` (relative to the repository root, outside this repository).
