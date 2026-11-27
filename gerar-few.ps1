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
  .\gerar-few.ps1 -BaseNsp D:\base.nsp -UpdateNsp D:\upd.nsp -Keys D:\prod.keys -ToolsData D:\cache
                             # arquivos por caminho, com qualquer nome (o setup do RecompLauncher usa assim)
  .\gerar-few.ps1 -NoUi      # sem a janela de progresso (tools\build_ui), so este terminal
#>
[CmdletBinding()]
param(
    [string]$Jogo,                                                   # padrao: .\Game Files
    [string]$Keys,
    [string]$BaseNsp,                                                # com -UpdateNsp: dispensa -Jogo e os nomes
    [string]$UpdateNsp,
    [string]$ToolsData,                                              # pasta do toolchain privado (padrao: %LOCALAPPDATA%\HWDER)
    [int]$Jobs = [Math]::Min(8, [Environment]::ProcessorCount),
    [switch]$SoBuild,
    [switch]$Launch,
    [switch]$NoUi                                                    # sem a janela de progresso
)

$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
if (-not $Jogo) { $Jogo = Join-Path $root 'Game Files' }  # Windows PowerShell 5.1: $PSScriptRoot is empty in param()
[Console]::OutputEncoding = [Text.Encoding]::UTF8
$env:PYTHONIOENCODING = 'utf-8'

