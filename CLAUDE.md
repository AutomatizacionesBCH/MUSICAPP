# Music App — Herramienta de tonos para músicos (NAM + tone3000)

> App nativa de escritorio (luego móvil) que procesa la señal de una guitarra/bajo en
> tiempo real a través de modelos **NAM (Neural Amp Modeler)**, IRs de cabinet y efectos,
> con un buscador de tonos conectado a **tone3000.com**. Una especie de AmpliTube, pero
> usando el ecosistema abierto de NAM.

**Uso:** personal y no comercial (herramienta propia del autor). Esto baja drásticamente
el riesgo legal y elimina la necesidad de pensar en App Stores o licencias comerciales.

## Estado actual

- **Fase 0 — UI/UX:** mockup de la pantalla principal en `design/mockups/main-screen.html`
  (semilla de la futura UI WebView). Pendiente: validar dirección, más pantallas/flujo.
- **Fase 1 — Spike (en curso):** Milestone 0 ✅ — `NeuralAmpModelerCore` clonado en `libs/` e
  integrado; compilado con CMake 3.30.5 (tools `render`/`benchmodel` en `build/namcore/`). Render
  offline funciona y los modelos de ejemplo suenan (`afplay`). **Benchmark en el Mac del autor
  (i5-7360U, 2 núcleos), buffer 64 @ 48 kHz, 2 s de audio:** A2-max 185 ms → **~10.8× tiempo real
  (~9% de un núcleo)**; A1-Standard 225 ms → **~8.9× (~11%)**. → 1 instancia en vivo es muy holgada,
  con margen para IR + efectos.
- **Fase 1 — Milestone 1 ✅ (compila):** proyecto JUCE standalone montado (`CMakeLists.txt` + `src/`)
  y **compila sin errores** → `build/app/MusicApp_artefacts/Release/Standalone/Music App.app`
  (JUCE 8.0.14 vía FetchContent). **Pendiente:** ejecutarlo con la interfaz real para confirmar el
  audio en vivo (aún NO se ha corrido — verificación pendiente para la próxima sesión).
- El catálogo de tonos se integrará vía la **API oficial de tone3000** (Fase 2); por ahora todo local.

### Receta de integración de NAM Core (verificada al compilar)
- Fuentes a compilar: `libs/NeuralAmpModelerCore/NAM/*.cpp` + `NAM/*/*.cpp` (incluye `wavenet/`).
- Includes: la raíz del repo, `Dependencies/eigen`, `Dependencies/nlohmann`.
- C++20; en macOS `-stdlib=libc++`. Definir `NAM_ENABLE_A2_FAST` (fast-path A2, ON por defecto).
- API en vivo: `auto m = nam::get_dsp(path)` (fuera del audio thread) → `m->Reset(sampleRate, maxBlock)`
  → en el callback `m->process(...)`. NAM trabaja en **double**; convertir float↔double fuera del hot path.
- Toolchain: CMake oficial 3.30.5 instalado en `/usr/local/bin/cmake` (brew compilaba desde fuente, muy lento).

## Decisiones de arquitectura (fijadas 2026-06-28)

| Decisión | Elección | Por qué |
|---|---|---|
| Lenguaje / framework | **JUCE / C++** desde cero | Único framework que cubre Win+macOS+iOS+Android en un codebase, con I/O de audio nativo, AUv3 y `dsp::Convolution` para IR. Genera standalone + plugin VST3/AU casi gratis. |
| Licencia JUCE | **AGPLv3 (gratis)** | Vale para uso personal no distribuido; no aplica el coste de la licencia comercial. |
| Motor de inferencia | `NeuralAmpModelerCore` (MIT) o el wrapper `NeuralAudio` (MIT) | Cargan `.nam` (A1 y A2), pensados para tiempo real. `NeuralAudio` da una API más alta y marca lo que NO es real-time-safe. |
| Convolución IR | `juce::dsp::Convolution` (o `FFTConvolver` MIT / motor híbrido zero-latency) | Lineal, barata, latencia ~cero. |
| Fuente de tonos | **API oficial de tone3000 (OAuth 2.0 + PKCE)** | Canal sancionado para apps de terceros; sin scraping; sin re-hosting. |
| Formato de plataforma | **Standalone + plugin** (VST3/AU) | Comparten el mismo core DSP; sale casi gratis con JUCE. |
| Roadmap | Desktop primero (Win/macOS) → móvil (iOS AUv3 / Android Oboe) | El core C++ se reutiliza; se reescriben I/O de audio y UI por plataforma. |

## Cadena de señal de audio

