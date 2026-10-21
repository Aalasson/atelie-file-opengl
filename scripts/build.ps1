param([ValidateSet('Debug','Release')][string]$Configuration='Release',[switch]$Test)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$vswherePath=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if(-not (Test-Path -LiteralPath $vswherePath)){throw 'Instale Visual Studio 2022 com Desktop development with C++ e C++ CMake tools.'}
$vsInstall=& $vswherePath -version '[17.0,18.0)' -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(-not $vsInstall){throw 'Visual Studio 2022 com compilador C++ não encontrado.'}
$cmakeCommand=Get-Command cmake -ErrorAction SilentlyContinue
if($cmakeCommand){$cmakePath=$cmakeCommand.Source}else{
    $cmakePath=Join-Path $vsInstall 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
    if(-not (Test-Path -LiteralPath $cmakePath)){throw 'CMake não encontrado. Instale C++ CMake tools no Visual Studio Installer.'}
}
& $cmakePath -S $projectRoot -B (Join-Path $projectRoot 'build') -G 'Visual Studio 17 2022' -A x64 "-DCMAKE_GENERATOR_INSTANCE=$vsInstall"
if($LASTEXITCODE -ne 0){throw 'Falha na configuração do CMake.'}
& $cmakePath --build (Join-Path $projectRoot 'build') --config $Configuration --parallel
if($LASTEXITCODE -ne 0){throw 'Falha na compilação.'}
if($Test){
    $ctestPath=Join-Path (Split-Path -Parent $cmakePath) 'ctest.exe'
    & $ctestPath --test-dir (Join-Path $projectRoot 'build') -C $Configuration --output-on-failure
    if($LASTEXITCODE -ne 0){throw 'Falha nos testes de lógica.'}
}
Write-Output ('Executável: '+(Join-Path $projectRoot 'build\bin\AtelieFileOpenGL.exe'))
