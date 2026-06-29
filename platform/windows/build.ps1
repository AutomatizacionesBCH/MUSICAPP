<#
  Compila Music App (Standalone) en Windows.

  Prerrequisitos:
    - Visual Studio 2022 con el workload "Desktop development with C++".
    - CMake (en PATH).
    - El repo clonado con submódulos:  git clone --recursive  (o
      git submodule update --init --recursive).

  Uso (desde cualquier sitio):
    powershell -ExecutionPolicy Bypass -File platform\windows\build.ps1
#>
$ErrorActionPreference = "Stop"

# Raíz del repo = dos niveles arriba de este script (platform\windows\..\..).
$root = Resolve-Path (Join-Path $PSScriptRoot "..\..")
Set-Location $root
Write-Host "Repo: $root"

# Configurar (la 1ª vez descarga JUCE 8.0.14 vía FetchContent).
cmake -S . -B build/app -G "Visual Studio 17 2022" -A x64
if ($LASTEXITCODE -ne 0) { throw "cmake configure falló" }

# Compilar la app standalone en Release.
cmake --build build/app --target MusicApp_Standalone --config Release -- -m
if ($LASTEXITCODE -ne 0) { throw "cmake build falló" }

$exe = Join-Path $root "build\app\MusicApp_artefacts\Release\Standalone\Music App.exe"
Write-Host ""
Write-Host "OK. Ejecutable: $exe"