```
input → pre-FX → amp NAM → cabinet IR → post-FX → output
```

Es exactamente la cadena que implementa el plugin oficial open source
`sdatkinson/NeuralAmpModelerPlugin` — úsalo como referencia de arquitectura.

- **NAM** modela el comportamiento no-lineal y dinámico del amplificador/pedal/preamp.
- **IR** captura la respuesta lineal del cabinet+micro+sala.
- Coste: la **inferencia NAM es la etapa cara**; la convolución IR es barata.

## NAM / "NAM2" — notas técnicas

- "NAM2" = **NAM Architecture 2 (A2)**: nueva arquitectura abierta (MIT) de junio 2026
  (tone3000 + Steven Atkinson), 30-40% menos CPU que A1-Standard, corre hasta en chips de $3.
  No es un producto separado; es la evolución del mismo NAM. Priorizar A2 para el camino a móvil.
- Modelos = archivos **`.nam`** (JSON: `version`, `architecture` [WaveNet|LSTM], `config`,
  `weights`; opcional `sample_rate` [def. 48 kHz] y `metadata`). Tamaño típico: cientos de KB.
  Existe `.namb` (binario, MIT) ~10× más pequeño para móvil/embebido.
- 1 instancia de NAM Standard ≈ 0.20 de un core (corre hasta en Raspberry Pi 5).
  Variantes ligeras: Lite / Feather / Nano (preferir en móvil).
- Latencia objetivo: buffers de **64-128 samples** → RTL ~3-6 ms a 96 kHz. NAM no añade
  latencia propia; la fija el buffer.

## Reglas de oro de tiempo real (audio thread)

Violarlas causa xruns/glitches audibles. En `processBlock` / callback de audio:

- **Nunca** alocar memoria, usar locks, ni hacer I/O de disco.
- Cargar modelos **fuera** del hilo de audio: `get_dsp(path)` → `Reset()` → `prewarm()`,
  y solo `DSP::process(in, out)` dentro del callback.
- Comunicar UI ↔ audio con atomics / FIFOs lock-free.
- Compilar Eigen con `EIGEN_MAX_ALIGN_BYTES=0`, `EIGEN_DONT_VECTORIZE` (evita crashes de
  alineación) y `EIGEN_MPL2_ONLY` (excluye el rastro LGPL si se enlaza estático).
