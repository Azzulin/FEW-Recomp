<#
.SYNOPSIS
  Unattended test run of few.exe in build\run (its own settings, save and log; never the player's "Jogar no PC").

.EXAMPLE
  powershell -File tools\run_test.ps1 -Seconds 90 -Script tools\input\a_every_2s.txt -Shots 20,40,60 -Fresh
#>
param(
    [int]$Seconds = 60,
    [string]$Script,                 # HWDER_INPUT_SCRIPT file
    [string]$Shots = '',             # screenshot times in s, "10,20,30" -> build\shots\<Tag>_<s>.png
    [string]$Tag = 'run',
    [switch]$Fresh,                  # start with an empty save
    [string]$Env = ''                # extra variables, "NAME=VALUE;NAME=VALUE" (e.g. HWDER_IPC_TRACE=1)
)
# -File passes everything as text: split here.
$ShotList = @($Shots -split ',' | Where-Object { $_.Trim() } | ForEach-Object { [double]$_ })
$EnvList = @($Env -split ';' | Where-Object { $_.Trim() })
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$play = Join-Path $root 'Jogar no PC'
$run = Join-Path $root 'build\run'
$shots = Join-Path $root 'build\shots'
New-Item -ItemType Directory -Force $run, $shots | Out-Null

# The published exe, hard-linked (same volume, no copy); relinked every run because gerar-few.ps1 replaces it.
$exe = Join-Path $run 'few.exe'
if (Test-Path -LiteralPath $exe) { Remove-Item -LiteralPath $exe -Force }
New-Item -ItemType HardLink -Path $exe -Target (Join-Path $play 'few.exe') | Out-Null
# Settings: the player's game/keys/update paths, nothing else.
$src = Get-Content -LiteralPath (Join-Path $play 'hwder_settings.ini') | Where-Object { $_ -match '^(version|nsp_path|keys_path|update_path)=' }
Set-Content -LiteralPath (Join-Path $run 'hwder_settings.ini') -Value $src -Encoding utf8
if ($Fresh -and (Test-Path -LiteralPath (Join-Path $run 'save'))) { Remove-Item -LiteralPath (Join-Path $run 'save') -Recurse -Force }
$log = Join-Path $run 'hwder.log'
if (Test-Path -LiteralPath $log) { Remove-Item -LiteralPath $log -Force }

$env:HWDER_NO_PRELAUNCH = '1'; $env:HWDER_OFFSCREEN = '1'; $env:HWDER_MUTE = '1'
if ($Script) { $env:HWDER_INPUT_SCRIPT = (Resolve-Path -LiteralPath $Script).ProviderPath } else { Remove-Item Env:HWDER_INPUT_SCRIPT -ErrorAction SilentlyContinue }
foreach ($kv in $EnvList) { $k, $v = $kv -split '=', 2; Set-Item -Path "Env:$k" -Value $v }

$p = Start-Process -FilePath $exe -WorkingDirectory $run -PassThru
$cap = Join-Path $PSScriptRoot 'screenshot.ps1'
foreach ($s in ($ShotList | Sort-Object)) {
    $wait = $s - ((Get-Date) - $p.StartTime).TotalSeconds
    if ($wait -gt 0) { Start-Sleep -Milliseconds ([int]($wait * 1000)) }
    if ($p.HasExited) { break }
    & powershell -NoProfile -ExecutionPolicy Bypass -File $cap (Join-Path $shots "${Tag}_$s.png") | Out-Null
}
$left = $Seconds - ((Get-Date) - $p.StartTime).TotalSeconds
if ($left -gt 0 -and -not $p.HasExited) { [void]$p.WaitForExit([int]($left * 1000)) }
if ($p.HasExited) { Write-Output "exited by itself, code $($p.ExitCode)" } else { Stop-Process -Id $p.Id -Force; Write-Output "stopped after $Seconds s" }
Write-Output "log: $log"
