# Handover — estado e próximos passos

Atualizado em 2026-11-27. **Próxima sessão: comece por "Começar aqui (2026-11-26)"**, logo abaixo; o trabalho no
jogo continua em "Retomar aqui (2026-11-16)". Leia antes de qualquer tarefa; atualize ao terminar uma sessão.

## Começar aqui (2026-11-26)

### Onde está cada coisa

- **GitHub `main`** (`Azzulin/FEW-Recomp`, **público**: a nota de "privado" em "Release pública" abaixo está
  desatualizada), publicada em 2026-11-27: correções das issues #2, #3 e #1 (`5ac2be3`, `6e4941e`, `0a04d44`), os
  símbolos por assinatura da #1 (`741381b`) e, no commit seguinte, o pacote e o setup do RecompLauncher.
- **Release v0.3.0** (2026-11-27, release normal, tag `v0.3.0`): tudo acima, com os assets
  `FEW-Recomp-v0.3.0-Windows-x64.zip` e `FEW-Recomp-v0.3.0-Linux-x64.tar.gz` para o RecompLauncher. v0.1.0 e v0.2.0
  continuam como pré-release.
- **Release v0.5.0** (2026-11-27, a mais recente): `FEW Recomp Setup.exe` no zip do Windows, catálogo de pipelines
  (shaders prontos antes de jogar), `aoc_dir`. A **primeira feita pelo workflow Release** do GitHub Actions
  ([releasing.md](releasing.md)); as anteriores foram empacotadas à mão.
- **Release v0.4.0** (2026-11-27, release normal): build ~3x mais rápido e janela de progresso
  (seção "Build 3x mais rápido e janela de progresso"), com os mesmos dois assets e `SHA256SUMS`. Publicada **sem
  teste no Deck** (o dono não tem como testar agora): o tempo no Deck foi pedido ao usuário da #3.
- A cópia local `Downloads\FEW-Recomp-main` é um zip extraído, **sem git**; batia com a `main` depois desse push.
  Para publicar algo dela: clonar o repositório, copiar por cima os arquivos mudados e conferir com `git diff`.
  `few-prepare.sh` precisa de modo 100755 no índice (`git update-index --chmod=+x`); no Windows entra como 100644.

### Issues (todas abertas)

| Issue | Estado | Próximo passo |
|---|---|---|
| #2 crash no menu Camp (Windows) | Corrigido em `5ac2be3` (TLS e handles das threads que terminam; ver "Crash da issue #2"). Em 27/11 comentado que está na v0.3.0, com pedido de teste (mais de 10 min, abrir o Camp) | Esperar o retorno; se fechar de novo, pedir o `hwder.log` |
| #3 build no Deck demorado / "fail updated" | `6e4941e`; em 27/11 avisado que está na v0.3.0. Também em 27/11: avisado da v0.4.0 (build ~3x mais rápido, janela de progresso) e pedido o tempo do build no Deck | Esperar o tempo dele e se a janela abriu; se falhar, o `build-linux.log` |
| #1 versão japonesa | **Funciona** (testador, 27/11): os 10 símbolos achados por assinatura (`same code, moved in this build`), jogo em japonês a 60 fps. Relatou engasgos na compilação de shaders na primeira vez ("igual ao emulador") e pediu DualSense. Respondido em 27/11: engasgos diminuem com o cache (`pipeline_*.bin`); DualSense já é suportado pelo SDL3 (giroscópio, vibração), pedido o `hwder.log` se não funcionar | Ver se o DualSense dele funciona; os engasgos de primeira vez são a pendência "Engasgos de compilação" |

### Pendências, por ordem

