# Cabinet IR — Design (Milestone 2b, parte 1)

**Date:** 2026-06-29
**Status:** Approved (design)
**Branch:** `feature/cabinet-ir`

## 1. Purpose

Añadir una etapa de **convolución de Impulse Response (IR) de cabinet** a la cadena
de señal, justo después del amplificador NAM y antes del reverb. Permite cargar un
archivo `.wav` de cabinet y desbloquea el uso de capturas NAM "amp solo" (sin cab)
junto con la librería de IRs descargada de tone3000.

Alcance de este ciclo: **solo la etapa IR** (carga + bypass + posición en la cadena).
El drive pre-amp y el selector de modelos quedan para ciclos siguientes.

## 2. Goals / non-goals

**Goals**
- Convolución IR mono real-time-safe tras el NAM, antes del reverb.
- Cargar un `.wav` de IR desde la UI; carga sin glitches (off-thread).
- **Bypass real** por etapa (no procesar cuando está OFF), por defecto OFF.
- Resolución de IR por defecto multiplataforma (espejo de `resolveDefaultModel`).

**Non-goals (YAGNI)**
- Mezcla wet/dry del IR (un cab IR va 100% wet; no se añade knob).
- IR estéreo / true-stereo (la cadena es mono).
- Selector/lista de IRs en la UI (solo botón de carga este ciclo).
- Drive pre-amp y selector de modelos (ciclos siguientes).

## 3. Por qué default OFF

El modelo por defecto actual (Fender Super Reverb) es una captura **amp-cab** (ya
incluye el cabinet). Aplicarle un IR encima sería **doble-cab**. Por eso el IR
arranca **bypasseado**; el usuario lo activa al cargar una captura "amp solo".

## 4. Arquitectura (enfoque: `juce::dsp::Convolution`)

`juce::dsp::Convolution` (FFT particionada, latencia ~cero). Su
`loadImpulseResponse(File, …)` es thread-safe: carga en background y hace swap
atómico del IR sin glitch, resamplea al sample rate de la sesión y normaliza —
internamente. Es la opción que ya nombra el CLAUDE.md.

**Cadena de señal resultante:**
```
input → suma L+R a mono → NAM → normGain por loudness → [Cabinet IR si irOn]
      → reverb (post-FX) → limitador de seguridad → output
```

## 5. Componentes

### 5.1 `PluginProcessor` (motor headless)
- Miembro nuevo: `juce::dsp::Convolution mConvolution;`
- Parámetro APVTS `irOn` (`AudioParameterBool`, default `false`); puntero atómico
  `std::atomic<float>* mIrOn` cacheado en el constructor.
- `bool loadIR(const juce::File& file)` (message-thread): llama
  `mConvolution.loadImpulseResponse(file, Stereo::no, Trim::no, 0, Normalise::yes)`;
  guarda `mIrLoadedName = file.getFileName()`; devuelve `true`/`false`.
- `juce::String getLoadedIRName() const`.
- `resolveDefaultIR()` (anónimo en el .cpp, espejo de `resolveDefaultModel`):
  env `MUSICAPP_DEFAULT_IR` (ruta a `.wav`) → primer `.wav` en
  `~/Documents/Music App/irs/` → `juce::File{}` (ninguno). Cargar en el constructor
  si existe (irOn sigue false).
- `prepareToPlay`: `mConvolution.prepare({sampleRate, (uint32) samplesPerBlock, 1})`
  y `mConvolution.reset()`.
- `processBlock`: tras escribir `mWork` (float, post-normGain) y **antes** del
  reverb: si `mIrOn->load() > 0.5f`, envolver `mWork` (n muestras, 1 canal) en
  `juce::dsp::AudioBlock<float>` + `ProcessContextReplacing` y llamar
  `mConvolution.process(ctx)`. Si OFF, no se toca `mWork` (bypass real).

### 5.2 `PluginEditor` (UI)
- Fila **IR** nueva (bajo el label de modelo): botón **"Cargar IR..."**, toggle
  **"IR"** (ON/OFF) y label que muestra el IR cargado o "(ninguno)".
- `chooseIR()`: `FileChooser` (filtro `*.wav`), directorio inicial multiplataforma
  (`~/Documents/Music App/irs` si existe, si no `userMusicDirectory`).
- Attachment del toggle al parámetro `irOn` (`ButtonAttachment`).
- La ventana crece de `520×300` a `520×352` para acomodar la fila IR; `resized()`
  reubica la fila IR entre el label de modelo y los knobs.
- Paleta y estilo dark existentes (tokens en `PluginEditor.h`).

## 6. Real-time safety

- Cargar IR (`loadImpulseResponse`) y `prepare` ocurren **fuera** del audio thread;
  el swap del IR lo gestiona JUCE de forma lock-free.
- En `processBlock` no se asigna memoria: `juce::dsp::Convolution` usa buffers
  pre-asignados en `prepare`. El bypass evita incluso ese coste cuando está OFF.
- El limitador de seguridad final (NaN/Inf→0, `jlimit(-1,1)`) sigue protegiendo la
  salida tras el IR.

## 7. Verificación

Coherente con cómo este proyecto valida hitos (en vivo / offline, no unit tests de
audio):
1. **Compila** en Windows (`platform\windows\build.ps1`, exit 0).
2. **Carga**: lanzar la app, cargar uno de los `.wav` IR descargados, screenshot
   mostrando el IR cargado y el toggle IR.
3. **Efecto audible**: alternar IR ON/OFF cambia el timbre — comprobado en vivo con
   interfaz, o con un render offline (tool `render` del NAM core para A/B).
4. Sin xruns/glitches al activar/desactivar o al cargar un IR mientras suena.

## 8. Archivos afectados

- `src/PluginProcessor.h` — miembro convolución, `loadIR`, `getLoadedIRName`, param.
- `src/PluginProcessor.cpp` — `resolveDefaultIR`, layout param `irOn`, prepare,
  process, carga inicial.
- `src/PluginEditor.h` — controles IR (botón, toggle, label) + attachment.
- `src/PluginEditor.cpp` — `chooseIR`, estilo, `resized` (ventana 520×352).
- `CMakeLists.txt` — enlazar `juce::juce_dsp` (módulo de la convolución).

## 9. Dependencias

`juce::juce_dsp` (módulo JUCE, ya disponible vía FetchContent — solo hay que
enlazarlo). Ninguna dependencia externa nueva.
