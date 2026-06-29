# Music App — macOS

Build e instrucciones específicas de macOS. El **código es compartido** con Windows
(misma carpeta `src/`); aquí solo vive lo propio de la plataforma.

## Prerrequisitos

- **CMake** (binario oficial; en este Mac `brew install cmake` compila desde fuente
  y es lentísimo → usar el binario precompilado en `~/.local/` con symlink en
  `/usr/local/bin/cmake`). ⚠️ NO instalar CMake en un scratchpad temporal.
- **Apple Command Line Tools** (clang):  `xcode-select --install`.
- Repo clonado con submódulos:  `git clone --recursive …` (o
  `git submodule update --init --recursive`).

## Compilar

```bash
platform/macos/build.sh
# o a mano:
cmake -S . -B build/app -DCMAKE_BUILD_TYPE=Release
cmake --build build/app --target MusicApp_Standalone -j4
open "build/app/MusicApp_artefacts/Release/Standalone/Music App.app"
```

## Notas

- Probado en vivo con interfaz **Arturia MiniFuse 2** (CoreAudio).
- `-stdlib=libc++` se aplica solo en APPLE (ya en CMakeLists).
- Modelo por defecto al arrancar: ver la sección equivalente en
  `platform/windows/README.md` (misma lógica multiplataforma:
  `MUSICAPP_DEFAULT_MODEL` → `~/Documents/Music App/models/*.nam` → passthrough).
