<#
.SYNOPSIS
  Gera o executavel nativo de Fire Emblem Warriors (few.exe) a partir dos arquivos em "Game Files".

.DESCRIPTION
  1. confere as arvores de third_party (baixa nos commits fixados se faltarem);
  2. acha o toolchain privado do HWDER (Python, LLVM-MinGW, CMake, Ninja) ou instala um igual;
  3. desempacota o update 1.5.0 em data\update (ExeFS inteiro + patch do RomFS), so quando mudou;
  4. traduz o ARM64 do ExeFS do update para C em generated\ (incremental: so regrava arquivo que mudou);
  5. compila em build\pc (Ninja retoma o que ja foi feito);
  6. publica em ".\Jogar no PC" com o hwder_settings.ini apontando para base, update e chaves.
  NSP e chaves sao lidos onde estao; nada deles entra no git nem e copiado.

.EXAMPLE
  .\gerar-few.ps1            # tudo
  .\gerar-few.ps1 -Launch    # tudo e abre o jogo
  .\gerar-few.ps1 -SoBuild   # pula update e geracao do C (so recompila o runtime)
#>
[CmdletBinding()]
param(
    [string]$Jogo,                                                   # padrao: .\Game Files
    [string]$Keys,
    [int]$Jobs = [Math]::Min(8, [Environment]::ProcessorCount),
    [switch]$SoBuild,
    [switch]$Launch
)

$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
if (-not $Jogo) { $Jogo = Join-Path $root 'Game Files' }  # Windows PowerShell 5.1: $PSScriptRoot is empty in param()
[Console]::OutputEncoding = [Text.Encoding]::UTF8
$env:PYTHONIOENCODING = 'utf-8'
function Step($m) { Write-Host "`n==> $m" -ForegroundColor Cyan }
function Fail($m) { Write-Host "ERRO: $m" -ForegroundColor Red; exit 1 }

# --- entradas ---------------------------------------------------------------------------------------------
Step 'Entradas'
$all = @(Get-ChildItem -LiteralPath $Jogo -Filter *.nsp -ErrorAction SilentlyContinue)
$base = @($all | Where-Object { $_.Name -match '\[0100F15003E64000\]' })
$upd = @($all | Where-Object { $_.Name -match '\[0100F15003E64800\]' })
if ($base.Count -ne 1) { Fail "Esperava 1 NSP da base [0100F15003E64000] em $Jogo (achei $($base.Count))." }
if ($upd.Count -ne 1) { Fail "Esperava 1 NSP do update [0100F15003E64800] em $Jogo (achei $($upd.Count))." }
if (-not $Keys) {
    $Keys = @((Join-Path $Jogo 'prod.keys'), (Join-Path $root 'prod.keys')) | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
}
if (-not $Keys) { Fail 'prod.keys nao encontrado (em "Game Files" ou na raiz). Passe -Keys.' }
Write-Host "base  : $($base[0].Name)"
Write-Host "update: $($upd[0].Name)"
Write-Host "keys  : $Keys"

# --- third_party ------------------------------------------------------------------------------------------
Step 'third_party'
$deps = @(
    @('SDL',            'https://github.com/libsdl-org/SDL.git',               'f5e5f6588921eed3d7d048ce43d9eb1ff0da0ffc', 'CMakeLists.txt'),
    @('imgui',          'https://github.com/ocornut/imgui.git',               'ed73ef4e84c9488256c5710de6ff1ebc2c9a8496', 'imgui.cpp'),
    @('spirv-headers',  'https://github.com/KhronosGroup/SPIRV-Headers.git',  '86f980c731e62ae4eaf383d320449d71687936bf', 'include\spirv\unified1\spirv.hpp'),
    @('vulkan-headers', 'https://github.com/KhronosGroup/Vulkan-Headers.git', 'c46850864f4661461b0f6cb9922c058ffea4915e', 'include\vulkan\vulkan.h')
)
foreach ($d in $deps) {
    $dir = Join-Path $root "third_party\$($d[0])"
    if (Test-Path -LiteralPath (Join-Path $dir $d[3])) { Write-Host "$($d[0]): ok"; continue }
    Write-Host "$($d[0]): baixando commit $($d[2].Substring(0,12))"
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
    git -C $dir init -q; git -C $dir remote add origin $d[1]
    git -C $dir fetch -q --depth 1 origin $d[2]; if ($LASTEXITCODE) { Fail "git fetch $($d[0])" }
    git -C $dir checkout -q FETCH_HEAD;          if ($LASTEXITCODE) { Fail "git checkout $($d[0])" }
}

# --- toolchain privado (o mesmo do HWDER; certificado por tools\managed_tools.py) ----------------------------
Step 'Toolchain privado'
$roots = @((Join-Path $env:LOCALAPPDATA 'HWDER'))
$roots += @(Get-ChildItem (Join-Path $env:LOCALAPPDATA 'Packages') -Directory -Filter 'Claude_*' -ErrorAction SilentlyContinue |
            ForEach-Object { Join-Path $_.FullName 'LocalCache\Local\HWDER' })
$managed = $null
foreach ($r in $roots) {
    $c = @(Get-ChildItem (Join-Path $r 'managed-tools') -Directory -ErrorAction SilentlyContinue |
           Where-Object { Test-Path (Join-Path $_.FullName 'complete.json') })
    if ($c.Count) { $managed = $c[0].FullName; break }
}
if (-not $managed) {
    $sys = (Get-Command python -ErrorAction SilentlyContinue).Source
    if (-not $sys) { Fail 'Sem toolchain privado e sem Python no sistema para instala-lo.' }
    & $sys -X utf8 -u (Join-Path $root 'tools\managed_tools.py') --data $roots[0] --install
    if ($LASTEXITCODE) { Fail 'Instalacao do toolchain falhou.' }
    $managed = (Get-ChildItem (Join-Path $roots[0] 'managed-tools') -Directory | Where-Object { Test-Path (Join-Path $_.FullName 'complete.json') } | Select-Object -First 1).FullName
}
$py = Join-Path $managed 'output\python\python.exe'
Write-Host "toolchain: $managed"