- Enmascarar denormales/excepciones FPU (FTZ/DAZ).
- Implementar **bypass real** (no solo silenciar) de cada etapa: el coste por instancia es
  fijo y corre aunque no haya señal (ver issue #553 del plugin oficial).

## Integración con tone3000 (legal y técnica)

**SIEMPRE usar la API oficial. NUNCA scrapear ni re-alojar contenido.**

- API: OAuth 2.0 + PKCE. El usuario inicia sesión con **su** cuenta tone3000.
- La app guarda **`tone_id`** en sus presets y resuelve/descarga el modelo **bajo el token
  del propio usuario** (flujos *Select* / *Load Tone*). Cache local solo de **sus** tonos.
- Rate limit por defecto 100 req/min; para producción contactar `support@tone3000.com`.
- **Por qué importa:**
  - El **software** NAM (core + plugin) es **MIT** → embebible libremente. ✅
  - El **contenido** `.nam` es propiedad del **autor que lo subió**, NO de libre
    redistribución (T3K license: puedes usarlo y publicar el audio resultante, pero no
    re-subir/redistribuir el archivo). → No re-alojar nunca.
  - Scraping masivo **viola el ToS** de tone3000 explícitamente (aunque la descarga manual
    individual sea gratis).
  - "Cliente que descarga bajo la cuenta del usuario" es legalmente mucho más defendible que
    "servidor que re-aloja" (conducta volitiva / *Cablevision*; safe harbor DMCA).
- **Marcas (Marshall/Fender/etc.):** solo texto descriptivo, sin logos ni sugerir afiliación;
  incluir disclaimer de no-afiliación (fair use nominativo).

## Arquitectura de UI (desacoplada — clave para migrar a UI custom)

Tres capas, con frontera estable entre ellas:

1. **Core de audio C++ headless** — `AudioProcessor` con la cadena `NAM → IR → FX` que
   funciona 100% sin editor. Estado canónico en `juce::AudioProcessorValueTreeState` (APVTS)
   + un `ValueTree` propio para la cadena de bloques reordenables.
2. **Fachada de comandos** (lo no-paramétrico que APVTS no modela): `loadNamModel(path)`,
   `loadIR(path)`, `applyTone3000Result(toneJson)`, `reorderBlock(...)`, y canales read-only
   para medidores/espectro/tuner. Usa el patrón **staging + swap atómico** del plugin oficial
   NAM (carga el modelo en thread de fondo y lo intercambia sin glitch).
3. **UI swappable** = cliente que SOLO habla por APVTS + fachada; nunca toca el DSP.

**Tecnología de UI: JUCE 8 WebView** (`juce::WebBrowserComponent` + `WebSliderRelay` /
`WebToggleButtonRelay` / `WebComboBoxRelay` + `withNativeFunction` / `withEventListener` /
`withResourceProvider`). La UI vive en HTML/CSS/JS → se rediseña/migra sin recompilar el motor
y se reutiliza en móvil/web. El mockup en `design/mockups/main-screen.html` es la **semilla** de
esta UI. Alternativa de menor fricción en Windows: `foleys_gui_magic` (BSD-3, UI nativa JUCE por
estilos, sin runtime WebView2; menos techo de personalización).

**Reglas de oro del desacople (innegociables):**
- La UI nunca llama métodos del processor ni toca DSP — solo APVTS + fachada.
- IDs de parámetro y firmas de funciones nativas = API pública versionada.
- El processor se instancia y testea **headless** (sin editor).
- Rig y presets = **JSON puro** (bloques + parámetros + rutas NAM/IR + `tone_id`), independiente del render.
- Caveat verificado: el plugin oficial NAM **no** es ejemplo de desacople (es iPlug2, monolítico);
  reutiliza `NeuralAmpModelerCore` (MIT, headless) como motor y del plugin solo el staging/swap.
  Windows necesita runtime WebView2 (Win11 lo trae; Win10 desde mediados 2022).

## Dirección visual (tokens — "dark pro audio tool", basada en herramientas reales)

No skeuomórfico. Carbón (no negro) + un acento ámbar/válvula. Referencias: NAM, AIDA-X, MOD Desktop, BYOD.
- Superficies: `--bg #0E0F11` · `--panel #17191C` · `--module #1E2125` · `--border #2A2E33` · `--track #33373D`
- Texto: `#E6E8EA` / `#9AA0A6` / `#5A6068`
- Acento: `--accent #FF7A29` (+ `#FFA45C`), solo selección y anillo de knob activo
- Medidores (comportamiento AIDA-X): verde `#2BD66A` → ámbar `#F4C430` (-3 dB) → rojo `#FF3B30` (clip), horizontales ~60 fps
- Tipografía: sans neo-grotesca (Inter/IBM Plex Sans) en labels MAYÚSCULAS con tracking; **mono** (JetBrains Mono) para todo valor numérico
- Knobs: vectoriales/SVG, cuerpo `#23262B`, anillo ámbar; Ctrl=fino, Shift/doble-click=reset
- Layout: signal flow **horizontal** izq→der (NAM→IR→FX) como rack de módulos arrastrables; buscador TONE3000 en drawer izquierdo; barra inferior full-width con IO + medidores + tuner + bypass global
- Patrón de cadena: **serie con slots reordenables** (no patchbay de cables libre); modo Dual/Paralelo aparte

## Reutilización y licencias (recordatorio: uso PERSONAL no distribuido)

Como NO se distribuye, GPL/AGPL no obliga a nada: puedes leer, forkear y reusar incluso código GPL
libremente. La distinción "reutilizar vs solo aprender" de la investigación asume distribución; relájala
para este caso. Aun así, conviene mantener el core limpio/portable:
- **Motor (MIT, ideal):** `NeuralAmpModelerCore`, `mikeoliphant/NeuralAudio`, `RTNeural`, `FFTConvolver`.
- **Atajo grande:** `Tr3m/nam-juce` (GPL) ya cablea NAM Core en un `PluginProcessor`/`PluginEditor` JUCE
  (tu stack exacto); al ser personal puedes partir de ahí en vez de desde cero.
- **UI desacoplada:** `ffAudio/foleys_gui_magic` (BSD-3) o JUCE WebView; esqueleto `JanWilczek/juce-webview-tutorial` (MIT).
- **tone3000:** repo oficial `tone-3000/api` (reimplementar cliente en C++) + `Mhr1375/MiniOAuth2` (MIT) para OAuth2/PKCE loopback.
- **Pedalboard/efectos a estudiar:** `Chowdhury-DSP/BYOD`, `mikeoliphant/stompbox`, `mod-audio/mod-desktop`.

## Estructura del proyecto

```
Music App/
├─ CLAUDE.md                   ← fuente de verdad (este archivo)
├─ README.md                   ← overview para GitHub
├─ CMakeLists.txt              ← build: JUCE 8.0.14 (FetchContent) + NAM Core + app standalone
├─ .gitignore
├─ src/
│  ├─ PluginProcessor.{h,cpp}  ← motor headless: input → NAM → output (estado en APVTS)
│  └─ PluginEditor.{h,cpp}     ← UI del spike (gains, cargar .nam, preferencias de audio)
├─ libs/
│  └─ NeuralAmpModelerCore/    ← motor NAM (MIT) + Eigen/nlohmann (submódulos) — pendiente convertir a submódulo git
├─ design/mockups/main-screen.html  ← mockup de la UI definitiva (semilla de la futura WebView)
└─ build/                      ← artefactos (gitignored)
   ├─ namcore/tools/           ← render / benchmodel (Milestone 0)
   └─ app/…/Music App.app      ← app standalone (Milestone 1)
```

## Cómo construir y ejecutar (desktop macOS)

Requisitos: **CMake** (3.30.5 oficial en `/usr/local/bin`), **Apple Command Line Tools** (clang).
JUCE se descarga solo la 1ª vez (FetchContent).

```bash
cd "Music App"
cmake -S . -B build/app -DCMAKE_BUILD_TYPE=Release          # configurar (1ª vez baja JUCE)
cmake --build build/app --target MusicApp_Standalone -j4    # compilar
open "build/app/MusicApp_artefacts/Release/Standalone/Music App.app"   # ejecutar
```

Al ejecutar: conectar la interfaz → **Preferencias de audio** (entrada / 48 kHz / buffer 128) → el
modelo A2 de ejemplo carga solo y se toca en vivo. ⚠️ El path del modelo por defecto está
**hardcodeado** en `PluginProcessor.cpp` (temporal del spike; lo reemplaza el buscador tone3000 en Fase 2).

Render offline (Milestone 0): `build/namcore/tools/render <model.nam> <in.wav> [out.wav]`.

## Plan por fases

- **Fase 0 — Concepto + UI** *(actual)*: flujo (login → buscar/filtrar → cargar tono →
  cadena de rig → tocar), mockups, alcance v1.
- **Fase 1 — Spike técnico:** prototipo mínimo que cargue un `.nam` y procese audio en vivo
  desde la interfaz de audio → validar motor y latencia.
- **Fase 2 — Integración tone3000:** API OAuth, buscador, descarga/cache local.
- **Fase 3 — Desktop completa:** cadena multibloque + presets, standalone + plugin.
- **Fase 4 — Móvil:** reusar core C++, reescribir I/O+UI, iOS (AUv3) primero, luego Android.

## Diferenciador del producto

tone3000 **ya** tiene buscador web + Web Player + "Live Input" (tocar en el navegador).
El valor de esta app NO es "buscador + reproductor" (eso ya existe gratis), sino la
**capa de rig/pedalboard LOCAL de baja latencia y offline**: cadena multibloque editable,
presets/setlists, y tocar/grabar en serio con tu interfaz de audio.

## Referencias clave

- NAM (proyecto): https://www.neuralampmodeler.com/
- NAM Core (C++, MIT): https://github.com/sdatkinson/NeuralAmpModelerCore
- Plugin oficial (referencia de arquitectura): https://github.com/sdatkinson/NeuralAmpModelerPlugin
- NeuralAudio (wrapper alto nivel, MIT): https://github.com/mikeoliphant/NeuralAudio
- Formato `.nam`: https://neural-amp-modeler.readthedocs.io/en/latest/model-file.html
- NAM Architecture 2 (A2): https://www.tone3000.com/blog/introducing-neural-amp-modeler-nam-architecture-2-a2
- API tone3000: https://www.tone3000.com/api  ·  demos: https://github.com/tone-3000/api
- ToS tone3000: https://www.tone3000.com/terms
- JUCE: https://juce.com/  ·  https://github.com/juce-framework/JUCE

## Notas para futuras sesiones

- Idioma del usuario: **español**. Responder en español (términos técnicos en inglés OK).
- **UI:** NO usar la skill `frontend-design`. La dirección visual se basa en herramientas de
  audio reales (referencias de GitHub), no en una identidad de marca inventada. La UI debe ir
  **desacoplada del motor de audio** para poder migrar a una interfaz más personalizada luego.
- Investigación completa de factibilidad (con fuentes verificadas): workflow run
  `wf_cf6cb6c8-e10`.
- Este archivo es la fuente de verdad del proyecto; actualizarlo cuando cambien decisiones.
