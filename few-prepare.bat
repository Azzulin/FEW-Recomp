@echo off
rem RecompLauncher's setup step on Windows: builds the game from the player's own files, without a window.
rem The launcher runs it (recomplauncher.json, made by tools\recomplauncher\manifest.py); to build by hand, use
rem gerar-few.ps1 instead.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\recomplauncher\prepare.ps1" %*
exit /b %ERRORLEVEL%