# Janela de progresso (tools\build_ui): le os eventos que este script e o tools\build.py gravam em
# build\build-events.jsonl (formato em tools\build_events.py; os ids dos passos sao os de la). Compilada em segundo
# plano enquanto o update e desempacotado e o C gerado; aberta antes de compilar. Fechar a janela nao para nada.
$events = $null
$env:FEW_EVENTS = $null
if (-not $NoUi -and -not $env:RECOMPLAUNCHER -and [Environment]::UserInteractive) {
    $events = Join-Path $root 'build\build-events.jsonl'
    New-Item -ItemType Directory -Force -Path (Split-Path $events) | Out-Null
    [IO.File]::WriteAllText($events, '')
    $env:FEW_EVENTS = $events
}
function Send-BuildEvent([hashtable]$e) {
    if (-not $script:events) { return }
    $e['t'] = [Math]::Round([DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds() / 1000.0, 3)
    $json = ConvertTo-Json -InputObject $e -Compress -Depth 4
    try { [IO.File]::AppendAllText($script:events, $json + "`n", (New-Object System.Text.UTF8Encoding($false))) } catch {}
}
Send-BuildEvent @{ type = 'start'; title = 'Fire Emblem Warriors'; log = ''; steps = @(
    @{ id = 'check'; label = 'Checking your files' }, @{ id = 'tools'; label = 'Getting the build tools' },
    @{ id = 'update'; label = 'Unpacking update 1.5.0' }, @{ id = 'translate'; label = "Translating the game's code to C" },
    @{ id = 'compile'; label = 'Compiling the game' }, @{ id = 'finish'; label = 'Finishing the game folder' }) }
function Step($m, $id) {
    Write-Host "`n==> $m" -ForegroundColor Cyan
    if ($id) { Send-BuildEvent @{ type = 'step'; id = $id } }
}
function Fail($m) {
    Write-Host "ERRO: $m" -ForegroundColor Red
    Send-BuildEvent @{ type = 'error'; text = $m }
    exit 1
}

# --- entradas ---------------------------------------------------------------------------------------------
Step 'Entradas' 'check'
if ($BaseNsp -or $UpdateNsp) {
    # Arquivos dados um a um (setup do RecompLauncher): o ticket do update diz a versao ao desempacotar.
    foreach ($f in @($BaseNsp, $UpdateNsp, $Keys)) {
        if (-not $f -or -not (Test-Path -LiteralPath $f -PathType Leaf)) { Fail "Arquivo nao encontrado: '$f' (passe -BaseNsp, -UpdateNsp e -Keys)." }
    }
    $base = @(Get-Item -LiteralPath $BaseNsp)
    $upd = @(Get-Item -LiteralPath $UpdateNsp)
} else {
$all = @(Get-ChildItem -LiteralPath $Jogo -Filter *.nsp -ErrorAction SilentlyContinue)
# Versao: a dos EUA ou a japonesa (vendida tambem na Asia; issue #1), pela base presente (tools\game_profile.py REGIONS).
$regioes = [ordered]@{ US = '0100F15003E64000'; JP = '0100EAE003A5A000' }
$achadas = @($regioes.Keys | Where-Object { $id = $regioes[$_]; $all | Where-Object { $_.Name -match "\[$id\]" } })
if ($achadas.Count -gt 1) { Fail "Ha NSPs da base de mais de uma versao ($($achadas -join ', ')) em $Jogo; deixe uma." }
$regiao = if ($achadas.Count) { $achadas[0] } else { 'US' }
$baseId = $regioes[$regiao]
$updId = $baseId.Substring(0, 13) + '800'
$base = @($all | Where-Object { $_.Name -match "\[$baseId\]" })
$upd = @($all | Where-Object { $_.Name -match "\[$updId\]" })
if ($base.Count -ne 1) { Fail "Esperava 1 NSP da base [0100F15003E64000] (EUA) ou [0100EAE003A5A000] (Japao) em $Jogo (achei $($base.Count))." }
if ($upd.Count -ne 1) { Fail "Esperava 1 NSP do update [$updId] em $Jogo (achei $($upd.Count))." }
if ($regiao -ne 'US') { Write-Host "versao: $regiao (ainda nao testada; conte como roda)" -ForegroundColor Yellow }
}
if (-not $Keys) {
    $Keys = @((Join-Path $Jogo 'prod.keys'), (Join-Path $root 'prod.keys')) | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
}
if (-not $Keys) { Fail 'prod.keys nao encontrado (em "Game Files" ou na raiz). Passe -Keys.' }
Write-Host "base  : $($base[0].Name)"
Write-Host "update: $($upd[0].Name)"
Write-Host "keys  : $Keys"

# --- third_party ------------------------------------------------------------------------------------------
Step 'third_party' 'tools'
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
if ($ToolsData) {
    $roots = @($ToolsData)
} else {
    $roots = @((Join-Path $env:LOCALAPPDATA 'HWDER'))
    $roots += @(Get-ChildItem (Join-Path $env:LOCALAPPDATA 'Packages') -Directory -Filter 'Claude_*' -ErrorAction SilentlyContinue |
                ForEach-Object { Join-Path $_.FullName 'LocalCache\Local\HWDER' })
}
# Python para instalar o toolchain quando o sistema nao tem um: o Python embutivel fixado em
# tools\managed_toolchain.json, conferido pelo SHA-256 (nada e instalado no sistema).
function Get-PinnedPython([string]$dir) {
    $m = Get-Content -Raw -LiteralPath (Join-Path $root 'tools\managed_toolchain.json') | ConvertFrom-Json
    $p = @($m.artifacts | Where-Object { $_.id -eq 'python' })[0]
    $target = Join-Path $dir ('bootstrap-python-' + $p.version)
    $exe = Join-Path $target 'python.exe'
    if (Test-Path -LiteralPath $exe) { return $exe }
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
    $zip = Join-Path $dir ('bootstrap-python-' + $p.version + '.zip')
    Write-Host "baixando o Python $($p.version) fixado ($($p.url))"
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    $old = $ProgressPreference; $ProgressPreference = 'SilentlyContinue'
    try { Invoke-WebRequest -UseBasicParsing -Uri $p.url -OutFile $zip } catch { Fail "Download do Python falhou: $($_.Exception.Message)" }
    finally { $ProgressPreference = $old }
    if ((Get-FileHash -Algorithm SHA256 -LiteralPath $zip).Hash.ToLower() -ne $p.sha256.ToLower()) {
        Remove-Item -LiteralPath $zip -Force; Fail 'O Python baixado nao confere com o SHA-256 fixado; nada foi usado.'
    }
    $partial = "$target.partial"
    if (Test-Path -LiteralPath $partial) { Remove-Item -LiteralPath $partial -Recurse -Force }
    Expand-Archive -LiteralPath $zip -DestinationPath $partial -Force
    Move-Item -LiteralPath $partial -Destination $target
    Remove-Item -LiteralPath $zip -Force
    return $exe
}
$managed = $null
foreach ($r in $roots) {
    $c = @(Get-ChildItem (Join-Path $r 'managed-tools') -Directory -ErrorAction SilentlyContinue |
           Where-Object { Test-Path (Join-Path $_.FullName 'complete.json') })
    if ($c.Count) { $managed = $c[0].FullName; break }
}
if (-not $managed) {
    $sys = (Get-Command python -ErrorAction SilentlyContinue).Source
    # Sem Python, com o atalho da Microsoft Store (WindowsApps) ou no setup do RecompLauncher: o fixado.
    if ($ToolsData -or -not $sys -or $sys -like '*\WindowsApps\*') { $sys = Get-PinnedPython $roots[0] }
    & $sys -X utf8 -u (Join-Path $root 'tools\managed_tools.py') --data $roots[0] --install
    if ($LASTEXITCODE) { Fail 'Instalacao do toolchain falhou.' }
    $managed = (Get-ChildItem (Join-Path $roots[0] 'managed-tools') -Directory | Where-Object { Test-Path (Join-Path $_.FullName 'complete.json') } | Select-Object -First 1).FullName
}
$py = Join-Path $managed 'output\python\python.exe'
Write-Host "toolchain: $managed"

# A janela compila em segundo plano (build\ui, ~1 min na primeira vez; depois so o que mudou) e o proprio
# tools\build.py a abre quando fica pronta: o build do jogo nao espera por ela.
if ($events) {
    $uiDir = Join-Path $root 'build\ui'
    New-Item -ItemType Directory -Force -Path $uiDir | Out-Null
    $q = { param($s) '"' + $s + '"' }  # Start-Process junta os argumentos sem aspas
    $uiArgs = @('-B', '-X', 'utf8', (& $q (Join-Path $root 'tools\build.py')), '--managed-toolchain', (& $q $managed),
                '--build-ui', '--jobs', '4', '--build-dir', (& $q $uiDir), '--open-events', (& $q $events),
                '--watch-pid', $PID)
    try {
        Start-Process -FilePath $py -ArgumentList $uiArgs -NoNewWindow `
            -RedirectStandardOutput (Join-Path $uiDir 'ui-build.log') -RedirectStandardError (Join-Path $uiDir 'ui-build.err.log') | Out-Null
        Write-Host "janela de progresso: compilando em segundo plano; abre sozinha (log: $uiDir\ui-build.log)"
    } catch { Write-Host "(a janela de progresso nao pode ser compilada: $($_.Exception.Message); o build segue aqui)" }
}

$data = Join-Path $root 'data'
$gen = Join-Path $root 'generated'
$build = Join-Path $root 'build\pc'
if (-not $SoBuild) {
    # --- update ------------------------------------------------------------------------------------------
    Step 'Update 1.5.0 (ExeFS + patch do RomFS)' 'update'
    $stamp = Join-Path $data 'update\source.txt'
    $sig = "$($upd[0].FullName)|$($upd[0].Length)|$($upd[0].LastWriteTimeUtc.Ticks)"
    if ((Test-Path $stamp) -and ((Get-Content $stamp -Raw).Trim() -eq $sig) -and (Test-Path (Join-Path $data 'update\update.json'))) {
        Write-Host 'ja desempacotado'
    } else {
        & $py -I -B -X utf8 -c "import sys; sys.path.insert(0, sys.argv[1]); import game_update as g; sys.exit(g.main(sys.argv[2:5]))" `
            (Join-Path $root 'tools') $upd[0].FullName $Keys (Join-Path $data 'update')
        if ($LASTEXITCODE) { Fail 'Falha ao desempacotar o update (o motivo esta na linha "update:" acima).' }
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
    Step 'Traduzindo ARM64 -> C (generated\)' 'translate'
    & $py -B -X utf8 -c "import sys, runpy; sys.path.insert(0, sys.argv[1]); sys.argv = ['gen_main', '--exefs', sys.argv[2], '--out', sys.argv[3]]; runpy.run_module('recomp.gen_main', run_name='__main__')" `
        (Join-Path $root 'tools') (Join-Path $data 'update\exefs') $gen
    if ($LASTEXITCODE) { Fail 'Geracao do C falhou.' }
}

# --- compilar ---------------------------------------------------------------------------------------------
Step "Compilando (build\pc, $Jobs jobs)" 'compile'
& $py -B -X utf8 (Join-Path $root 'tools\build.py') --managed-toolchain $managed --build-dir $build --generated-dir $gen --jobs $Jobs
if ($LASTEXITCODE) { Fail 'Compilacao falhou (rodar de novo retoma).' }
$exe = Join-Path $build 'few.exe'
if (-not (Test-Path $exe)) { Fail "Nao achei $exe" }

# --- publicar ---------------------------------------------------------------------------------------------
$out = Join-Path $root 'Jogar no PC'
Step "Publicando em $out" 'finish'
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
Send-BuildEvent @{ type = 'done'; play = $dst; folder = $out }
if ($Launch) { Start-Process -FilePath $dst -WorkingDirectory $out }
