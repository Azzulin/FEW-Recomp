# RecompLauncher: pacote de release e setup

[RecompLauncher](https://recomplauncher.com) instala recomps a partir dos assets das releases do GitHub, roda um
passo de setup próprio do jogo (`prepare`) e inicia o jogo. Referência do formato:
[guia do SDK](https://recomplauncher.com/sdk-files/guide.md) e
[schema](https://recomplauncher.com/schema/recomplauncher.json). O launcher não recebe nada do jogo: o setup compila
o jogo na máquina do jogador, a partir dos arquivos dele, como `gerar-few.ps1` e `build-linux.sh`.

## O que há no repositório

| Arquivo | Papel |
|---|---|
| `tools/recomplauncher/manifest.py` | Gera o `recomplauncher.json` de cada sistema (fonte única). `--write-root` reescreve o da raiz (layout do Windows), usado só pelas ferramentas de desenvolvedor do launcher ("Add a build" de uma pasta local) |
| `tools/recomplauncher/package.py` | Monta os assets da release a partir de um checkout git limpo |
| `few-prepare.bat` → `tools/recomplauncher/prepare.ps1` | Setup no Windows: chama `gerar-few.ps1 -BaseNsp -UpdateNsp -Keys -ToolsData` |
| `few-prepare.sh` → `tools/recomplauncher/prepare.py` | Setup no Linux e no Deck: chama `tools/linux/few_build.py --base --update --keys --no-steam --tools-dir` |

O manifesto é um por sistema porque o launcher tenta `prepare.program` em ordem: um `.bat` listado num manifesto do
Linux rodaria pelo Proton.

## Como o jogo roda pelo launcher

- **Entradas** (`inputs`): NSP da base (`game`), NSP do update 1.5.0 (`update`), até 5 DLCs (`dlc1`…`dlc5`, opcionais,
  `into: "dlc"`) e o `prod.keys` compartilhado (`switch.prod-keys`). O launcher não tem verificação de conteúdo para
  NSP (as dele são para discos, `.nup` e pastas `exefs`): quem confere é o próprio setup (`game_update.py` diz por que
  um update não serve) e o jogo (a base). Sem `check`, não há selo "Verified Copies".
- **Setup** (`prepare`): recebe `--game=… --update=… --keys=…`, imprime `progress <0-1> <texto>` (passos do build e
  `[n/m]` do ninja), termina com 0 ou com o motivo na última linha (em inglês; os `ERRO:` do `gerar-few.ps1` são
  repetidos depois de uma linha em inglês, e a linha `update:` do `game_update.py` fica por último). Antes de
  compilar compara `RELEASE` + caminho/tamanho/data do update com `few-build.json` na pasta do jogo: igual, imprime
  `progress 1 Already built`. O carimbo é apagado antes do build e escrito só no fim.
- **Ferramentas**: no Linux, em `{data}/cache/tools` (`RECOMPLAUNCHER_DATA`), apagadas ao desinstalar o jogo no
  launcher. No Windows, em `%LOCALAPPDATA%\HWDER`, o padrão do `gerar-few.ps1` (compartilhado com builds à mão),
  **não** em `{data}`: o toolchain cria arquivos até ~190 caracteres abaixo da pasta dele, e com a pasta de jogo do
  launcher o caminho passa de 260 e o Windows não carrega a DLL (medido: `_cffi_backend`, "nome do arquivo muito
  grande"). Fica para trás ao desinstalar (~1 GB). `FEW_TOOLS_DATA` muda o lugar (testes). O `gerar-few.ps1` baixa o
  Python embutível fixado em `tools/managed_toolchain.json` (SHA-256 conferido) e com ele roda o
  `managed_tools.py --install`; o jogador não precisa de Python nem de git (o pacote traz o `third_party`). Erro de
  caminho longo, em qualquer idioma, termina com uma linha em inglês dizendo para usar uma pasta mais curta.
- **Jogo** (`start`): `Jogar no PC/few.exe` ou `FireEmblemWarriors/few-steam.sh`, com `HWDER_NSP`, `HWDER_KEYS`,
  `HWDER_UPDATE`, `HWDER_AOC_DIR={install}/dlc` e `HWDER_NO_PRELAUNCH=1`. Nada depende de caminho absoluto no
  `hwder_settings.ini`, então mover a pasta do jogo não quebra nada. A base é lida durante o jogo (RomFS), por isso o
  launcher continua pedindo a cópia (`start` usa `{input:game}`); sem `keeps_copy`.
- **Saves e logs**: `folders.saves`/`saves` = `<pasta do jogo>/save`; `folders.logs` = a pasta do jogo (`hwder.log`).

## Publicar uma release

1. Num checkout git limpo e com a tag criada (`git tag v0.3.0`):

   ```
   python tools/recomplauncher/package.py --version v0.3.0
   ```

   Gera em `dist/` (ignorado): `FEW-Recomp-v0.3.0-Windows-x64.zip` (com SDL, imgui, SPIRV-Headers e Vulkan-Headers
   nos commits do `gerar-few.ps1`) e `FEW-Recomp-v0.3.0-Linux-x64.tar.gz` (sem SDL: o Linux usa o `linux-deps` do
   HWDER). Cada um tem uma pasta `FEW-Recomp-v0.3.0/` com os arquivos rastreados pelo git (lidos do índice, com LF; os
   `.bat` com CRLF), o `RELEASE` e o manifesto do sistema. Recusa checkout sujo (`--allow-dirty` só para teste) e
   qualquer caminho de jogo, chave, `generated/`, `data/` ou `build/`.
2. Criar a release no GitHub com os dois arquivos como assets, **como release normal** (pré-release vira beta no
   launcher e some para os jogadores comuns). Não use "source" no nome de um asset do jogo (o launcher ignora).
3. Cada release nova faz o launcher recompilar o jogo de quem já tem ("Finish the Update"); não crie tag só para
   documentação.

## Entrar no catálogo

1. Criar a conta de desenvolvedor gratuita (<https://recomplauncher.com/developers?signup=1>; aprovação manual).
2. Enviar o repositório em <https://recomplauncher.com/submit?platform=pc> e marcar os recursos.
3. Testar antes como jogador: no launcher, Settings → Developers → Developer tools → *Add a build* apontando para a
   pasta extraída de um dos assets; a aba Developer mostra erros do manifesto. Depois, setup com os próprios arquivos.
   **A seção Developers só aparece para conta de desenvolvedor verificado ou para quem já tem uma build local**
   (`devAllowed` no `ui/app.js` do launcher, 0.90 Beta 69). Sem conta aprovada, use o modo de teste do launcher
   (abaixo).

### Testar pelo modo de teste do launcher (sem conta de desenvolvedor)

O launcher tem um servidor MCP para agentes (`docs/TEST-CONTROL.md` no repositório `elripalda/RecompLauncher`):

1. Fechar o launcher (também o ícone perto do relógio) e abri-lo com `RECOMPLAUNCHER_TEST_CONTROL` = um token de
   16+ caracteres. Aparece "Test control on" no canto e o `launcher-trail.log` diz `MCP at http://127.0.0.1:47811/mcp`.
2. Falar com ele por MCP sobre HTTP (`Authorization: Bearer <token>`): `launcher_command` com `dev_add {path}`
   adiciona a build (a janela só a vê depois de `ui_eval` repetir o `dev_add` pelo `invoke` dela e pôr o resultado em
   `S.library`); `dev_check {id}` é a aba Developer; `setup_input_set {id, input, path}` escolhe cada arquivo.
3. Depois disso a seção Developers aparece também para o dono, porque já existe uma build local.
4. Para desligar: fechar o launcher e abri-lo normalmente.

## Testado e pendente (2026-11-27)

- Manifestos dos dois sistemas validados contra o schema oficial; conteúdo dos pacotes conferido (modos, CRLF/LF,
  `third_party`, nada do jogo).
- `prepare.py` e `prepare.ps1` testados com um build falso: progresso, "Already built", falha com motivo por último,
  arquivo faltando, caminhos com espaço (o `.bat` chamado direto, como o launcher faz).
- Windows de ponta a ponta a partir do zip, sem Python no PATH: ver "Teste real" no handover.
- **Windows dentro do RecompLauncher, de ponta a ponta (2026-11-27, 0.90 Beta 69, pacote `v0.3.0-rc1` de `b8f1c61`,
  dump US + 5 DLCs):** a página mostra a Configuração fácil em duas etapas (Jogo: base, update, `prod.keys`;
  Add-ons: 5 DLCs). Setup: ferramentas baixadas, tradução para C em ~3 min, 717 compilações, ~50 min no total
  (notebook com 12 threads), "Your files are set up" e "Pronto para jogar". Jogar abriu o jogo com o update, as 5
  DLCs (`aoc: ListAddOnContent -> 5`) e o modo de 60 fps; menus a 16,67 ms por quadro. As DLCs vão para `{install}/dlc`
  como hard links (sem ocupar espaço a mais no mesmo disco). `dev_check` antes do setup marca em vermelho
  `None of "start.program" is in this folder` (o `few.exe` só existe depois do setup, mesmo com `prepare.done`); depois
  do setup, tudo OK.
- Os textos do manifesto (rótulos das entradas, do setup) aparecem em inglês no launcher em português: o formato não
  tem tradução por idioma.
- **Não testado:** dentro do RecompLauncher no Deck (adiado: o Deck do dono não tem o launcher). O pacote do Linux
  só foi testado fora do launcher (WSL, ver handover).
- **AppImage:** o guia pede, no Linux, um tarball e um AppImage da mesma pasta; publicamos só o tarball (o nome tem
  `Linux-x64`, que é como o launcher escolhe o arquivo). Falta confirmar no launcher que só o tarball basta.
