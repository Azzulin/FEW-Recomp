# Publicar uma versão

Uma versão nova sai pelo GitHub Actions: você escreve o que mudou, cria a tag e confere o rascunho. Nenhuma ferramenta
local é necessária, e nada do jogo entra nos arquivos (só código-fonte e o `FEW Recomp Setup.exe`, que não tem
código do jogo).

## Passo a passo

1. **CHANGELOG.md:** troque `## Unreleased` por `## FEW Recomp vX.Y.Z (AAAA-MM-DD)`, em inglês, para os jogadores.
   Essa seção vira as notas da release (`tools/release_notes.py`); sem ela, a release para com um aviso.
2. **Commit e push** na `main` (a CI roda os testes; veja a aba Actions).
3. **Tag:** `git tag vX.Y.Z` e `git push origin vX.Y.Z`. Pelo site também dá: *Releases > Draft a new release > Choose a
   tag*, digite a versão e *Create new tag* (mas aí o GitHub já cria um rascunho próprio; prefira a tag pelo git).
4. **Espere o workflow "Release"** (aba Actions, ~15 min; a primeira vez baixa as ferramentas, depois vêm do cache).
   Ele roda os testes, compila o `FEW Recomp Setup.exe`, monta o zip do Windows e o tarball do Linux, e cria uma
   release **em rascunho** com os dois, o `SHA256SUMS` e as notas.
5. **Confira o rascunho** (*Releases*): as notas, os três arquivos. Baixe o zip, abra o Setup e veja se ele abre. Então
   **Publish release**. Deixe *Set as a pre-release* desmarcado: o RecompLauncher trata pré-release como beta.
   Publicar cria a tag, e a tag dispara o workflow mais uma vez: essa execução curta só vê que a release existe e para.
6. **Depois:** abra de novo um `## Unreleased` no CHANGELOG para a próxima versão, e responda as issues que a versão
   resolve.

Errou algo? Apague o rascunho e a tag (`git push origin :refs/tags/vX.Y.Z` e `git tag -d vX.Y.Z`), corrija e repita.

## Ensaio sem publicar

*Actions > Release > Run workflow*, com uma versão qualquer (ex. `v0.0.0-test`): faz tudo, mas em vez da release
guarda os arquivos como *artifact* da execução, para baixar e testar.

## O que o workflow faz (`.github/workflows/release.yml`)

| Passo | Comando |
|---|---|
| Testes | `python -m unittest tools/tests/test_*.py` (os que não precisam do jogo) |
| Notas | `python tools/release_notes.py vX.Y.Z` |
| Ferramentas fixadas | `python tools/managed_tools.py --data <pasta> --install` (LLVM-MinGW, CMake, Ninja, Python; SHA-256 conferido) |
| Bibliotecas | `python tools/fetch_deps.py` (SDL, imgui e headers nos commits do `gerar-few.ps1`) |
| Setup | `tools/build.py --managed-toolchain <toolchain> --build-ui` → `build/ui/few-build-ui.exe` |
| Pacotes | `python tools/recomplauncher/package.py --version vX.Y.Z --setup-exe build/ui/few-build-ui.exe` |
| Release | `gh release create vX.Y.Z --draft ...` |

Os mesmos comandos funcionam na sua máquina (PowerShell, na pasta de um clone limpo); o `package.py` recusa um
checkout com mudanças não commitadas. O zip do Windows leva o setup como `FEW Recomp Setup.exe` na raiz da pasta.

## O Setup e o antivírus

O `FEW Recomp Setup.exe` não é assinado, então o SmartScreen pode pedir *Mais informações > Executar assim mesmo*.
Para não parecer malware (o setup do HWDER foi marcado como `Wacatac`, ver [antivirus.md](antivirus.md)), ele tem
ícone, manifesto (sem pedir administrador) e informações de versão, é ligado sem data no binário (o mesmo código dá o
mesmo arquivo, que mantém a reputação já ganha) e abre o PowerShell **minimizado e visível**, nunca escondido. Mantenha
essas regras ao mexer em `tools/build_ui/`.

## Checklist rápido

- [ ] `CHANGELOG.md` com a seção `## FEW Recomp vX.Y.Z (data)`
- [ ] push na `main`, CI verde
- [ ] tag `vX.Y.Z` enviada
- [ ] rascunho conferido (notas, zip, tarball, SHA256SUMS) e publicado, sem marcar pré-release
- [ ] `## Unreleased` de volta no CHANGELOG