1. **Testar dentro do RecompLauncher no Deck: adiado pelo dono em 2026-11-27** (o Deck não tem o RecompLauncher
   instalado). O Windows passou em 2026-11-27: setup completo com o dump real
   (~50 min) e o jogo abriu pelo launcher com update, DLCs e 60 fps ([docs/recomplauncher.md](recomplauncher.md),
   "Testado e pendente"). A seção Developers do launcher só aparece com conta de desenvolvedor verificado ou com
   uma build local já adicionada; sem conta, use o modo de teste (mesmo documento). Pacotes de teste em
   `Downloads\FEW-Recomp-launcher-test` (`v0.3.0-rc1`; o zip do Windows extraído em `windows\`).
2. **Próxima release: pelo GitHub Actions**, [docs/releasing.md](releasing.md) (seção no CHANGELOG, *Actions >
   Release > Run workflow*, conferir o rascunho, publicar). Cada release faz o launcher recompilar o jogo de quem
   já tem.
3. **Catálogo do RecompLauncher**: conta de desenvolvedor pedida pelo dono em 2026-11-27, esperando a aprovação
   manual. Depois, envio em
   `recomplauncher.com/submit?platform=pc`. Conferir se o launcher aceita só o tarball no Linux (o guia pede também
   um AppImage).
4. **Responder à #2** depois da validação; na #1, acompanhar o teste do usuário japonês.
5. **Build 3x mais rápido e janela de progresso** (seção abaixo): na **v0.4.0**. Falta o número real no Deck (tempo
   do build e se a janela abre no Modo Desktop): pedido ao usuário da #3; o dono não tem como testar agora.
6. **Shaders prontos antes de jogar** (fase 1 feita em 2026-11-27, [docs/estudo-shaders.md](estudo-shaders.md),
   "Implementado"): catálogo `tools/pipeline_catalog/few-us-1.5.0.bin` versionado (o dono aprovou), copiado para a
   pasta do jogo pelos três scripts; na batalha do prólogo, sem cache, 0 travadas e 24 efeitos atrasados (eram 561).
   Próximos: gravar mais rotas (somar com `tools/pipeline_catalog/catalog.py merge`), os 35 shaders ainda não
   achados, testar no Deck, preparo paralelo. Ao mudar o tradutor de shaders, gravar de novo: programas cuja tradução
   mudou deixam de valer (o log da abertura diz quantos: `catalog: local translation: N resolved, M unresolved`).

### Armadilhas desta sessão

- A cópia local sem git foi a base de tudo. Ela batia com a `main` em 2026-11-26; para outra cópia, confira com
  `git diff` antes de copiar arquivos.
- Clone no Windows com `core.autocrlf=true`: o empacotador lê do **índice** do git (LF; `.bat` em CRLF), nunca da
  árvore de trabalho. `git add` antes de empacotar; `--allow-dirty` só para teste.
- O `.gitignore` ignora `*.bat` e `*.ps1`. Arquivo novo desse tipo precisa de exceção `!caminho`.
- Commits: identidade `Azzulin <126606424+Azzulin@users.noreply.github.com>` (`git config` local no clone),
  mensagens em português sem acento e sem prefixo de tipo, mais o trailer `Co-Authored-By`.
- No Windows, o toolchain do setup do launcher fica em `%LOCALAPPDATA%\HWDER`, não em `{data}`, por causa do limite
  de 260 caracteres de caminho (medido). Detalhes em [docs/recomplauncher.md](recomplauncher.md).
- Teste de ponta a ponta: rodar o `few-prepare.bat` direto, sem `cmd /c`, que remove as aspas de caminhos com espaço.
  O teste do Linux roda no WSL `archlinux`, em `/tmp`. Limpe `%TEMP%\fewt` e `/tmp/few-e2e` no fim.

## FEW Recomp Setup e publicação pelo GitHub Actions (2026-11-27)

Pedido do dono: o build pelas instruções estava difícil; queria no Windows uma tela como a do Setup do HWDER (escolher
NSPs, DLCs e update) e um processo de versão que ele mesmo consiga rodar.

- **`FEW Recomp Setup.exe`** = `tools/build_ui` sem `--events` no Windows (`setup.cpp`; o visual e a tela de progresso
  vêm do `ui.h`, compartilhado com a janela de progresso, que segue servindo ao `build-linux.sh`). Um botão principal:
  arquivos → **Build the game** → progresso (Cancel) → **Play** / **Desktop shortcut** / **Open game folder**.
  - Reconhece cada NSP pelo ID do título do ticket (`<rights id>.tik` na tabela PFS0, que não é criptografada), senão
    pelo `[0100…]` do nome. "Choose a folder with all of them" acha base, update, DLCs e `prod.keys` (na pasta, uma
    acima, `%USERPROFILE%\.switch` ou ao lado do kit). Escolhas em `%LOCALAPPDATA%\FEW-Recomp\setup.ini` (vale entre
    versões); build concluído em `build\setup-built.ini` (versão + caminhos): abrir de novo cai direto em Play.
  - Roda `powershell gerar-few.ps1 -NoUi -BaseNsp -UpdateNsp -Keys [-DlcDir]` com `FEW_EVENTS` apontando para
    `build\build-events.jsonl` (o `gerar-few.ps1` grava nele sem abrir outra janela), num *job object* (Cancel e
    fechar a janela matam tudo), saída em `build\setup-build.log`. PowerShell **minimizado e visível**, nunca oculto.
  - Ícone próprio (`make_icon.py` → `few.ico`, o mesmo brasão), manifesto `asInvoker`, informações de versão,
    `-Wl,--no-insert-timestamp`: as práticas do HWDER contra falso positivo de antivírus (`docs/antivirus.md`).
  - Testes: `FEW_SETUP_FOLDER=<pasta>` (como escolher a pasta), `FEW_SETUP_AUTOBUILD=1` (aperta Build) e
    `FEW_BUILD_UI_SCREENSHOT=<bmp>` (espera o build terminar). De ponta a ponta no Windows em 2026-11-27: a pasta do
    dump detectada (base e update US, 5 DLCs), build pelo setup em 163 s (incremental) e tela de pronto.
- **DLCs em outra pasta:** chave `aoc_dir` no `hwder_settings.ini` (UTF-8; o `core.cpp` a passa como
  `HWDER_AOC_DIR` antes do NSP). `gerar-few.ps1 -DlcDir` grava; vazio = pasta da base.
- **Publicação** ([releasing.md](releasing.md)): `.github/workflows/release.yml` (Windows 2022) instala o toolchain
  fixado (cache), baixa as bibliotecas fixadas (`tools/fetch_deps.py`, agora nos commits do `gerar-few.ps1`), compila
  o setup, empacota (`package.py --setup-exe`, que também grava o `SHA256SUMS`), gera as notas do CHANGELOG
  (`tools/release_notes.py`) e cria a release **em rascunho**. `ci.yml` roda os testes a cada push. Pelo site:
  *Actions > Release > Run workflow* com a versão e "release" marcado.
- **Rodado no GitHub em 2026-11-27:** a v0.5.0 saiu pelo botão Run workflow (2 min 26 s, rascunho conferido: zip
  baixado, `FEW Recomp Setup.exe` na raiz, aberto da pasta extraída, versão v0.5.0) e um ensaio `v0.0.0-test` passou
  inteiro (artifact de 33 MB). Dois tropeços corrigidos: o contexto `runner` não vale nas variáveis do job, e o
  PowerShell do GitHub termina o passo com o último código de saída (por isso o `exit 0` nas notas do ensaio).
  Publicar o rascunho cria a tag, que roda o workflow de novo: o passo "Already released?" o encerra sem erro.
- **Falta:** o setup numa máquina limpa (SmartScreen, download do toolchain do zero) e um build completo a partir de
  um kit recém-extraído (o teste de ponta a ponta foi incremental, na pasta de desenvolvimento). Ações do GitHub em
  Node 20 dão aviso de obsolescência (`checkout@v4`, `setup-python@v5`, `cache@v4`): subir de versão quando houver.

## Build 3x mais rápido e janela de progresso (2026-11-27, release v0.4.0)

Pedido do dono: o build do Super Odyssey PC Recomp leva 20–30 min no Deck; o nosso, horas. Medido antes de mudar:

- **98% do tempo é o código do jogo** (414 arquivos, 5,8 h de CPU no build do launcher; o runtime, 380 s). **88%**
  está em 96 arquivos com uma função de ~20.900 linhas cada, quase-clones (diferem em endereços de retorno e alguns
  imediatos: um template do jogo instanciado 96 vezes). Ex.: `rc_main_00b.c` = `f_10000b0b04`.
- `-ftime-report` (Zig, `-O2`): **SROA 46% e LoopRotate 41%**. Causa: o gerador (`tools/recomp/a64_simd.py`) emitia
  cada instrução NEON como `for (int i = 0; i < N; i++) …`; só aquela função tinha 5.356 laços, e o LLVM roda esses
  passes por laço sobre a função inteira.
- **Correção:** `unroll_lanes()` no `a64_simd.py`, aplicado em `translate()` e `ld_struct()`: os laços saem
  desenrolados (cada lane no próprio bloco, `i` trocado pelo número). Conferido: o C novo é **byte a byte** o antigo
  com essa transformação (416 arquivos) e o código de máquina do arquivo grande ficou igual (`.text` −0,4%, mesmo
  número de instruções vetoriais). Um corpo que mexa em `i` ou saia do laço para o gerador com `RuntimeError`.
- **Debug info fora do código do jogo** (`HW_GAME_DEBUG_INFO=OFF` no CMake; o Zig precisa de `-s`, porque põe
  `-gdwarf-4` sozinho mesmo sem `-g`). Nomes `f_<endereço>` vêm da tabela de símbolos e o unwind das tabelas de
  unwind; `symbolize_samples.py` segue funcionando (o runtime mantém `-g`).
- Números do `rc_main_00b.c` (4 compilações em paralelo, notebook de 12 threads):

  | Compilador | Antes | Depois |
  |---|---|---|
  | clang do Windows (llvm-mingw), `-O2 -g` | 186 s | 64 s |
  | Zig (Linux/Deck), `-O2 -g` | 410 s | 168 s; **141 s** sem debug info |

- **Build completo no Windows** (8 jobs, `gerar-few.ps1 -SoBuild` do zero): ninja de **44,5 → 15,4 min**, CPU do
  código do jogo de 20.963 → 7.011 s, `few.exe` de 652 → 202 MB. No Deck ainda **não medido** (estimativa pela
  proporção: ~1/3 do tempo de antes).
- Ganhos não adotados: `-O1` nas funções gigantes (no arquivo original, mais −20%; precisa provar que não custa fps:
  o amostrador só pega a MainThread quando detecta batalha, o que não aconteceu nesta build); clang ligado à glibc no
  Linux (~1,5x mais rápido que o do Zig, cuja musl aloca devagar; +1 GB de download no Deck). Maior alavanca futura:
  o gerador emitir **uma** função parametrizada no lugar dos 96 quase-clones.

**Janela de progresso** (`tools/build_ui/`, SDL3 + Dear ImGui no renderer do SDL, visual próprio inspirado no jogo, sem
arte dele): mostra passos, barra, arquivos, tempo restante e, no fim, **Play**. Só lê `build/build-events.jsonl`
(formato em `tools/build_events.py`): `gerar-few.ps1` e `few_build.py` gravam os passos, `tools/build.py` o progresso
do ninja (ponderado pelo tamanho de cada fonte; um *dry run* `ninja -n` lista o que falta). É compilada em segundo
plano (`build.py --build-ui`, em `build/ui` ou `build/ui-linux`) enquanto o update é desempacotado e o C gerado, e
aberta pelo próprio `build.py --build-ui --open-events … --watch-pid …` quando fica pronta (esse modo não pega a
trava de build: o build do jogo nunca espera por ela), com `--pid` do build para notar um terminal fechado. Fechar a
janela não para nada. `-NoUi`/`--no-ui` desligam; os setups do RecompLauncher passam essas opções.

- Peso de cada arquivo no progresso: `tamanho + (maior função)² / 1 MB` (r = 0,92 com os tempos medidos; só o
  tamanho, 0,87). O tempo restante ainda erra para menos quando só sobram gigantes rodando em paralelo.
- Testado no Windows: três estados por capturas (`FEW_BUILD_UI_SCREENSHOT=<bmp>` salva e fecha), e de ponta a ponta
  com `gerar-few.ps1 -SoBuild` (40 arquivos recompilados): eventos completos, a janela compilou em ~95 s (SDL do
  zero, 4 jobs) em paralelo e abriu sozinha na tela "pronto". Batalha longa (`battle_long`, 330 s, cache de shaders
  quente) com o `few.exe` novo e o antigo: 59,5 contra 59,4 fps, thread do jogo 16,6 ms/quadro nos dois.
- Testado no Linux (WSL archlinux, Zig e `linux-deps` do Deck, em `/tmp/few-ui`): a janela compila em 43 s (2 jobs) e
  abriu pelo WSLg durante a tradução. **O build completo no WSL não terminou**: o Claude Code o parou por falta de
  memória no Windows durante a tradução (10 jobs, VM com 10 GB). Não há ainda o tempo total no Linux; repetir com
  `--jobs 4` ou medir direto no Deck.

## Release pública (2026-11-24)

Sessão de 2026-11-24, só repositório e documentação (nenhuma mudança no runtime nem no gerador). Commits `fc40da5`,
`e42aba8`, `70d7d38`, `44468e6` e `9ec199f`, enviados ao `origin/main`; a árvore ficou limpa.

**Histórico reescrito em 2026-11-24** (`git filter-repo --replace-text`, push forçado da `main` e das tags) para
tirar dados pessoais antes de o repositório poder ficar público: IP do Deck, nome da chave SSH, UUID do cartão SD,
appid do atalho e nomes de pastas locais viraram marcadores (`<ip-do-deck>`, `<chave-do-deck>`, `<cartao>`,
`<appid>`). Todos os hashes anteriores mudaram (os citados em seções antigas deste arquivo, como `8d32b02d…`, são
SHA-256 de binários, não commits). Endereço e chave do Deck ficam agora em `tools/deck/deck.local`, ignorado pelo
git e lido pelos scripts de `tools/deck/`. **Nunca versionar IP, chave, UUID do cartão ou caminhos locais.**

Releases **v0.1.0** e **v0.2.0** no GitHub, as duas como *prerelease* (só código; cada um gera a partir do próprio
dump; o repositório seguia **privado**, por escolha do dono; em 2026-11-26 já estava **público**: não mude a
visibilidade sem ele pedir). Para jogadores: `README.md`
e `GUIDE.md`, em inglês. Nova release: atualizar o `CHANGELOG.md` (entradas "FEW Recomp vX.Y.Z"; o histórico do
HWDER fica abaixo, em "HWDER history"), criar a tag `vX.Y.Z` e publicar com notas em inglês. Sem o `gh` instalado,
a release sai pela API REST com o token de `git credential fill` (nunca imprimir o token).

Documentação para quem baixa:

- **`README.md`** (inglês, com resumo em português): aviso de que não há arquivo do jogo, tabela "How to play"
  (um comando por sistema), estado por plataforma, limites conhecidos, como reportar e créditos. Seção **"Project
  focus"** (e "Foco do projeto" no resumo): desempenho sem estressar o Deck e sem cortar efeitos (30 fps nativos a
  720p com as partículas; sobreposição de quadros, envio só das páginas alteradas, build Zen 2, shaders em segundo
  plano e em cache, degrau 1120×630; 60 fps opcional) e o **próximo interesse, dispositivos móveis**, dizendo que
  **não existe versão Android** nem aparelho do dono para testá-la (pede ajuda por issue). As notas da v0.2.0 no
  GitHub receberam o mesmo trecho.
- **`GUIDE.md`** (novo, inglês): o que é preciso (IDs de título, só `.nsp`), estrutura de `Game Files`, Windows,
  Steam Deck passo a passo para quem nunca usou o Modo Desktop, PC Linux (tabela de opções), Deck gerado no
  Windows pelo WSL, configurações/saves/logs, atualização, solução de problemas e como reportar.
- `AGENTS.md`: `GUIDE.md` em "Comece por aqui"; exceção de idioma (README, GUIDE e notas de release em inglês);
  "mudou um passo do build? mude nos dois caminhos e no `GUIDE.md`"; o foco e o interesse em móveis (nunca afirmar
  que há versão Android).
- Os documentos herdados do HWDER em `docs/` (`playing`, `releasing`, `windows-setup*`, `linux-*`, `antivirus`,
  `building`, `local-build`) ganharam um aviso de que são referência, não instrução para o FEW.
- `recomplauncher.json` aponta para `Jogar no PC/few.exe` e `FireEmblemWarriors/few-steam.sh`; `play-local.sh`
  virou atalho de `build-linux.sh --launch`; `.gitignore` ignora `FireEmblemWarriors/`; mensagens do
  `steam_shortcut.py` sem "HWDER".

Build no Linux:

- **`build-linux.sh`** (raiz) → `tools/linux/few_build.py`: build nativo no Linux e no próprio Deck, sem root.
  Usa o Python do sistema (só biblioteca padrão) e o `linux_setup.install_toolchain` do HWDER para o toolchain fixado
  (`tools/linux/toolchain.json`), em `~/.local/share/FEW-Recomp/HWDER/toolchain/<id>`; o `linux-deps` vem da
  tarball Linux do HWDER v0.3.5 (SHA-256 fixado no script) em `~/.local/share/FEW-Recomp/linux-deps`; imgui e os
  headers SPIR-V/Vulkan nos commits do `gerar-few.ps1` (git ou tarball do GitHub). Depois: `data/update`,
  `generated/`, `build/linux` (com `build.log`) e a pasta do jogo `FireEmblemWarriors/` (caminhos absolutos no
  `.ini`; no Deck, os padrões do Deck e `hwder_env.txt` com `HWDER_FRAME_OVERLAP=1`), atalho na Steam por
  `steam_shortcut.py`. No Deck detecta `board_vendor=Valve` e usa `-march=znver2`. Roda sob `systemd-inhibit`
  quando permitido. Recusa pastas em exFAT/FAT/NTFS (sem bit de execução).
- `gerar-deck.ps1` chama o WSL com `-u root` e acha o NSP base pelo título; o `build-deck-wsl.sh`, sem o toolchain
  em `/root/hwder`, chama `few_build.py --tools-only --tools-dir /root/few/tools`. Os textos da pasta do Deck
  (`LEIA-ME.txt`, `instalar-no-deck.sh`, `few-steam.sh`, `hwder_env.txt`) agora são em inglês.
- Teste de 2026-11-24: build completo do zero com o `build-linux.sh` numa distro Ubuntu 24.04 do WSL (root, sem
  nada instalado antes): ferramentas (~5 min, o zig vem devagar do ziglang.org), update, C e 495 passos de
  compilação com 10 jobs, **106 min** no total; `few` de 678 MB, só `libc`/`libm`/`ld-linux`, glibc máxima 2.35.
  Rodado 45 s com `HWDER_OFFSCREEN=1`: leu base, update 1.5.0, chaves e as 5 DLCs pelo `.ini`, 5 módulos, Vulkan
  (llvmpipe no WSL), janela aberta. **Falta**: o build no próprio Deck (tempo e memória com o Steam aberto no Modo
  Desktop) e o atalho da Steam criado pelo script num Deck/Linux real. A pasta de teste foi apagada.
- Também conferido: um clone limpo, sem `Game Files`, para no começo com mensagem clara.
- **Espaço em disco**: a pasta de teste e as ferramentas baixadas foram apagadas dentro dessa distro e rodado o
  `/sbin/fstrim`, mas o `ext4.vhdx` dele continua com ~27 GB no C: (~17 GB usados dentro). O modo esparso do WSL
  (`--set-sparse true`) não devolveu nada e foi desfeito. Só encolhe compactando com `diskpart` como
  administrador (`wsl --shutdown` antes); o dono recusou essa etapa em 2026-11-24: não fazer sem ele pedir.

## RecompLauncher: pacote de release e setup (2026-11-26, falta testar dentro do launcher)

Tudo em [docs/recomplauncher.md](recomplauncher.md): manifesto por sistema (`tools/recomplauncher/manifest.py`),
empacotador (`package.py`), setup `few-prepare.bat` → `prepare.ps1` → `gerar-few.ps1 -BaseNsp -UpdateNsp -Keys
-ToolsData` e `few-prepare.sh` → `prepare.py` → `few_build.py --base --update --keys --no-steam --tools-dir`.

- `gerar-few.ps1` ganhou `-BaseNsp`/`-UpdateNsp`/`-ToolsData` e baixa o Python embutível fixado quando não há Python
  (ou só o atalho da Microsoft Store). `few_build.py` ganhou `--base`/`--update`; sob `RECOMPLAUNCHER` não imprime a
  dica do `build-linux.log`. `build-linux.sh` não reabre num terminal sob `RECOMPLAUNCHER`.
- `.gitignore`: exceções para `few-prepare.bat` e `prepare.ps1` (os padrões `*.bat`/`*.ps1` os escondiam) e
  `/recompdata/`, `/dlc/`, `/.recomplauncher-tmp/`. No índice, `few-prepare.sh` precisa de modo 100755
  (`git update-index --chmod=+x`; o Windows não marca sozinho).
- **Teste real no Windows** (zip da release, NSPs falsos, sem Python no PATH): Python fixado baixado e conferido,
  toolchain privado instalado e aprovado nos imports, progresso por ferramenta, parada na extração do update com o
  motivo em inglês por último. A primeira tentativa achou o limite de 260 caracteres: com o toolchain em
  `{data}\cache\tools` sob uma pasta longa, `_cffi_backend` não carregava. Por isso o Windows usa `%LOCALAPPDATA%\HWDER`.
- **Teste real no Linux** (tarball no WSL archlinux, Python 3.14): bits de execução preservados, toolchain baixado em
  `{data}/cache/tools` com velocidade e tempo restante, mesma parada com o motivo por último.
- `tools/tests/test_recomplauncher.py` (7 testes, sem rede): manifesto, raiz em sincronia com o do Windows,
  mapeamento de progresso, `\r\n` partido entre leituras, motivo do update, "Already built".
- Falta: testar dentro do RecompLauncher (Windows e Deck), um build completo pelo setup com dump real, publicar a
  primeira release com os assets (v0.3.0, release normal) e pedir a entrada no catálogo (conta de desenvolvedor +
  /submit). AppImage não é publicado; conferir se o launcher aceita só o tarball no Linux.

## Issue #1: versão japonesa (2026-11-26, sem teste: não há dump japonês)

- `tools/game_profile.py REGIONS` (`US`, `JP` = `0100EAE003A5A000`) e `region_of`. Os scripts acham a região pela
  base presente em `Game Files` (`few_build.find_inputs`, `gerar-few.ps1`, `gerar-deck.ps1`) e recusam bases de duas
  regiões juntas.
- `game_update.unpack` escolhe a região pelo ticket do update e grava `region` no `update.json`; `main` imprime
  `update: code identical to the tested US update 1.5.0` ou `… differs … in <módulos>` (hash dos arquivos do ExeFS
  contra `supported_games.json`).
- `gen_main.update_region` lê `../update.json` ao lado do ExeFS e escreve `set(HW_GAME_REGION "JP")` no
  `generated/sources.cmake`. O `CMakeLists.txt` aplica `HW_GAME_REGION_JP` só ao alvo `hwder`, então trocar de
  região não recompila o código gerado. Chega também ao build do Deck pelo WSL, que copia `generated/`.
- `game_profile.h`: título, `HW_GAME_REGION` e idioma `ja` com `HW_GAME_REGION_JP`. O `set` `GetLanguageCode`
  agora responde `HW_GAME_LANGUAGE` (antes, `en-US` fixo; nos EUA não muda nada).
- A `extra_entries.txt` rastreada (uma entrada no `glslc`, biblioteca do SDK) vale para as duas regiões.
- Testado: detecção de região (Python e PowerShell), `update_region`, `region_of`, `misc.cpp` e `game_profile.h`
  compilados nas duas regiões. **Não testado:** dump japonês real, jogo rodando em japonês, vídeos `nx/movie/jpn`.
- Se o código japonês for diferente, `few_build.merge_found_entries`/`gerar-few.ps1` juntariam endereços descobertos
  nele à lista dos EUA; resolver só se isso acontecer.

**2026-11-27: o `main` japonês é outro build.** Pelas capturas do testador: `update: JP release`, `code differs …`,
os outros 4 módulos com as mesmas faixas de texto do US, `main` com 20585 funções (US: 20587) e texto até
`0x1000c06fe8` (US: `…c07540`). A geração parava em `symbols.txt: Display_SetMode 0x10008b7f1c is not a recompiled
function start`. Correção (sem dump japonês para testar):

- `tools/recomp/symbol_sigs.py` + `tools/recomp/symbols.sig`: para cada símbolo do `symbols.txt`, o tamanho da função
  que o contém, o deslocamento nela, CRC-32 das 4 primeiras instruções e SHA-256 do corpo com campos relativos ao PC
  e deslocamentos de ADRP mascarados (`remap_symbols.normalize`), e quantas vezes esse corpo aparece no módulo. Só
  hashes e tamanhos, nenhum byte do jogo (`test_signatures_hold_no_code`). Dado é achado pela n-ésima referência ADRP
  de uma função de corpo único, mais um deslocamento curto (`g_PresentInterval` = ref − 4, `g_FrameRate` = ref − 0xc).
- `gen_main` confere cada símbolo no endereço dos EUA (instantâneo) e, se o código não estiver lá, procura no módulo
  (~2 s no `main`) e imprime `symbols: <nome> <US> -> <novo> (same code, moved in this build)`. Não achou, ou o corpo
  aparece um número de vezes diferente do US: `symbols: WARNING: … left out`, e o gancho sai junto.
  `few_display_mode.cpp` só define cada gancho com `#ifdef HW_ADDR_<nome>`.
- `read_symbols` mudou do `gen_main` para o `symbol_sigs` (o teste roda sem `lz4`; `remap_symbols` importa o `lz4` só
  dentro de `image()`).
- Testado: `main` US gera a mesma saída de antes, byte a byte; `main` US com uma página inserida: 10 símbolos
  andaram +0x1000 e o que vinha antes ficou; função corrompida: gancho fora com aviso; `few_display_mode.cpp`
  compila com e sem os símbolos (g++ no WSL); `tools/tests/test_symbol_sigs.py` (6 testes, sintéticos).
- **Ao mudar o `symbols.txt`, regerar o `symbols.sig`** (comando no cabeçalho do `symbols.txt`); o teste
  `test_every_symbol_has_a_signature_at_its_address` falha se esquecer. O ExeFS US extraído está em `data/update` e a
  tabela do `main` US em `data/gen_us_main/functable.c` (ignorados). Python com `lz4`: um venv com `lz4` e
  `cryptography` basta; o Python do sistema não tem.
- Se o teste japonês mostrar outro problema de endereço, o próximo suspeito é `tools/recomp/extra_entries.txt`
  (uma entrada no `subsdk0`, que tem a mesma faixa de texto nas duas regiões).

## Issue #3: build no Steam Deck (2026-11-26, falta testar num Deck)

Relato: o download "não acaba" e no fim aparece algo como "fail updated"; pede um jeito de instalar sem Konsole.
Sem log do usuário; a mensagem provável era `Could not unpack the update`, a primeira etapa depois dos downloads.

- **`build-linux.sh`**: aberto por duplo clique (sem terminal, com `DISPLAY`/`WAYLAND_DISPLAY`), reabre em
  `konsole` (ou gnome-terminal, kgx, xfce4-terminal, x-terminal-emulator, xterm) com `FEW_IN_TERMINAL=1`; nesse
  modo espera Enter no fim, com sucesso ou erro. O `systemd-inhibit` virou a função `build` (antes era `exec`).
  O duplo clique precisa do bit de execução no git (`git update-index --chmod=+x build-linux.sh`, modo 100755).
- **`linux_setup.download`**: baixa em `<arquivo>.part` e continua com `Range` (servidor que responde 200 recomeça
  do zero; 416 apaga a parte). Continua também na execução seguinte, porque `downloads/` só é apagada no sucesso.
  Desiste após 6 tentativas seguidas sem progresso; a linha de progresso mostra MB/s e o tempo restante. Teste com
  servidor local: conexão caindo duas vezes, `.part` de execução anterior, servidor sem `Range`, hash errado.
- **`game_update.main`/`explain`**: o `gerar-few.ps1` e o `few_build.py` chamam `g.main` e mostram uma linha
  `update: <motivo>` em vez do traceback (NSP de outra região, `titlekek_XX` ou `header_key` faltando no
  `prod.keys`, NSP sem ticket, NSP danificado). `unpack` recusa versão diferente de `UPDATE_VERSION` antes de gravar.
- **`few_build.py`**: copia toda a saída para `build-linux.log` na raiz (as linhas de progresso com `\r` ficam
  fora). Não vale para `--tools-only` (WSL).
- O build continua levando horas no Deck; a saída mais rápida segue sendo o `gerar-deck.ps1` no Windows.

## Crash da issue #2: região de TLS esgotada (2026-11-26, falta validar no jogo)

Issue #2 ("Game crashed going to Camp menu", Windows, RTX 3080 Ti): o log termina em
`mem: commit failed at 0x4801001000 (+0x1000): error 487` e `fatal: commit`. O menu Camp foi só o momento.

- **Causa:** cada thread do guest pega um slot de TLS de 0x200 bytes e nenhum era devolvido. A região tem 16 MiB
  (`kTlsSize`), ou seja, 32.768 threads; o endereço da falha é o fim da região mais uma página. O log tinha 32.727
  quadros apresentados no crash, o que sugere **uma thread nova por quadro** (crash em ~9 min a 60 fps, ~18 min a
  30 fps). Só o `svcCreateThread` cria threads além da principal. Dedução pela contagem; não foi medido no jogo.
- **Correção:** `kernel.cpp reap_ended_threads` (chamada em `create_thread`) libera as threads que terminaram e
  cujo handle já foi fechado (`use_count() == 1` sob `g_threads_lock`): join da thread nativa, evento, `Ctx` e slot
  de TLS, que volta por `mem::free_tls_slot` (lista de livres em `memory.cpp`). Região esgotada mesmo assim dá
  `mem: TLS region full` e `fatal: too many guest threads`. `all_threads()` deixou de guardar todas as threads.
- **Mesmo problema nos handles:** `HandleTable::add` fazia `(next_++ << 15) | 0x10` em 32 bits e dava a volta
  depois de 131.072 handles, sobrescrevendo handles vivos (~36 min a uma thread por quadro). Agora o índice de 17
  bits volta a 1 e pula valores ainda abertos (`handles.cpp`).
- **Diagnóstico novo:** `kernel: N guest threads created, M kept` a cada 10.000 threads. Se `kept` crescer, o
  jogo não fecha os handles das threads e a correção não basta.
- **Testes:** `host_kernel_test` cria 40.000 threads curtas e exige o TLS reaproveitado (sem a correção falha em
  "TLS slot reused"); `runtime_unit_test` ganhou `test_handle_table_wrap`. `tools/tests/host.cmake` não linkava o
  `host_kernel_test` (faltava `host/sampler.cpp`). Rodados no Windows com o clang do MSYS2; no Linux, não.
- **Validar:** jogar mais de 10 min (ou 20 no Deck) e ver as linhas `kernel: … kept` no `hwder.log`.
- O `perf_stats` (só com `HWDER_PERF`) ainda guarda um registro por thread nativa; é diagnóstico.
- Vale também para o HWDER (mesmo código).

## Retomar aqui (2026-11-16)

**Build `8d32b02d…` enviado ao Deck em 2026-11-19 às ~21:50** (SHA-256 conferido): o de `465354ac…` mais o
descarte de quadros só com limitador detectado (ver "Jogatina de 19/11 à noite" em "Sessão de 2026-11-19"). **Falta
a partida de teste do dono** (passos 4 e 5): na de 19/11 ele não alternou o F1, então não há comparação 720p × 630p.

1. Conferir o SSH e que não há jogo aberto: `ssh deck@<ip do Deck> 'pgrep -a few'`
   (sem saída = livre). Se não responder, procurar o IP (DHCP) com `Get-NetNeighbor` no PowerShell.
2. Gerar com `.\gerar-deck.ps1` antes de enviar: o `few` de `Envio Deck` foi apagado em 2026-11-20 para liberar
   espaço (o do Deck é o `8d32b02d…`). Enviar com
   `ARQUIVOS="few few-steam.sh" bash tools/deck/enviar-para-deck.sh` (confere o SHA-256).
3. Ligar a opção no `.ini` do Deck sem tocar no resto (o `few-steam.sh` novo também acrescenta a chave se faltar):
   na pasta do jogo, `grep -q '^res_630p=' hwder_settings.ini || echo res_630p=1 >> hwder_settings.ini` (o
   arquivo pode terminar sem quebra de linha: confira com `tail -c 1`).
4. Pedir ao dono uma partida de teste: **limitador de fps da Steam desligado** (ele usava 30/40, o que antes
   deixava a batalha em câmera lenta), modo Desempenho (já escolhido no menu do jogo), e na batalha alternar F1 →
   "Lower 3D resolution (1120x630)" (~1 min ligada, ~1 min desligada).
5. Depois, ler pelo journal (`journalctl --user -S "<início>" -o short-iso | grep -F "[hwder]"`) as linhas
   `display: 300 frames` (fps, `limiter`, `N not shown`), `gpu thread busy` e `few: 3D resolution configuration`,
   comparando os trechos 1280×720 e 1120×630. Resumo por janela: `python tools/deck/resumo_journal.py journal.txt`.
   Esperado: `limiter` perto de 0 nos trechos pesados, `not shown` 0 sem o limitador da Steam (em 19/11 eram até
   65 de 300) e, com ele ligado, `(host frame limiter)` no fim da linha e a batalha na velocidade normal.

Objetivo do dono: batalha a 30 fps **estáveis no Steam Deck com as partículas ligadas** ("não quero comprometer o
jogo pela estabilidade"). Nada de desligar efeito (`HWDER_GMEM=0` não é solução).

Estado: com a sobreposição de quadros, a luta densa do `battle_long.txt` mede **29,8–30,0 fps** no Deck (em série,
24–29). A **tela preta nos ataques fortes** que o dono viu jogando foi corrigida (`SUST`, seção "Sessão de
2026-11-16") e conferida no PC; **falta o dono confirmar no Deck**, e dizer como ficou o jogo com a sobreposição
(ele só tinha jogado em série, ver abaixo).

**A Steam do Deck abria o `few` direto, não o `few-steam.sh`** (visto em `/proc/<pid>/environ` e na linha do
`reaper`): o dono jogava por uma entrada "few" que adicionou à mão no Modo Jogo, cujo seletor de arquivos não
mostra `.sh`. Em 2026-11-17 essa entrada saiu da Steam; sobrou só "Fire Emblem Warriors" (criada
pelo `instalar-no-deck.sh`, com arte), que abre o `few-steam.sh`. Para recriar: `sh instalar-no-deck.sh --yes` na
pasta do jogo (por SSH também; fecha a Steam, que no Modo Jogo reabre sozinha). A capa vinha do Hyrule Warriors
(endereço da loja da Nintendo herdado do HWDER); agora sai de `game_profile.STORE_ART`. Imagens já presentes em
`config/grid/<appid>*` são mantidas: para trocar, apague-as antes (as do HWDE estão em `~/few-arte-hwde` no Deck).
O `setup_ui` e o `linux_setup.py` (instaladores do HWDER, sem uso no FEW) ainda têm nome e arte do HWDE. As variáveis continuam em
**`hwder_env.txt`** na pasta do jogo, lido pelo próprio `few` (`core.cpp load_env_file`), então valem por qualquer
entrada. O do Deck tem `HWDER_FRAME_OVERLAP=1`; apagar a linha volta ao modo em série. Diagnósticos de uma sessão
jogada entram ali também.

**Como trabalhar com o dono em bugs que ele vê jogando**: ele joga, o agente arma a captura antes e lê depois; nada
de rodar testes longos no Deck sem pedir. Receita que achou a tela preta: `HWDER_FILMSTRIP=film`,
`HWDER_FILMSTRIP_BATTLE=0`, `HWDER_FILMSTRIP_FOR=150`, `HWDER_DRAWLOG=1` no `hwder_env.txt` (criar a pasta `film`);
depois, por SSH, achar os quadros escuros nas miniaturas (`f_<n>.raw`, RGBA 96×54) e comparar no `drawlog.txt` os
programas que só aparecem neles. Antes de pedir ao dono, tente reproduzir no PC com `-Ini "fps60=0;fps30_720p=1"`.

Passo a passo para medir de novo:

1. **Deck livre**: ligado, no Modo Jogo, sem jogo aberto (o teste se recusa a rodar com um jogo aberto). Conferir
   o SSH: `ssh deck@<ip do Deck> uptime`. O Deck suspende sozinho e some da rede.
2. **Build do Deck**: `.\gerar-deck.ps1` (incremental, ~3 min; não passe a saída por `Select-Object`: a tabela
   final quebra o pipeline e o script sai com erro depois de já ter gerado tudo) e
   `ARQUIVOS="few few-steam.sh" bash tools/deck/enviar-para-deck.sh` (o envio confere o SHA-256).
3. **Medir a batalha** (~5 min; save vazio até o prólogo e luta longa):
   `DECK_FRESH=1 DECK_PACE=1 DECK_INPUT=tools/input/battle_long.txt bash tools/deck/testar-no-deck.sh 230`.
   O teste liga a sobreposição como o lançador; `DECK_SERIAL=1` mede em série. As janelas de 145 a 230 s são a
   luta densa. As lutas variam entre execuções: rodar duas vezes e olhar a pior janela. Referências em
   `build/deck_pace_*.log` (`overlap2`, `overlap3`: sobreposição; `merge_serial1`: em série).
4. **Depois, se precisar de mais folga**: tempo de GPU na janela pesada (`DECK_ENV="HWDER_GPU_TIMING=5"`): passe de
   iluminação `e789f3db` (~7,7 ms), "other" (~6 ms, inclui a cópia de volta de ~30 MB na GPU), `a3e216c7`
   (~3,4 ms); e o custo por despacho na CPU (pendência 2).
5. **Fechar a sessão**: commitar e atualizar este arquivo (estado, medidas, pendências).

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

**Steam Deck**: o cartão SD tem o build de 2026-11-15 à noite (partículas ligadas, 30 fps a 720p, despachos
encadeados, cópia de volta de `gmem` por command buffer com fusão em três vias, sobreposição de quadros ligada pelo
`few-steam.sh`). Luta densa: 29,8–30,0 fps; à tarde, 26–28 com ~19–20 na janela mais pesada; de manhã, ~11 fps.
**No Deck o padrão é 30 fps** (`fps60=0`, modo Qualidade): decisão do dono, porque no FEW 30 fps é o ritmo nativo
da batalha no Switch (a lógica segue `g_FrameRate`), ao contrário do HWDE; 60 fps fica como opção no menu.

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
| Laço de quadros (jogo × thread da GPU) | `-Env "HWDER_TIMELINE=<s>"`: 0,5 s de eventos em `build\run\hwder.log.timeline.txt` (µs); `HWDER_TIMELINE=<s>,<duração>` grava mais tempo (até ~260 mil eventos) |
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
Deck depois. Commitado em partes (de `ed07277` em diante); o `few.exe` de `Jogar no PC` e o `few` de `Envio Deck`
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
  cartão com pasta `Games`). Por SSH, usuário `deck`; endereço e chave em `tools/deck/deck.local` (ignorado pelo git:
  `DECK_HOST=deck@<ip>`, `DECK_KEY=<chave>`; o IP vem por DHCP). Só o que mudou, retoma arquivo cortado, confere SHA-256 nos dois lados, mantém o `.ini`
  do Deck (`ENVIAR_INI=1` sobrescreve). O Deck precisa estar acordado; o SteamOS nega `systemd-inhibit` por SSH.
- **No Deck**: `/run/media/deck/<cartão>/Games/FireEmblemWarriors` (SD ext4, SteamOS
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
  `6081ff3`: as operações de warp agem dentro da metade de 32 lanes (`warp_lane`, `warp_ballot` em `program.cpp`).
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

Medido no Deck (luta densa do `battle_long.txt`): **~10,6 fps** com a correção de warp (`6081ff3`). O passe
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
   (Na sessão da noite o espelho virou memória própria da CPU e a comparação, por palavra: ver adiante.)
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
como se a GPU fosse mais lenta: reproduz no PC o ritmo do Deck; `HWDER_FRAME_OVERLAP=1` (seção "Sessão de
2026-11-15 (noite)"); eventos de espera do guest e da thread de espera na linha do tempo.

Ferramenta nova: `HWDER_DISPATCH_TRACE=<s>` loga um quadro inteiro de despachos e de uploads de `gmem` (linhas
`dtrace:`), a partir de <s> segundos depois do primeiro despacho. Cada despacho mostra programa, grade, faixas de
storage buffer lidas e escritas e imagens. A análise de dependências foi um script Python simples sobre essas
linhas, procurando sobreposição de faixas `w`/`r`.

## Sessão de 2026-11-15 (noite): sobreposição de quadros com a memória global inteira

Os dois problemas que seguravam a `HWDER_FRAME_OVERLAP` foram resolvidos ou explicados, e a opção passou a ser o
padrão do Deck (hoje pelo `hwder_env.txt`, ver "Retomar aqui"). No PC continua desligada por padrão.

Como funciona: o jogo só entrega o quadro N+1 depois que os syncpoints do quadro N disparam, e o runtime só os
dispara quando a GPU termina o quadro N (`on_gpu_idle`). Com a opção, eles disparam quando o quadro N está gravado
e enviado e o trabalho até o ponto de sincronização anterior terminou: a GPU desenha N enquanto o jogo e a thread
de comandos preparam N+1 (um quadro em voo).

### Cópia de volta de `gmem` refeita (`vk_resources.cpp`, `GmemBuf` em `vk_renderer.h`)

Antes, só o command buffer mais novo que escreveu um buffer copiava de volta (`pending != serial` pulava os
outros). Com quadros sobrepostos sempre há um mais novo: a memória do guest parava de receber as partículas.
Agora:

- **Um `readback` por command buffer** (`gmem_record_copybacks`, no envio): cada command buffer que escreveu o
  buffer leva a sua cópia na GPU para um `readback` próprio (reaproveitados, até 3 por buffer), e a thread de
  espera a funde na memória do guest quando ele conclui (`gmem_merge`), haja ou não outro mais novo.
- **Espelho na CPU** (`GmemBuf::mirror`): o conteúdo da cópia da GPU na última fusão, mais os uploads gravados
  depois. É o ancestral comum que separa o que o jogo escreveu do que a GPU escreveu. Começa zerado, como o buffer.
- **Fusão em três vias por palavra de 32 bits** (`gmem_merge`): a GPU não mudou a palavra → nada; a GPU mudou e o
  jogo não → vai para o guest; os dois mudaram → fica a do jogo (escrita depois de entregar aquele trabalho) e o
  espelho guarda a da GPU, então o bind seguinte vê a diferença e sobe a do jogo; palavra que subiu num command
  buffer mais novo que a cópia (`GmemBuf::uploads`) → a cópia é anterior ao upload, pulada. Páginas sem mudança
  não são escritas (não sujam o write watch).
- **Uploads com escrita pendente** (`gmem_upload_changed`): o bind compara com o espelho só as páginas que o write
  watch acusou (`TextureCache::written_since` agora devolve a lista) e sobe só as palavras diferentes. Com escrita
  da GPU em voo, palavra igual ao espelho não sobe (a da GPU pode ser mais nova); sem nada em voo, trechos
  vizinhos se juntam. O upload inteiro (`gmem_upload`) saiu: o primeiro bind compara tudo com o espelho zerado.
- Limite conhecido: se o jogo escrever numa palavra o mesmo valor que o espelho tem enquanto a GPU a muda em voo,
  a escrita do jogo não é vista (indetectável sem interceptar a escrita). No rastreio da batalha o jogo não
  escreve nesses buffers (0 bytes diferentes em 139 uploads).

No PC a batalha com a sobreposição mostra as partículas (poeira, faíscas, pedras: `build/shots/ov_*.png`).

### Travadas de ~300 ms: são do PC de teste, não da sobreposição

Linha do tempo de uma travada (`HWDER_TIMELINE=4,6`, com as marcas novas `present lock/locked/recorded/captured/
submitted` e `comp fence wait/presented/screen read`): o compositor fica parado em `wait_screen_read`, esperando o
blit do quadro **anterior** na fila de apresentação, que leva ~350–450 ms a cada ~1 s. Acontece igual **sem** a
sobreposição e antes do primeiro quadro do jogo (`display: slow: swapchain fence wait 350 ms` na abertura), e não
acontece no Deck (0 `slow frame` em 230 s). É a apresentação da janela neste PC (RTX 3050 Laptop + Intel), nesta
sessão; nos logs da manhã não aparecia. A causa do lado do Windows não foi investigada.

### Medições no Deck (noite)

Luta densa do `battle_long.txt`, 30 fps a 720p, janelas de 20 s, mesmo build:

| Modo | fps por janela (143–223 s) | Mediana | p95 | p99 |
|---|---|---|---|---|
| Em série (`DECK_SERIAL=1`) | 26,6 / 29,4 / 23,8 / 25,9 | 33,3–33,5 ms | 38–66 ms | 51–73 ms |
| Sobreposição (padrão do Deck) | 29,9 / 29,8 / 29,8 / 30,0 | 33,3 ms | 34–35 ms | 40–41 ms |

Em série o build novo não regrediu (era 26–28 com a pior janela em 19–20). Com a sobreposição, de 3 a 12 quadros
acima de 40 ms a cada 20 s e no máximo 1 acima de 100 ms.

Efeitos colaterais conhecidos da sobreposição: os dados das partículas que voltam para o guest (vértices e
argumentos de draw indireto) chegam um quadro mais tarde; nas primeiras aparições de um efeito alguns draws
indiretos ainda leem memória não escrita e são descartados pela rede de segurança (`3d: indirect macro … skipped`,
8 de 934 numa execução; em série também acontece).

## Sessão de 2026-11-19: jogatina de 18/11 à noite no Deck, ritmo de quadros

O dono jogou em 18/11, das 23:20 às 23:59 (duas execuções, journal guardado no PC só durante a sessão). Sem
`stall:`, sem submissão lenta nos quadros 600×n (a gravação do cache de pipelines fora da thread da GPU funcionou) e
o **F2 funcionou** (18 trocas). Ele passou ~30 min no modo 60 fps.

**Limitador de quadros da Steam deixava o jogo em câmera lenta.** De 23:29 em diante todo `present` esperava ~33 ms no
`vkAcquireNextImageKHR` (às vezes 25 ou 40 ms), também nos menus, depois de desligar o F2 e na segunda execução: era
o limitador do gamescope (menu de desempenho da Steam), não o jogo. Com `vsync=1` (FIFO), o `acquire` bloqueava o
compositor, e como o jogo avança a lógica por quadro liberado, a batalha a 60 fps rodou **na metade da velocidade**
por ~20 min (e a 2/3 com o limite em 40). As ~48 mil linhas `display: slow: swapchain fence wait` e os `present took
70–200 ms` (`acquire` estourando os 100 ms, perto dos toques no F2, provavelmente com o menu da Steam aberto) eram
isso. Correção (`display.cpp present_image`): o `acquire` espera no máximo até o próximo slot de liberação (menos
2 ms); passado isso o quadro **não é mostrado** e o buffer volta ao jogo na hora, que mantém a velocidade. O resumo de
300 quadros ganhou `N not shown, N acquires over 20 ms`, e a primeira vez loga `display: the host shows frames slower
than the game`. Aviso por linha só acima de 50 ms.

**`frame_limit` segurava o quadro seguinte a um atrasado** (código do HWDER v0.3.5). O teste de atraso comparava com
um período a partir do slot anterior (todo quadro de 17+ ms contava como "travada") e o reancoramento punha a grade
um passo à frente, então o quadro seguinte esperava até 33 ms depois do atrasado (66 ms no intervalo 2). Com o
limitador desligado (23:57–23:59) a batalha a 60 fps fez 44–57 fps, 20–50% dos quadros acima de 20 ms, com o
compositor **dormindo ~10 ms por quadro** mesmo abaixo de 60: sintoma disso. Agora o quadro é devido um passo depois
do slot anterior, atraso de até um período mantém a grade e só mais que isso reancora em `now`. Simulação com o
jogo preso à liberação (`jogo` = tempo do jogo por quadro):

| Jogo | Antes | Agora |
|---|---|---|
| 15–19 ms (modo 60) | 40,6 fps | 56,1 fps |
| 60% 12–16 ms, 40% 17–25 ms | 43,5 fps | 55,8 fps |
| 20–30 ms (modo 30, intervalo 2) | 21,6 fps | 30,0 fps |

No PC (`battle_long`, 60 fps a 720p, `build/pc_pacefix_pace.log`): 0 quadros descartados, 0 `acquire` acima de
20 ms, luta leve com 91,5% dos quadros até 20 ms. O fps por janela não serve de comparação: houve de novo as
travadas de 300–400 ms do PC de teste (o jogo fica ~400 ms sem entregar quadro, sem esperar buffer nem `acquire`),
~6 a cada 20 s. **Enviado ao Deck** (`5f7caf51…`, SHA-256 conferido); **falta medir no Deck**: o resumo de 300
quadros da batalha a 60 fps deve mostrar `limiter` perto de 0 nos trechos pesados e menos quadros acima de 20 ms.

**Opção "Lower 3D resolution (1120x630)"** (`res_630p`, `settings.h res_630p`, pedido do dono, que prefere
desempenho a qualidade). O gancho `DynRes_Update` prendia cada grupo da resolução dinâmica no maior degrau; com a
opção, o grupo do Desempenho (ids 3–4) fica no degrau 4, 1120×630 (~23% menos pixels no 3D; o HUD continua em
1280×720). Vale no modo Desempenho e no "720p at 30 fps", na hora (o jogo chama a função todo quadro); loga
`few: 3D resolution configuration 4 (1120x630)`. **Ligada por padrão no Deck** (`.ini` do `gerar-deck.ps1`,
`few-steam.sh`, `testar-no-deck.sh`). No PC (`build/pc_r630_pace.log`, `build/shots/r630b_*.png`): imagem inteira,
HUD nítido, luta densa ~59 fps. O tamanho dos alvos não foi conferido direto (vem da tabela do jogo em `few.md`).
O dono jogava com o modo Desempenho escolhido no menu do próprio jogo (log: `few: display mode 1` sem conversão),
então o F2 não mudava nada na batalha. Build `465354ac…` gerado; **não enviado** (o Deck suspendeu): enviar com
`ARQUIVOS="few few-steam.sh"` e acrescentar `res_630p=1` ao `hwder_settings.ini` do Deck.

**Jogatina de 19/11 à noite (20:41–20:51, build `465354ac…`, journal em `build/journal_1119b.txt`).** Batalha no
modo 60 fps a 1120×630 o tempo todo (ele não alternou o F1), limitador da Steam desligado (`0 acquires over 20
ms` em todas as janelas): a câmera lenta acabou e o `limiter` caiu para 0–5 ms nos trechos pesados, então o
`frame_limit` corrigido funciona. Luta leve a 58–60 fps; densa a 38–48 (`gpu thread busy` 10–14 ms, 100–230
despachos por quadro), 25–64% dos quadros acima de 20 ms. Mas **o jogo descartava quadros prontos sem limitador**
(até 65 de 300, junto com os quadros atrasados): o prazo do `acquire` era o slot seguinte ao do quadro, e um quadro
atrasado 10–16 ms ficava com 1–3 ms, menos que a espera normal do gamescope (até um vblank). Correção
(`display.cpp present_image`): o descarte só vale com o **limitador do host detectado** (com a GPU já em dia com os
presents anteriores, um `acquire` acima de 20 ms ou estourado soma 3, um abaixo de 2 ms tira 1; liga em 15, desliga
em 0) e, nesse modo, o prazo mínimo é 4 ms; fora dele o `acquire` espera até 100 ms como antes. Loga a entrada e a
saída do modo, e a linha de 300 quadros termina com `(host frame limiter)` enquanto ele vale. Sem teste com o
limitador ainda. Também 786 `vk: slow: wait_serial took 30–60 ms`: são da thread que aposenta os command buffers
(`waiter_thread`), esperando a GPU com a sobreposição de quadros; não seguram o jogo.

Outros pontos do log: `nvdrv` `0xc0400001` (submit de canal, vídeo) leva ~15 ms nas cutscenes em vídeo; ~1 draw por
quadro cai num buffer já apresentado (`vk: N draws into already-presented buffers`), não investigado.

## Sessão de 2026-11-18 (tarde): engasgos periódicos na batalha (Deck)

O dono jogou de novo (18/11, 17:51–18:03) e o travamento da cutscene **não se repetiu** (nenhuma linha `stall:`).
Batalha travada em 30 fps (33,3 ms de média), menus e cutscenes a 60. Sobravam engasgos: um quadro de 150–190 ms a
cada 10–20 s, e 0,75 s e 1,6 s uma vez cada. Todos vinham de `gpu: submission on syncpt 1 took 120–170 ms` mesmo
com 11–25 entradas, e caíam **exatamente nos quadros múltiplos de 600** (`vk: frame 601, 1201, …`), quando
`save_pipeline_cache()` roda: se algum pipeline novo apareceu desde a última vez, ele lia o cache inteiro do driver
(`vkGetPipelineCacheData`) e gravava no cartão SD **na thread da GPU**. Os múltiplos de 600 sem pipeline novo não
engasgavam.

Correção (`vk_renderer.h save_pipeline_cache`): leitura e gravação numa thread à parte (uma por vez), num
`pipeline_cache.bin.tmp` trocado no fim (fechar o jogo no meio não deixa arquivo pela metade). No PC
(`HWDER_CACHE_PROFILE=1`): leitura 16–34 ms + disco 18–57 ms por gravação, agora fora da thread da GPU; nenhuma
submissão lenta nos quadros 600×n. Enviado ao Deck (SHA-256 conferido); **falta o dono jogar e conferir** que os
`submission on syncpt 1 took` nos quadros 600×n sumiram.

**F2: batalhas a 60 fps na hora** (`overlay::toggle_battle_60fps`, pedido do dono): alterna `fps60`, salva o `.ini`,
loga `few: 60 fps battles on/off (F2)` e mostra um aviso de 2,5 s no canto. Vale na hora porque o jogo chama
`Display_SetMode` todo quadro e o gancho lê a opção a cada chamada (o texto do menu dizia "no próximo início"). No
Deck os dois modos renderizam a 1280×720 (`fps30_720p`), então a troca é só de ritmo. F1 já era o menu. O Deck não
tem teclado: mapear um botão traseiro para F2 no layout da Steam. **Não testado com a tecla** (o teste sem
supervisão não aperta teclas): só compilado e aberto no PC; falta o dono apertar e conferir.

## Sessão de 2026-11-18: travamento em tela preta numa cutscene (Deck)

Sintoma (dono, sessão de 17/11 das 21:51 às 23:36): ao chegar numa cutscene o jogo ficou numa tela preta e não
voltou; ele fechou depois de ~1 min. O `hwder.log` do Deck é sobrescrito a cada execução, mas **o journal guarda
tudo** (a Steam joga a saída do jogo nele):
`journalctl --user -S "<início>" -U "<fim>" -o short-iso | grep -F "[hwder]"` por SSH. Os intervalos longos no meio
da sessão (32, 27 e 9 min) terminam em `vk: slow: present took 840 ms`: é o Deck suspenso e retomado.

O que o log mostra:

- **Travamento final (23:35:40)**: batalha normal até ali (30 fps), uma leitura de RomFS às 23:35:44 e depois
  **nada** até `window closed` às 23:36:36. Nenhuma linha `hang:` nem `vi: frame fence … not reached`: o compositor
  não tinha quadro na fila, então o jogo parou de entregar quadros por conta própria. Ou uma thread do jogo esperava
  algo que não veio, ou o jogo esperava a GPU ficar ociosa (comum na troca de cena) com a thread da GPU presa. O
  vigia antigo (`gpu::dump_hang_state`) só rodava a partir da espera do compositor, então o log não diz qual.
  **Causa ainda não achada.**
- **Parada de 9,5 s às 21:54:40, logo antes de outra cutscene (vídeo)**: a thread da GPU passou 1,1 s, 1,1 s e 7,2 s
  em três despachos de cálculo; o número de shaders subiu de 190 para 195 nesse trecho, com 5 despachos lentos. É a
  compilação de shaders de cálculo novos (as partículas de ~12 mil instruções) no ACO, feita na thread da GPU antes
  de gravar o despacho. Nessa sessão o `pipeline_warmup.bin` era incompatível (`kVersion` 5 do `SUST`) e tudo foi
  compilado de novo. Da segunda vez em diante o cache de pipelines (`pipeline_cache.bin`, gravado ao fechar; cresceu
de 3,6 para 4,4 MB nessa sessão) e o cache em disco do Mesa devem evitar a espera (não medido).

Feito nesta sessão (diagnóstico, para a próxima vez que acontecer):

- **Relatório de travamento** (`runtime/src/stall_watch.cpp`, sempre ligado, `HWDER_STALL_DUMP=<s>`, padrão 5, `0`
  desliga): se o jogo passa 5 s sem entregar quadro (QueueBuffer), loga linhas `stall:` com o estado da GPU
  (`hang:`), a fila do vi e o que o compositor faz, cada thread do jogo (no que está parada: `mutex`, `condvar`,
  `address` ou o tipo do objeto do WaitSynchronization, com endereço/handle e há quanto tempo) e a pilha do host de
  todas as threads do jogo e do runtime (GPU, gravação, espera, compositor). De novo aos 30 s; `stall: frames
  resumed` quando volta.
- **Pilha de outra thread** (`runtime/src/host/stack.cpp`): no Linux um sinal (`SIGRTMIN+6`, por `tgkill`) para a
  thread, que roda `backtrace()` no tratador e espera ali enquanto a pilha é varrida; no Windows,
  `SuspendThread` + `RtlVirtualUnwind` (o código que era do `HWDER_THREAD_DUMP`). Conferido num teste isolado no
  WSL com o toolchain do Deck (thread bloqueada, thread girando, thread que já saiu). Funções do jogo aparecem como
  `f_<endereço>`; quadros sem nome exportado como `few+0x<offset>`, para `addr2line -f -e
  /root/few/build-deck/few 0x<offset>` no WSL (o mesmo build que foi para o Deck).
- `HWDER_THREAD_DUMP` usa o mesmo despejo e agora funciona no Linux (antes só listava "started/exited").
- Compilação de programa de cálculo marcada (`hang:` mostra `building a compute program`) e medida:
  `vk: slow: compute program <chave> took N ms (pipeline creation N ms; …)` acima de 50 ms.

Próximo passo: o dono joga de novo até a mesma cutscene (ou até travar); depois, ler as linhas `stall:` no journal
ou no `hwder.log` e corrigir pela pilha. Se o travamento for a thread da GPU compilando, a saída é compilar os
programas de cálculo fora dela (como os gráficos, de forma assíncrona) ou pré-aquecer pelo `pipeline_warmup.bin`.

## Sessão de 2026-11-16: tela preta nos ataques fortes (`SUST`)

Sintoma (dono, no Deck): ao usar habilidades a tela pisca preta e volta. Nas miniaturas (`HWDER_FILMSTRIP`): a
cena 3D sai preta por ~7 quadros com o HUD normal, a cada ataque forte (X). Reproduz no PC com 30 fps a 720p
(34 vezes em 2 minutos) e no modo Desempenho (60 fps a 720p); **não** no Qualidade a 1080p. Com ou sem sobreposição.

Causa: nesses quadros um passo de tela cheia (programa `f3a332122ff3ad54`) mistura a cena com uma imagem 640×360
que nenhum draw escreve. Quem a escreve é o shader de cálculo `d17e7155f43e7e87` (média de 2×2 texels da cena) com
`SUST`, escrita em imagem, que o tradutor tratava como no-op: a imagem ficava preta.

Correção: `SUST` no tradutor (`emit_mem.cpp`, `Translator::image2d` em `program.cpp`): forma com handle fixo
(bit 51, índice nos bits 36–48 ×4 no buffer de handles de textura), dados RGBA em Rd..Rd+3 (bit 23), coordenadas
inteiras em Ra, tipo 2D (bits 33–35 = 3). Imagem de armazenamento sem formato no tipo
(`StorageImageWriteWithoutFormat`, recurso `shaderStorageImageWriteWithoutFormat` pedido em `display.cpp`); o
renderer já ligava `Resources::images`. Outras formas logam `SUST form unsupported`. `kVersion` do
`pipeline_warmup.bin` foi para 5. Conferido no PC: 0 quadros pretos em ~65 s de luta com ~30 ataques fortes.
O `TEX` com AOFFI não constante do mesmo shader continua (os quatro toques do filtro usam deslocamento vindo de
registrador; o `spirv-val` reclama, as GPUs aceitam).

Também nesta sessão:

- **Fusão de `gmem` só entre submissões** (`gmem_apply_merges`, chamada por `Renderer::begin_submission` e por
  `gmem_sync`): a thread de espera só enfileira as cópias de volta concluídas; a thread da GPU as funde na memória
  do guest no começo de cada submissão do jogo. Antes a fusão podia cair no meio da gravação de um quadro (contagem
  de um quadro, vértices de outro). Foi feita como hipótese para a tela preta, que não era isso; fica porque é o
  comportamento certo. Custo: a comparação (~4 ms no Deck) passou para a thread da GPU. **Não medido de novo no
  Deck**: se a luta densa cair dos 30 fps, é o primeiro suspeito.
- `hwder_env.txt` (acima) e `HWDER_TIMELINE=<s>,<duração>`.

## Pendências, por prioridade

1. **Batalha densa a 30 fps no Deck com as partículas**: medida em 29,8–30,0 fps com a sobreposição. Falta o dono
   jogar e confirmar (visual das partículas no Deck, cutscenes, vídeos, menus com a sobreposição). Folga a buscar,
   se precisar: tempo de GPU do passe de iluminação `e789f3db` (~7,7 ms a 720p no Deck, contra ~1 ms a 1080p no
   PC), "other" (~6 ms), `a3e216c7` (~3,4 ms).

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
   gráficos (não conferida); `SUST` fora da forma 2D com handle fixo, `AL2P` + `IPA` indexado, `TEX` com AOFFI não
   constante (o único programa que falha no `spirv-val`). Todos logam uma vez com `[shader]`.
5. **Escala de resolução > 1x**: travada em 1x (`texture_cache.cpp res_scale_allowed`). Em 2x a composição cobre
   só o quadrante superior esquerdo. Diagnóstico provável, não confirmado: passes (compute, ou fetches a partir de
   `gl_FragCoord`) que endereçam alvos escalados em texels do guest. Confirmar com `HWDER_VK_FRAME_DUMP` em 2x
   (`HWDER_ALLOW_RES_SCALE=1`).
6. **Engasgos de compilação** (seção "Engasgos") e o 120 fps do HWDE (`HW_GAME_HOOKS`: achar no FEW as funções
   equivalentes do quadro do KTGL; os ganchos novos mostram o caminho com `gxref.py`).
7. **Nomes do save**: o jogo grava `system` e `scenario0` em `save/0100F15003E64000/<usuário>/`. Ajustar
   `HW_GAME_SAVE_MAIN/SIDE` em `game_profile.h` (só afetam o seletor de save do menu F1).
8. **Steam Deck: medição** como a do HWDER (a medição dele fica fora deste repositório).
9. **Compilar sem as NSPs no PC** (tarefa futura, pedida pelo dono para quando o jogo estiver estável e otimizado:
   ele quer arquivar as NSPs na nuvem e liberar ~20 GB do PC). Hoje o `gerar-few.ps1` exige a NSP da base e a do
   update em `Game Files` mesmo com `-SoBuild` (seção "entradas" e o `nsp_path` do `.ini`), e o `gerar-deck.ps1`
   exige a base porque refaz os hardlinks de `Envio Deck`. Plano: com `-SoBuild`, compilar sem NSP (o C está em
   `generated\`, o update extraído em `data\update`) e manter o `.ini` de `Jogar no PC` como está; no
   `gerar-deck.ps1`, sem NSP, montar só o `few` e os scripts, e enviar com `ARQUIVOS="few few-steam.sh"` (o Deck
   já tem NSPs, chaves e update no cartão). Avisar claramente que sem as NSPs não há como jogar no PC, rodar o
   `run_test.ps1` (testes passam a ser no Deck, pedindo antes) nem gerar o C do zero (precisa da NSP do update).
   Lembrar o dono: base e DLCs são hardlinks em `Game Files` e `Envio Deck`; o espaço só volta apagando as duas
   pastas. Extrair o RomFS para pasta não economiza (o conteúdo não é comprimido na NSP); a única economia de
   conteúdo é dispensar a DLC "Japanese-Voice Pack" (2,8 GB) se ele joga com vozes em inglês.
10. **`build-linux.sh` num Deck/Linux real** (seção "Release pública"): medir tempo e memória do build no próprio
    Deck e conferir o atalho da Steam criado pelo script. Precisa do dono.
11. **Dispositivos móveis** (próximo interesse do dono, 2026-11-24): não há versão Android nem aparelho para testar.
    Qualquer trabalho nessa frente depende de testadores; até lá, só documentar e não afirmar que existe.

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
- Ritmo no PC: se o log tiver `display: slow: swapchain fence wait`, a apresentação da janela está travando
  sozinha (~350 ms a cada ~1 s, com ou sem jogo) e os `slow frame` não dizem nada sobre o runtime. Meça no Deck.
- `wsl.exe -- bash -c '...'` passa pela shell padrão e perde aspas e variáveis: escreva um script e rode
  `wsl -d archlinux -u root -- bash /mnt/c/.../script.sh` (com `MSYS_NO_PATHCONV=1` no Git Bash).
- O ExeFS desempacotado fica em `data/update/exefs` (o `tools/gaddr.py` antigo ainda aponta para `data/exefs`).
