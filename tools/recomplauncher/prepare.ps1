<#
RecompLauncher's setup step on Windows (started by few-prepare.bat, see tools/recomplauncher/manifest.py).

    few-prepare.bat --game=<base.nsp> --update=<update.nsp> --keys=<prod.keys>

Runs gerar-few.ps1 with the player's files and turns its output into the launcher's "progress <0-1> <text>" lines.
The private toolchain (and the pinned Python that installs it, so the PC needs no Python) goes to
%LOCALAPPDATA%\HWDER, as with gerar-few.ps1 by hand: a short path (see below). When the game in "Jogar no PC" was built by this release from
the same update, it says "Already built" and stops. On a failure the reason is printed last, in English (gerar-few's
own messages are in Portuguese), because the launcher shows the last 10 lines.
#>
$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [Text.Encoding]::UTF8
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$gameDir = Join-Path $root 'Jogar no PC'
$stampFile = Join-Path $gameDir 'few-build.json'
$exe = Join-Path $gameDir 'few.exe'

function Write-Progress-Line([double]$fraction, [string]$text) {
    $f = [Math]::Max(0.0, [Math]::Min(1.0, $fraction))
    [Console]::Out.WriteLine(('progress {0} {1}' -f $f.ToString('0.000', [Globalization.CultureInfo]::InvariantCulture), $text))
    [Console]::Out.Flush()
}
function Write-Line([string]$text) { [Console]::Out.WriteLine($text); [Console]::Out.Flush() }

# --- arguments: --game=... --update=... --keys=... (one argument each, as the launcher passes them) -------------
$given = @{}
foreach ($a in $args) {
    if ($a -match '^--(game|update|keys)=(.*)$') { $given[$Matches[1]] = $Matches[2] }
}
$what = @{ game = 'base game'; update = 'update 1.5.0'; keys = 'prod.keys' }
foreach ($k in 'game', 'update', 'keys') {
    if (-not $given[$k] -or -not (Test-Path -LiteralPath $given[$k] -PathType Leaf)) {
        Write-Line "The $($what[$k]) file was not found: '$($given[$k])'. Pick it again on the setup screen."
        exit 2
    }
}

# --- already built by this release from this update? -------------------------------------------------------
$release = 'dev'
$releaseFile = Join-Path $root 'RELEASE'
if (Test-Path -LiteralPath $releaseFile) { $release = (Get-Content -Raw -LiteralPath $releaseFile).Trim() }
$u = Get-Item -LiteralPath $given['update']
$key = "$release|$($u.FullName)|$($u.Length)|$($u.LastWriteTimeUtc.Ticks)"
if ((Test-Path -LiteralPath $exe) -and (Test-Path -LiteralPath $stampFile) -and
    ((Get-Content -Raw -LiteralPath $stampFile).Trim() -eq $key)) {
    Write-Progress-Line 1 'Already built'
    exit 0
}
if (Test-Path -LiteralPath $stampFile) { Remove-Item -LiteralPath $stampFile -Force }  # a cut-short build never looks finished

# The private toolchain lives in %LOCALAPPDATA%\HWDER, where gerar-few.ps1 keeps it by default (shared with builds
# by hand), not in {data}\cache: it puts files up to ~190 characters below its folder, and a DLL or a header past
# Windows' 260-character limit fails to load (measured: _cffi_backend under a long game folder). FEW_TOOLS_DATA
# moves it (tests).
$tools = $env:FEW_TOOLS_DATA
if (-not $tools) { $tools = Join-Path $env:LOCALAPPDATA 'HWDER' }
$m = Get-Content -Raw -LiteralPath (Join-Path $root 'tools\managed_toolchain.json') | ConvertFrom-Json
$toolIds = @($m.artifacts | ForEach-Object { $_.id })