$data = Join-Path $root 'data'
$gen = Join-Path $root 'generated'
$build = Join-Path $root 'build\pc'
if (-not $SoBuild) {
    # --- update ------------------------------------------------------------------------------------------
    Step 'Update 1.5.0 (ExeFS + patch do RomFS)'
    $stamp = Join-Path $data 'update\source.txt'
    $sig = "$($upd[0].FullName)|$($upd[0].Length)|$($upd[0].LastWriteTimeUtc.Ticks)"
    if ((Test-Path $stamp) -and ((Get-Content $stamp -Raw).Trim() -eq $sig) -and (Test-Path (Join-Path $data 'update\update.json'))) {
        Write-Host 'ja desempacotado'
    } else {
        & $py -I -B -X utf8 -c "import sys; sys.path.insert(0, sys.argv[1]); import game_update as g; g.unpack(*sys.argv[2:5])" `
            (Join-Path $root 'tools') $upd[0].FullName $Keys (Join-Path $data 'update')
        if ($LASTEXITCODE) { Fail 'Falha ao desempacotar o update.' }
        Set-Content -LiteralPath $stamp -Value $sig -Encoding utf8
    }

    # --- entradas descobertas em execucao ------------------------------------------------------------------
    # O runtime grava em "Jogar no PC\extra_entries.txt" cada endereco para o qual o jogo saltou sem ele ter sido
    # reconhecido como funcao; elas entram na lista rastreada (sao enderecos, nao dados do jogo).
    $found = Join-Path $root 'Jogar no PC\extra_entries.txt'
    $tracked = Join-Path $root 'tools\recomp\extra_entries.txt'
    if (Test-Path -LiteralPath $found) {
        $old = @(Get-Content -LiteralPath $tracked | Where-Object { $_.Trim() } | ForEach-Object { $_.Trim().ToLower() })
        $new = @(Get-Content -LiteralPath $found | Where-Object { $_.Trim() } | ForEach-Object { $_.Trim().ToLower() } | Where-Object { $old -notcontains $_ })
        if ($new.Count) {
            $all = @($old + $new | Sort-Object -Unique)
            [IO.File]::WriteAllText($tracked, (($all -join "`n") + "`n"), (New-Object System.Text.UTF8Encoding($false)))
            Write-Host "entradas novas: $($new -join ' ')"
        }
        Remove-Item -LiteralPath $found
    }

    # --- ARM64 -> C --------------------------------------------------------------------------------------
    Step 'Traduzindo ARM64 -> C (generated\)'
    & $py -B -X utf8 -c "import sys, runpy; sys.path.insert(0, sys.argv[1]); sys.argv = ['gen_main', '--exefs', sys.argv[2], '--out', sys.argv[3]]; runpy.run_module('recomp.gen_main', run_name='__main__')" `
        (Join-Path $root 'tools') (Join-Path $data 'update\exefs') $gen
    if ($LASTEXITCODE) { Fail 'Geracao do C falhou.' }
}

# --- compilar ---------------------------------------------------------------------------------------------
Step "Compilando (build\pc, $Jobs jobs)"
& $py -B -X utf8 (Join-Path $root 'tools\build.py') --managed-toolchain $managed --build-dir $build --generated-dir $gen --jobs $Jobs
if ($LASTEXITCODE) { Fail 'Compilacao falhou (rodar de novo retoma).' }
$exe = Join-Path $build 'few.exe'
if (-not (Test-Path $exe)) { Fail "Nao achei $exe" }

# --- publicar ---------------------------------------------------------------------------------------------
$out = Join-Path $root 'Jogar no PC'
Step "Publicando em $out"
New-Item -ItemType Directory -Force -Path $out | Out-Null
$dst = Join-Path $out 'few.exe'
if (Get-Process -Name few -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $dst }) { Fail 'O jogo esta aberto; feche e rode de novo.' }
$tmp = Join-Path $out ('.few-' + [guid]::NewGuid().ToString('N') + '.tmp')
Copy-Item -LiteralPath $exe -Destination $tmp
Move-Item -LiteralPath $tmp -Destination $dst -Force

$ini = Join-Path $out 'hwder_settings.ini'
$utf8 = New-Object System.Text.UTF8Encoding($false)
$values = [ordered]@{ nsp_path = $base[0].FullName; keys_path = (Resolve-Path -LiteralPath $Keys).ProviderPath; update_path = (Join-Path $data 'update') }
$lines = if (Test-Path -LiteralPath $ini) { @([IO.File]::ReadAllText($ini, $utf8) -split "\r?\n" | Where-Object { $_ -ne '' }) } else { @('version=2', 'face_labels=1') }
$seen = @{}
$o = foreach ($l in $lines) { $k = ($l -split '=', 2)[0]; if ($values.Contains($k)) { if (-not $seen[$k]) { $seen[$k] = 1; "$k=$($values[$k])" } } else { $l } }
$o = @($o) + @($values.Keys | Where-Object { -not $seen[$_] } | ForEach-Object { "$_=$($values[$_])" })
[IO.File]::WriteAllText($ini, (($o -join "`n") + "`n"), $utf8)

Write-Host "`nPronto: $dst" -ForegroundColor Green
if ($Launch) { Start-Process -FilePath $dst -WorkingDirectory $out }
