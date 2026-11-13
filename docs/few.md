# Fire Emblem Warriors — fatos medidos no dump

Fonte de verdade sobre o jogo. Tudo aqui foi medido no dump do próprio dono (`Game Files/`, ignorado pelo git) com
`tools/inspect_dump.py` e `tools/module_census.py`. Nada de chave, código ou asset do jogo entra neste arquivo.

## Alvo

| Item | Valor |
|---|---|
| Jogo | Fire Emblem Warriors (US) |
| Base | `0100F15003E64000` v0, SDK 1.6.0, keygen 3 |
| Update | `0100F15003E64800` v327680 = **1.5.0**, SDK **3.7.0**, keygen 4 |
| DLC (AddOnContent, tipo `publicdata`) | `…65001` Lucina's Bridal Costume v0 · `…65002` Fates Pack v131072 · `…65003` Shadow Dragon Pack v65536 · `…65004` Awakening Pack v65536 · `…65005` Japanese-Voice Pack v0 |
| Engine | Koei Tecmo **KTGL** sobre NVN (mesma família do Hyrule Warriors DE) |
| Projeto interno do `main` | `c:\Troop\Program\Output\NX64\Release\Troop.nss` (HWDE: `Swim.nss`) |
| Idiomas (NACP) | en-US, en-GB, fr, de, es-419, es, it, fr-CA |
| Save (NACP) | `user_account_save_size` 0x400000 |

## Por que o código vem do update

O update 1.5.0 foi recompilado num SDK novo e **troca o conjunto de módulos inteiro**:

| | Base v0 | Update 1.5.0 | HWDE (referência, já roda no HWDER) |
|---|---|---|---|
| SDK | 1.6.0 | 3.7.0 | 3.5.1 |
| Módulos | rtld, main, subsdk0 `glslc`, subsdk1 `libcurl`, subsdk2 `multimedia`, subsdk3 `nvn`, subsdk4 `nnDisplay`, sdk | rtld, main, subsdk0 `glslc`, subsdk1 `multimedia`, sdk (`nnSdk` com NVN e libz dentro) | igual ao update |
| Texto (código) | 24,8 MB | 27,4 MB (6,84 M instruções) | 21,1 MB |
| SVCs distintos | 104 | 106 | 106 (mesmo conjunto) |
| Fim dos módulos | 0x1003800000 | 0x1003600000 | — |

Consequência: recompilamos **só o ExeFS do update**; a base entra apenas com o RomFS, corrigido pelo patch BKTR do
update. `tools/game_update.py` desempacota o ExeFS inteiro do update e o runtime (`fs/nsp.cpp read_exefs`) não cai
para módulos da base quando há update.

### Módulos do update 1.5.0

| Módulo | Base | Texto | Build ID (16 bytes) | SHA-256 |
|---|---|---|---|---|
| rtld | 0xFFFE00000 | 6 048 | ca0198cc95cefec5f60e539d9926a79a | e6e39e21…af76091 |
| main | 0x1000000000 | 12 612 928 | 1953770037acc52a4e28bf7aa5968c03 | 735833f0…2dab1ea4ad |
| subsdk0 (glslc) | 0x1001400000 | 7 603 892 | 30fe832b279d4b7c7b63023c995db787 | de261338…f512ce87 |
| subsdk1 (multimedia) | 0x1002200000 | 3 098 036 | 434bc9467960230aee33c2fad7940cc1 | 7935ed35…7a244737 |
| sdk (nnSdk) | 0x1002A00000 | 4 058 360 | 4fea41dd7345eca3a4ce829c398227a4 | 4d269e54…551b5bd4 |

Hashes completos em `tools/supported_games.json`.

Imports do `main` (1.5.0) por namespace: nn::audio 85, nn::os 51, nn::fs 22, nn::hid 18, nn::util 14, nn::nfp 14,
nn::vi 8, nn::mem 7, nn::oe 6, movie 15, nn::time 4, nn::crypto 3, nn::web 3, nn::account 3, nn::aoc 2,
nn::settings 2, nn::ec 1, `nv::InitializeGraphics`, `nv::SetGraphicsAllocator`.

## RomFS

- Base: 6 440 arquivos, 13,02 GiB. Maiores pastas: `nx/movie/eng` (36 `.mp4` H.264, 2,8 GiB), `nx/stage/model`
  (2,5 GiB), `nx/action/model`, `nx/voice/eng`, `nx/sound/bgm` (um `.g1l` de 1 GiB), `nx/shader` (81 `.spkg`).
  Formatos: `.gz` (4 683), `.bin`, `.g1l` (áudio), `.g1t` (textura), `.arc`, `.spkg` (shaders).
- Update: RomFS final de 14 115 004 508 bytes por tabela indireta (565 trechos), patch de 280 MB.
- DLCs: cada uma é um RomFS próprio (`nx/action/model`, `nx/voice/eng` + `jpn`, `nx/sound/bgm`, `common/talkEvent` …).
  O Japanese-Voice Pack traz `nx/voice/jpn` (1,5 GiB) e um BGM próprio.

## Vídeos

`nx/movie/eng/*.mp4` (`tools/movie_info.py`): H.264 High@4.1, 1920×1088, 30 fps, POC tipo 0
(`log2_max_poc_lsb_minus4` 1), `log2_max_frame_num_minus4` 0, `max_num_ref_frames` 2, quadros B
(`max_num_reorder_frames` 1), CABAC, predição ponderada (`weighted_pred` 1, `weighted_bipred_idc` 2),
`num_ref_idx_l0/l1` 2/1, transform 8×8, `chroma_qp_index_offset` −4, **um único IDR por vídeo**.
O bloco de parâmetros do NVDEC traz `num_ref_idx_*_default_active_minus1` (1 e 0 aqui).

## Execução (medido)

- Entrada extra necessária: `glslc+0x6c3ff4` (`0x1001ac3ff4`), a mesma do HWDE no mesmo `glslc`.
- O jogo monta as cinco DLCs no boot com `fsp-srv OpenDataStorageByDataId(storage 0, data id = título da DLC)`.
- Save: `save/0100F15003E64000/<usuário>/system` (496 bytes) logo após "Creating new save data".
- Batalha do prólogo: 1.000–1.300 draws por quadro, 2 dispatches, 30 fps pedidos pelo jogo.
- O pós-processamento lê profundidade/G-buffer com `TLDS` (sem ele, a cena inteira vira neblina).
- Batalha: o jogo pede 30 Hz (`limiter` ~32,6 ms por quadro) com a thread da GPU ocupada 6–7 ms por quadro.
- Primeira sessão com cache frio: compilações síncronas de pipeline de 100–330 ms quando surgem efeitos novos;
  `skipped_draws` 486 em ~30 s de batalha.
- Travamento reproduzível na batalha do prólogo (~30–50 s de luta com cache frio), descrito em
  [handover.md](handover.md#travamento-na-batalha).
- Há shaders com `SUST` (escrita em imagem) e `ATOMS`; o efeito deles ainda não foi identificado. Em 2x a
  composição cobre só o quadrante superior esquerdo, a forma típica de compute com grade em tamanho nativo
  (diagnóstico provável, não confirmado).

## Comparação com a versão japonesa v0 (dump descartado)

O primeiro dump era o japonês `0100EAE003A5A000` v0: mesmos 8 módulos da base US, SDK 1.6.0, RomFS só com pastas
`jpn`. Foi trocado pelo US + update + DLC; nada derivado dele ficou no disco.