# gerar-few.ps1's steps ("==> <text>") -> where they start on the bar, and what the player reads.
$steps = @(
    @('Entradas', 0.01, 'Checking your files'),
    @('third_party', 0.02, 'Checking the bundled libraries'),
    @('Toolchain privado', 0.03, 'Downloading the build tools (once, about 260 MB)'),
    @('Update 1.5.0', 0.15, 'Unpacking update 1.5.0'),
    @('Traduzindo', 0.2, "Translating the game's code to C"),
    @('Compilando', 0.3, 'Compiling'),
    @('Publicando', 0.98, 'Finishing the game folder')
)
$state = @{ text = 'Starting'; base = 0.0; last = -1.0; when = [DateTime]::MinValue; error = ''; update = ''; long = $false }
Write-Progress-Line 0 'Starting the build'

$params = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', (Join-Path $root 'gerar-few.ps1'),
            '-BaseNsp', $given['game'], '-UpdateNsp', $given['update'], '-Keys', $given['keys'], '-ToolsData', $tools)
$ErrorActionPreference = 'Continue'  # a native program's stderr is output here, not a failure
& powershell.exe @params 2>&1 | ForEach-Object {
    $line = "$_"
    Write-Line $line
    $t = $line.Trim()
    if ($t -match '^==> (.*)$') {
        foreach ($s in $steps) {
            if ($Matches[1].StartsWith($s[0])) {
                $state.text = $s[2]; $state.base = $s[1]
                Write-Progress-Line $s[1] $s[2]; $state.last = $s[1]; $state.when = Get-Date
                break
            }
        }
    } elseif ($t -match '^ERRO: (.*)$') {
        $state.error = $Matches[1]
    } elseif ($t -match '^update: (.*)$' -and $state.text -like 'Unpacking*') {
        $state.update = $Matches[1]  # game_update.py's reason (in English) when the update cannot be unpacked
    } elseif ($t -match '^\[(\d+)/(\d+)\]' -and $state.text -eq 'Compiling') {
        $f = 0.3 + 0.67 * [int]$Matches[1] / [Math]::Max(1, [int]$Matches[2])
        if ($f - $state.last -ge 0.01 -or ((Get-Date) - $state.when).TotalSeconds -ge 5) {
            Write-Progress-Line $f "Compiling ($($Matches[1]) of $($Matches[2]))"; $state.last = $f; $state.when = Get-Date
        }
    } elseif ($t -match '"message":\s*"([^"]+)"') {
        # managed_tools.py events (JSON lines) while the toolchain installs: the bar moves by tool, in the order of
        # tools\managed_toolchain.json (3% to 15%).
        $msg = $Matches[1]
        $i = -1
        if ($msg -match '^(?:Downloading|Extracting|Checking|Verifying) ([\w.-]+)') { $i = [Array]::IndexOf($toolIds, $Matches[1]) }
        $f = if ($i -ge 0) { 0.03 + 0.12 * $i / [Math]::Max(1, $toolIds.Count) } else { [Math]::Max($state.last, 0.03) }
        if ($f - $state.last -ge 0.01 -or ((Get-Date) - $state.when).TotalSeconds -ge 5) {
            Write-Progress-Line $f $msg; $state.last = $f; $state.when = Get-Date
        }
    }
    if ($t -match 'too long|muito grande|PathTooLong|demasiado largo|trop long|zu lang') { $state.long = $true }
}
$code = $LASTEXITCODE
if ($code -ne 0) {
    Write-Line "The build stopped during: $($state.text). Press Set Up again to continue where it stopped."
    if ($state.error) { Write-Line "gerar-few.ps1: $($state.error)" }
    if ($state.update -and $state.text -like 'Unpacking*') { Write-Line $state.update }  # the actual reason, last
    if ($state.long) {
        Write-Line "A path was too long for Windows. Move the game to a library folder with a shorter path (for example C:\Games), or turn on Windows' long paths setting, then press Set Up again."
    }
    exit $code
}
if (-not (Test-Path -LiteralPath $exe)) {
    Write-Line "The build ended without $exe; see the output above."
    exit 1
}
Set-Content -LiteralPath $stampFile -Value $key -Encoding UTF8
Write-Progress-Line 1 'Ready to play'
exit 0
