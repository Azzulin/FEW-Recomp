<#
.SYNOPSIS
  Fire Emblem Warriors para o Steam Deck: build Linux para o Zen 2 do Deck e pasta pronta para o cartão SD.

.DESCRIPTION
  1. Compila no WSL (archlinux) com tools/deck/build-deck-wsl.sh: o C já gerado em generated\ (rode gerar-few.ps1
     antes), o toolchain Zig/Clang do HWDER (glibc 2.35, roda no SteamOS) e -march=znver2.
  2. Monta ".\Envio Deck\FireEmblemWarriors", uma pasta sem nenhum caminho fixo:
       few                     o jogo (ao iniciar, entra na própria pasta: ajustes, saves e caches ficam aqui)
       *.nsp                   base e DLCs (hardlinks: não ocupam espaço no PC; o jogo usa o .nsp de nome menor,
                               que é o da base, e as DLCs da mesma pasta)
       prod.keys, title.keys   hardlinks das chaves
       update\                 o update 1.5.0 desempacotado (hardlinks de data\update)
       hwder_settings.ini      perfil do Deck (caminhos relativos)
       few-steam.sh            lançador que a Steam usa
       instalar-no-deck.sh     rode uma vez no Deck: marca como executável e cria o atalho na Steam
       tools\                  steam_shortcut.py + game_profile.py (usados pelo instalador)
  Enviar: bash tools/deck/enviar-para-deck.sh (SSH, retoma e confere SHA-256).

.EXAMPLE
  .\gerar-deck.ps1             # build no WSL + monta a pasta
  .\gerar-deck.ps1 -SoMontar   # só remonta a pasta com o último build do WSL
  .\gerar-deck.ps1 -March ""   # AVX2 genérico (outros PCs Linux), em vez do Zen 2
  .\gerar-deck.ps1 -SoDados    # só os dados (NSPs, chaves, update, scripts), sem o few: para ir enviando
                               # os ~19 GB enquanto o WSL compila
#>
[CmdletBinding()]
param(
    [switch]$SoMontar,
    [switch]$SoDados,
    [int]$Jobs = 10,  # cada compilação do C gerado ocupa até ~0,7 GB; o WSL tem 10 GB
    [string]$Distro = 'archlinux',
    [string]$March = 'znver2',
    [string]$WslBuild = '/root/few/build-deck'
)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$out = Join-Path $root 'Envio Deck\FireEmblemWarriors'
$utf8 = New-Object Text.UTF8Encoding($false)
function Fail($m) { Write-Host "ERRO: $m" -ForegroundColor Red; exit 1 }
function WinToWsl([string]$p) { '/mnt/' + $p.Substring(0, 1).ToLower() + ($p.Substring(2) -replace '\\', '/') }
function WriteLF([string]$path, [string]$text) { [IO.File]::WriteAllText($path, ($text -replace "`r`n", "`n"), $utf8) }

# 1. build no WSL
if (-not $SoMontar -and -not $SoDados) {
    Write-Host "==> Build Linux no WSL ($Distro, -march=$March, $Jobs jobs)" -ForegroundColor Cyan
    # ~1,5 h: sem isto o Windows entra em espera com a tela apagada e o WSL para junto (2026-11-13).
    Add-Type -Namespace Few -Name Power -MemberDefinition '[DllImport("kernel32.dll")] public static extern uint SetThreadExecutionState(uint f);'
    [void][Few.Power]::SetThreadExecutionState([uint32]2147483651)  # ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED
    try {
        & wsl.exe -d $Distro --exec env "FEW_JOBS=$Jobs" "FEW_MARCH=$March" bash (WinToWsl (Join-Path $root 'tools\deck\build-deck-wsl.sh')) (WinToWsl $root)
    } finally {
        [void][Few.Power]::SetThreadExecutionState([uint32]2147483648)
    }
    if ($LASTEXITCODE) { Fail 'build no WSL falhou (veja a saída acima)' }
}

# 2. pasta de envio
Write-Host "==> Montando $out" -ForegroundColor Cyan
New-Item -ItemType Directory -Force -Path $out, "$out\tools", "$out\save", "$out\update\exefs" | Out-Null
$dst = WinToWsl $out
if (-not $SoDados) {
    & wsl.exe -d $Distro --exec sh -c "set -e; test -f '$WslBuild/few'; cp -f '$WslBuild/few' '$dst/few'; sha256sum '$WslBuild/few'"
    if ($LASTEXITCODE) { Fail "não achei o build Linux em $WslBuild (rode sem -SoMontar)" }
}

