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
       LEIA-ME.txt             instruções para o jogador (em inglês)
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
        & wsl.exe -d $Distro -u root --exec env "FEW_JOBS=$Jobs" "FEW_MARCH=$March" bash (WinToWsl (Join-Path $root 'tools\deck\build-deck-wsl.sh')) (WinToWsl $root)
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
    & wsl.exe -d $Distro -u root --exec sh -c "set -e; test -f '$WslBuild/few'; cp -f '$WslBuild/few' '$dst/few'; sha256sum '$WslBuild/few'"
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
# Identifica pelo título entre colchetes (como o gerar-few.ps1), não pelas etiquetas [Base]/[UPD] do nome.
# Versão dos EUA ou japonesa (issue #1); o update vai desempacotado em update\, então só a base e as DLCs entram.
$nsps = @(Get-ChildItem -LiteralPath $games -Filter *.nsp | Where-Object { $_.Name -notmatch '\[(0100F15003E64800|0100EAE003A5A800)\]' })
if (-not ($nsps | Where-Object Name -match '\[(0100F15003E64000|0100EAE003A5A000)\]')) { Fail "NSP base [0100F15003E64000] ou [0100EAE003A5A000] não encontrado em $games" }
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
# Catálogo de pipelines (tools\pipeline_catalog): gráficos preparados antes do primeiro quadro. Só metadados.
$catalog = Join-Path $root 'tools\pipeline_catalog\few-us-1.5.0.bin'
if (Test-Path -LiteralPath $catalog) { Copy-Item -LiteralPath $catalog -Destination "$out\pipeline_catalog.bin" -Force }

# 3. ajustes do Deck: caminhos relativos à pasta do jogo (o few entra nela ao iniciar).
#    A escala de resolução fica em 1x neste jogo (texture_cache.cpp), então não entra aqui.
WriteLF "$out\hwder_settings.ini" @'
version=2
face_labels=1
update_path=update
save_dir=save
async_shaders=1
show_fps=1
fps60=0
fps60_1080p=0
fps30_720p=1
res_630p=1
'@

WriteLF "$out\few-steam.sh" @'
#!/bin/sh
# Launcher for the "Fire Emblem Warriors" Steam shortcut (written by gerar-deck.ps1).
# Battles in the game's Quality mode (30 fps, FEW's native pace on the Switch) with the 3D at 1280x720, the height of
# the Deck's screen ("720p at 30 fps" in the F1 menu). 60 fps: "60 fps battles", at 720p ("1080p at 60 fps" off).
# A settings file from an older build lacks some keys: the missing ones get the Deck's defaults; after the first
# save, whatever is in the menu applies.
cd "$(dirname -- "$0")" || exit 1
if [ -f hwder_settings.ini ]; then
    for kv in fps60=0 fps60_1080p=0 fps30_720p=1 res_630p=1; do
        grep -q "^${kv%%=*}=" hwder_settings.ini && continue
        [ -n "$(tail -c 1 hwder_settings.ini)" ] && echo >> hwder_settings.ini  # the file may end without a newline
        echo "$kv" >> hwder_settings.ini
    done
fi
# The game's variables (frame overlap etc.) are in hwder_env.txt, which few reads itself: they also apply when Steam
# starts few directly, without this launcher.
exec ./few "$@"
'@

WriteLF "$out\hwder_env.txt" @'
# Variables for the game (NAME=VALUE), read by few itself when it starts. A variable set in the environment wins.
# Frame overlap: the game prepares the next frame while the GPU draws the current one; holds 30 fps in dense fights
# with the GPU particles (Deck, 2026-11-15: 29.8 fps against 24-29 without it). Delete the line to turn it off.
HWDER_FRAME_OVERLAP=1
'@

WriteLF "$out\instalar-no-deck.sh" @'
#!/bin/sh
# Fire Emblem Warriors on the Steam Deck - run ONCE in Desktop Mode (Konsole, or F4 in Dolphin), inside this folder:
#     sh instalar-no-deck.sh
# Makes the game executable and adds "Fire Emblem Warriors" to Steam (Steam closes and reopens).
# If you move the folder later, run it again to update the shortcut.
set -eu
DIR=$(cd "$(dirname "$0")" && pwd)
cd "$DIR"
[ -f few ] || { echo "ERROR: $DIR/few is missing"; exit 1; }
ls ./*.nsp >/dev/null 2>&1 || echo "WARNING: no .nsp in this folder."
[ -f prod.keys ] || echo "WARNING: prod.keys is missing from this folder."
[ -f update/romfs.patch ] || echo "WARNING: the update folder is missing (the game needs update 1.5.0)."
chmod +x few few-steam.sh
[ -x few ] || { echo "ERROR: this drive cannot hold executable files (exFAT/FAT?). Use internal storage or an ext4 card."; exit 1; }
mkdir -p save
echo "game folder: $DIR"
python3 - "$DIR/few" "$@" <<'PY'
import sys
from pathlib import Path
exe = Path(sys.argv[1])
sys.path.insert(0, str(exe.parent / "tools"))
import steam_shortcut
ok = steam_shortcut.add_to_steam(exe, assume_yes=True if "--yes" in sys.argv[2:] else None)
sys.exit(0 if ok else 1)
PY
echo "Done. Open 'Fire Emblem Warriors' in your Steam library (Non-Steam tab)."
'@

WriteLF "$out\LEIA-ME.txt" @'
Fire Emblem Warriors for the Steam Deck (static recompilation, built on a PC from your own copy)
================================================================================================

Self-contained folder: "few" finds the game (.nsp of the base game and DLCs), the keys, the update (update folder)
and the saves (save folder) inside its own folder. No fixed paths; you can move the folder.

First time: Desktop Mode > Dolphin > this folder > F4 (terminal here) >  sh instalar-no-deck.sh
The game then shows up in your Steam library, Non-Steam tab: "Fire Emblem Warriors".
Gaming Mode's "Add a Non-Steam Game" does not list few-steam.sh (it is a script): use the installer above, which
also works over SSH or in Konsole with  sh instalar-no-deck.sh --yes  (Steam closes and reopens by itself).

- Built for the Deck's processor (Zen 2, -march=znver2): it does not run on CPUs without the same instructions.
- Battles run at 30 fps, the game's own pace on the Switch (Quality mode), with the 3D at 1280x720 (the Deck's
  screen has 800 lines; "720p at 30 fps" in the F1 menu goes back to 1920x1080); menus at 60. F1 menu:
  "60 fps battles" turns on Performance mode (60 fps at 1280x720; "1080p at 60 fps" is too heavy for the Deck).
- The first battle of each session can pause ~2 s at the start while compute shaders compile.
- The first minutes stutter while shaders compile; the caches (pipeline_cache.bin, pipeline_warmup.bin) stay here
  and the stutter goes down with use.
- F1 (or holding View) opens the runtime menu. F2 toggles 60 fps battles on the fly (on the Deck: map a back
  button to F2 in Steam's controller layout). Log: hwder.log in this folder.
- Full guide: GUIDE.md in the source folder.
'@

Write-Host "`nPasta pronta em $out" -ForegroundColor Green
Get-ChildItem -LiteralPath $out -Recurse -File | Select-Object @{n = 'Arquivo'; e = { $_.FullName.Substring($out.Length + 1) } }, @{n = 'MB'; e = { [math]::Round($_.Length / 1MB, 1) } } | Format-Table -AutoSize
