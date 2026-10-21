$ErrorActionPreference='Stop'
$appPath=Join-Path $PSScriptRoot 'build\bin\AtelieFileOpenGL.exe'
if(-not(Test-Path -LiteralPath $appPath)){& (Join-Path $PSScriptRoot 'scripts\build.ps1') -Test}
# Suppress a second console window while allowing the game's own Win32 window.
$gameProcessInfo=[System.Diagnostics.ProcessStartInfo]::new()
$gameProcessInfo.FileName=$appPath
$gameProcessInfo.WorkingDirectory=$PSScriptRoot
$gameProcessInfo.UseShellExecute=$false
$gameProcessInfo.CreateNoWindow=$true
[System.Diagnostics.Process]::Start($gameProcessInfo) | Out-Null