# Hardlinks: New-Item -Target trata [ ] do nome como curinga; a API do Windows usa o caminho literal.
Add-Type -Namespace Few -Name Fs -MemberDefinition '[DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)] public static extern bool CreateHardLink(string link, string target, IntPtr sa);'
function Link([string]$link, [string]$target) {
    if (-not (Test-Path -LiteralPath $target)) { return }
    if (Test-Path -LiteralPath $link) { Remove-Item -LiteralPath $link -Force }
    if (-not [Few.Fs]::CreateHardLink($link, $target, [IntPtr]::Zero)) {
        Fail ("hardlink de {0} falhou (erro {1})" -f $link, [Runtime.InteropServices.Marshal]::GetLastWin32Error())
    }
}
$games = Join-Path $root 'Game Files'
$nsps = @(Get-ChildItem -LiteralPath $games -Filter *.nsp | Where-Object { $_.Name -notmatch '\[UPD\]' })
if (-not ($nsps | Where-Object Name -match '\[Base\]')) { Fail "NSP base não encontrado em $games" }
Get-ChildItem -LiteralPath $out -Filter *.nsp | ForEach-Object { Remove-Item -LiteralPath $_.FullName -Force }
foreach ($n in $nsps) { Link (Join-Path $out $n.Name) $n.FullName }
foreach ($k in 'prod.keys', 'title.keys') {
    $src = @((Join-Path $root $k), (Join-Path $games $k)) | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
    if ($src) { Link (Join-Path $out $k) $src } elseif ($k -eq 'prod.keys') { Fail 'prod.keys não encontrado' }
}
$upd = Join-Path $root 'data\update'
if (-not (Test-Path -LiteralPath (Join-Path $upd 'romfs.patch'))) { Fail 'falta data\update (rode gerar-few.ps1 antes)' }
Get-ChildItem -LiteralPath $upd -Recurse -File | ForEach-Object {
    $rel = $_.FullName.Substring($upd.Length + 1)
    New-Item -ItemType Directory -Force -Path (Split-Path (Join-Path "$out\update" $rel)) | Out-Null
    Link (Join-Path "$out\update" $rel) $_.FullName
}
Copy-Item -LiteralPath (Join-Path $root 'tools\steam_shortcut.py'), (Join-Path $root 'tools\game_profile.py') -Destination "$out\tools" -Force

# 3. ajustes do Deck: caminhos relativos à pasta do jogo (o few entra nela ao iniciar).
#    A escala de resolução fica em 1x neste jogo (texture_cache.cpp), então não entra aqui.
WriteLF "$out\hwder_settings.ini" @'
version=2
face_labels=1
update_path=update
save_dir=save
async_shaders=1
show_fps=1
'@

WriteLF "$out\few-steam.sh" @'
#!/bin/sh
# Lançador do atalho "Fire Emblem Warriors" na Steam (escrito por gerar-deck.ps1).
cd "$(dirname -- "$0")" || exit 1
exec ./few "$@"
'@

WriteLF "$out\instalar-no-deck.sh" @'
#!/bin/sh
# Fire Emblem Warriors no Steam Deck - rode UMA vez no Modo Desktop (Konsole), dentro desta pasta:
#     sh instalar-no-deck.sh
# Marca o jogo como executável e adiciona "Fire Emblem Warriors" à Steam (fecha e reabre a Steam).
# Se mover a pasta depois, rode de novo para atualizar o atalho.
set -eu
DIR=$(cd "$(dirname "$0")" && pwd)
cd "$DIR"
[ -f few ] || { echo "ERRO: falta $DIR/few"; exit 1; }
ls ./*.nsp >/dev/null 2>&1 || echo "AVISO: nenhum .nsp nesta pasta."
[ -f prod.keys ] || echo "AVISO: falta prod.keys nesta pasta."
[ -f update/romfs.patch ] || echo "AVISO: falta a pasta update (o jogo precisa do update 1.5.0)."
chmod +x few few-steam.sh
[ -x few ] || { echo "ERRO: o cartão não aceita arquivos executáveis (exFAT/FAT?). Use um cartão ext4."; exit 1; }
mkdir -p save
echo "pasta do jogo: $DIR"
python3 - "$DIR/few" "$@" <<'PY'
import sys
from pathlib import Path
exe = Path(sys.argv[1])
sys.path.insert(0, str(exe.parent / "tools"))
import steam_shortcut
ok = steam_shortcut.add_to_steam(exe, assume_yes=True if "--yes" in sys.argv[2:] else None)
sys.exit(0 if ok else 1)
PY
echo "Pronto. Abra 'Fire Emblem Warriors' na biblioteca da Steam (aba Não Steam)."
'@

WriteLF "$out\LEIA-ME.txt" @'
Fire Emblem Warriors para Steam Deck (recompilação estática, compilada no PC a partir da sua própria cópia)
=========================================================================================================

Pasta autossuficiente: o "few" acha o jogo (.nsp da base e das DLCs), as chaves, o update (pasta update) e os
saves (pasta save) dentro da própria pasta. Nenhum caminho fixo; pode mover a pasta.

Primeira vez: Modo Desktop > Dolphin > esta pasta > F4 (terminal aqui) >  sh instalar-no-deck.sh
O jogo aparece na biblioteca da Steam, aba Não Steam: "Fire Emblem Warriors".

- Compilado para o processador do Deck (Zen 2, -march=znver2): não roda em CPUs sem as mesmas instruções.
- A batalha roda a 30 fps por escolha do próprio jogo. Não force 30 fps na Steam nos menus (60 fps).
- Os primeiros minutos engasgam compilando shaders; os caches (pipeline_cache.bin, pipeline_warmup.bin) ficam
  aqui e o engasgo diminui com o uso.
- F1 (ou segurar View) abre o menu do runtime. Log: hwder.log nesta pasta.
'@

Write-Host "`nPasta pronta em $out" -ForegroundColor Green
Get-ChildItem -LiteralPath $out -Recurse -File | Select-Object @{n = 'Arquivo'; e = { $_.FullName.Substring($out.Length + 1) } }, @{n = 'MB'; e = { [math]::Round($_.Length / 1MB, 1) } } | Format-Table -AutoSize
