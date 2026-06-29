# Music App — Windows

Build e instrucciones específicas de Windows. El **código es compartido** con macOS
(misma carpeta `src/`); aquí solo vive lo propio de la plataforma.

## Prerrequisitos

- **Visual Studio 2022** con el workload **"Desktop development with C++"**
  (instalable con `winget install Microsoft.VisualStudio.2022.Community`
  añadiendo el workload `Microsoft.VisualStudio.Workload.NativeDesktop`).
- **CMake** (`winget install Kitware.CMake`).
- Repo clonado **con submódulos**:
  ```powershell
  git clone --recursive https://github.com/AutomatizacionesBCH/MUSICAPP.git
  # o, si ya estaba clonado:
  git submodule update --init --recursive
  ```
  (NeuralAmpModelerCore anida Eigen, nlohmann y AudioDSPTools como submódulos.)

## Compilar

```powershell
powershell -ExecutionPolicy Bypass -File platform\windows\build.ps1
```

La 1ª vez descarga JUCE 8.0.14 (FetchContent) → tarda. El ejecutable queda en
`build/app/MusicApp_artefacts/Release/Standalone/Music App.exe`.

## Audio / latencia (Windows)

- JUCE usa **WASAPI** por defecto (funciona out-of-the-box). Para baja latencia
  con guitarra, preferir **ASIO** (driver de la interfaz, p. ej. Arturia MiniFuse).
  Habilitar ASIO requiere el **ASIO SDK de Steinberg** + `JUCE_ASIO=1` (pendiente).
- Al ejecutar: conectar la interfaz → botón **PREFS. AUDIO** → elegir entrada /
  48 kHz / buffer 128.

## Modelo por defecto al arrancar (multiplataforma)

La app intenta cargar un modelo `.nam` al inicio, sin rutas hardcodeadas:
1. variable de entorno `MUSICAPP_DEFAULT_MODEL` (ruta a un `.nam`), o
2. el primer `.nam` en `%USERPROFILE%\Documents\Music App\models\`, o
3. ninguno → arranca en *passthrough*; carga uno con el botón **CARGAR .NAM**.

## Específico de MSVC (ya en CMakeLists)

`/utf-8` (acentos), `/bigobj` (templates de Eigen/NAM), `_USE_MATH_DEFINES` (M_PI),
y `$<LINK_LIBRARY:WHOLE_ARCHIVE,nam_core>` se traduce a `/WHOLEARCHIVE` de MSVC.
