#!/usr/bin/env bash
#
# Compila Music App (Standalone) en macOS.
# Prerrequisitos: CMake + Apple Command Line Tools (clang).
# Uso:  platform/macos/build.sh
#
set -e

# Raíz del repo = dos niveles arriba de este script.
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"
echo "Repo: $ROOT"

# Configurar (la 1ª vez descarga JUCE 8.0.14 vía FetchContent).
cmake -S . -B build/app -DCMAKE_BUILD_TYPE=Release

# Compilar la app standalone.
cmake --build build/app --target MusicApp_Standalone -j4

echo ""
echo "OK. App: build/app/MusicApp_artefacts/Release/Standalone/Music App.app"
echo "Ejecutar:  open \"build/app/MusicApp_artefacts/Release/Standalone/Music App.app\""
