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
- **Fase 1 — Milestone 1 ✅ CONFIRMADO EN VIVO (2026-06-29):** la app JUCE standalone
  (`build/app/MusicApp_artefacts/Release/Standalone/Music App.app`, JUCE 8.0.14) **procesa la
  guitarra en tiempo real** a través de NAM, vía interfaz **Arturia MiniFuse 2** → suena. ✔️
  Cadena actual: input (suma L+R a mono) → NAM (normalizado por loudness) → **reverb (post-FX)** →
  limitador → output. UI con knobs INPUT/REVERB/OUTPUT y **medidores In/Out** (timer 30 Hz).
- **Fase 1 — Milestone 2a ✅ (2026-06-29):** añadidos medidores In/Out y reverb (`juce::Reverb`).
  Modelo por defecto temporal = `Dunlop Eric Johnson Fuzz 01.nam` (única captura real local; los
  example_models son fixtures rotos). **Siguiente (2b):** Cabinet IR (`juce::dsp::Convolution`, necesita
  un archivo IR — bajar gratis de tone3000), drive pre-amp, y selector de modelos en la UI.
- El catálogo de tonos se integrará vía la **API oficial de tone3000** (Fase 2); por ahora todo local.
- **Port a Windows (en curso, 2026-06-29):** codebase unificado cross-platform (un solo `src/`).
  CMake con bloques MSVC (`/utf-8 /bigobj _USE_MATH_DEFINES`); eliminadas las rutas hardcodeadas de
  macOS — el modelo por defecto y el FileChooser ahora son multiplataforma (`resolveDefaultModel`:
  env var `MUSICAPP_DEFAULT_MODEL` → `~/Documents/Music App/models/*.nam` → passthrough). Nueva
  estructura `platform/{windows,macos}/` con scripts de build. **Pendiente:** compilar/ejecutar en
  Windows (instalando VS2022 + CMake) y validar Eigen/NAM bajo MSVC.
- **Windows — build verificado (2026-06-29):** la app **compila y corre nativa en Windows** (VS2022
  + CMake 4.3.3, flags MSVC), carga el modelo por defecto y renderiza la UI. `main` ya integrado.
- **Milestone 2b — Cabinet IR ✅ (2026-06-29):** etapa de convolución (`juce::dsp::Convolution`,
  mono, normalizada, thread-safe) tras el NAM y antes del reverb. Cadena: `NAM → normGain → [IR si
  irOn] → reverb`. UI: botón **Cargar IR** + toggle **IR** (default OFF, para no doble-cab con
  capturas amp-cab) + label; `resolveDefaultIR()` (`~/Documents/Music App/irs/*.wav`). Verificado en
  Windows (carga un IR de cabinet).
- **Milestone 2b — Selector de modelos ✅ (2026-06-29):** explorador in-app de la librería NAM.
  `ModelLibrary` (datos: escaneo recursivo + filtro por substring) + `ModelBrowser` (UI: búsqueda +
  `ListBox`) en una ventana popup (botón **Modelos...**). Carpeta de librería **configurable y
  persistida** (`PropertiesFile`), default `~/Documents/Music App/models/`. Verificado en Windows con
  2.000+ modelos y búsqueda en vivo ("fender" → filtra). **Siguiente (2b cont.):** drive pre-amp.
- **UI WebView — Ciclo 1, Etapa 1 ✅ (2026-06-29):** la UI definitiva pasa a **JUCE WebView**
  (`WebUIEditor` + `juce::WebBrowserComponent`, backend WebView2). Render de la **cadena de señal**
  `IN → AMP·NAM → CAB·IR → REVERB → OUT` estilo Helix/AmpliTube (HTML/CSS embebidos, resource provider)
  con **fotos** del amp/cab (resueltas de `<modelo>/images/image_*`). **Windows:** necesita el SDK
  WebView2 (NuGet, lo baja `build.ps1` a `.webview2/`) y `WebView2Loader.dll` junto al exe (lo copia
  CMake post-build; sin ella JUCE cae al backend IE).
- **UI WebView — Ciclo 1, Etapa 2 ✅ (2026-06-29):** controles **cableados al motor**. Relays
  `WebSliderRelay` (input/output/reverb) + `WebToggleButtonRelay` (irOn) ↔ APVTS vía
  `WebSliderParameterAttachment`/`WebToggleButtonParameterAttachment`; los knobs HTML son arrastrables.
  Funciones nativas `loadModel`/`loadIR` (JS → abre el `ModelBrowser`/FileChooser; al cargar, el motor
  emite `modelChanged` y la UI refresca nombre+foto). Assets (HTML/CSS/JS + la librería JS de JUCE)
  embebidos con `juce_add_binary_data`. Binding verificado: la UI arranca con los valores reales de los
  params (knob IN a 0 dB/50%, IR OFF). **`createEditor()` ahora devuelve `WebUIEditor`** (la UI nativa
  `PluginEditor` queda como referencia).
- **UI WebView — barra inferior: medidores + afinador ✅ (2026-06-29):** barra inferior estilo
  AmpliTube con **medidores IN/OUT** (verde→ámbar→rojo) y un **afinador** (nota + cents + aguja). El
  motor guarda la entrada en un ring buffer (`getRecentInput`); el `WebUIEditor` (un `juce::Timer` a
  24 Hz) hace **detección de pitch por autocorrelación** y empuja `{in,out,hz}` por el evento `meters`.
  **Siguiente:** grabador, metrónomo, buscador de presets, drive (camino a la completitud tipo AmpliTube).
- **UI WebView — layout completo tipo AmpliTube ✅ (2026-06-29):** la UI se acerca al mockup. **3 zonas:**
  barra superior (logo, PRESET, Guardar/Presets, TONE3000), **buscador de tonos a la izquierda**
  (búsqueda + chips + lista de la librería, funcional vía funciones nativas `listModels`/`loadModelByPath`
  — click carga el modelo), **cadena** central con bloques estilo mockup (IN·GAIN, AMP·NAM con "MODELO
  CARGADO" + foto, CAB·IR con toggle + Cargar IR, POST·REVERB·MIX, OUT·MASTER), y **barra inferior**
  (medidores + afinador). Controles **reales** (sin EQ falsa de amp; NAM es caja negra). Ventana 1180×780.
  **Pulido pendiente:** el extremo derecho de la barra superior se recorta por el viewport DPI; añadir
  drive, presets (guardar/cargar rig), grabador, metrónomo; gráficos de ampli/pedal por bloque.
- **UI WebView — presets ✅ (2026-06-29):** guardar/cargar el rig. Un preset = params normalizados
  (input/output/reverb/irOn) + rutas del modelo NAM y del IR, en JSON en `~/Documents/Music App/presets/`.
  Nombre de preset editable en la barra superior; **Guardar** (función nativa `savePreset`) escribe el
  JSON; **Presets** (`listPresets`) despliega la lista y al elegir (`loadPreset`/`applyPreset`) restaura
  todo (params vía `setValueNotifyingHost` → los relays mueven los knobs; recarga modelo/IR). Guardado
  verificado (escribe JSON correcto con params + rutas). Nota: el **input sintético no llega al WebView2**
  (limitación de automatización; los clicks reales del usuario sí) — verificar features de click en vivo.

### Gotchas de la UI WebView (¡no perder tiempo de nuevo!)
- **`juce_add_binary_data` regenera los assets en *configure*, NO en build.** Tras editar
  `ui/*.html|css|js` hay que **reconfigurar** (`cmake -S . -B build/app …`) o el HTML/CSS/JS embebido
  queda viejo. `build.ps1` siempre reconfigura, así que por ahí no falla. Para iterar rápido: borrar
  `build/app/**/BinaryData*.cpp` + reconfigurar fuerza la regeneración.
- **El viewport del WebView no tiene altura definida** → `height:100%`, `100vh` y `position:fixed`
  (bottom) NO posicionan bien (la barra se empuja fuera de la vista). **Usar flujo normal del documento**
  (la barra fluye tras la cadena); evitar layouts dependientes de la altura del viewport.
- WebView2 **cachea** los recursos del provider (carpeta de user data en `%TEMP%/MusicAppWebView2`).
  Al cambiar assets, borrar esa carpeta para forzar recarga limpia al probar.

### Receta de integración de NAM Core (verificada al compilar)
- Fuentes a compilar: `libs/NeuralAmpModelerCore/NAM/*.cpp` + `NAM/*/*.cpp` (incluye `wavenet/`).
- Includes: la raíz del repo, `Dependencies/eigen`, `Dependencies/nlohmann`.
- C++20; en macOS `-stdlib=libc++`. Definir `NAM_ENABLE_A2_FAST` (fast-path A2, ON por defecto).
- API en vivo: `auto m = nam::get_dsp(path)` (fuera del audio thread) → `m->Reset(sampleRate, maxBlock)`
  → en el callback `m->process(...)`. NAM trabaja en **double**; convertir float↔double fuera del hot path.
- ⚠️ **Registro estático (bug ya resuelto):** las arquitecturas (WaveNet/LSTM/…) se auto-registran con
  objetos estáticos a nivel de archivo (`wavenet/model.cpp`: `static ConfigParserHelper _register_WaveNet`).
  Si NAM se enlaza como `.a` normal, el linker los descarta y `get_dsp` falla con *"No config parser
  registered for architecture: WaveNet"*. **Fix:** compilar NAM como `add_library(nam_core STATIC …)` y
  enlazarla con `$<LINK_LIBRARY:WHOLE_ARCHIVE,nam_core>` en el **ejecutable final** (`MusicApp_Standalone`),
  NO en la lib intermedia de JUCE. (La tool `render` no tenía el bug porque compila NAM directo al exe.)
- **Standalone como amp sim:** JUCE silencia la entrada por defecto ("muted to avoid feedback loop").
  Desmutear en el editor: `juce::StandalonePluginHolder::getInstance()->getMuteInputValue().setValue(false)`.
- **Encoding:** `juce::String(const char*)` mostró mojibake (`Â·`) con UTF-8; usar ASCII o `String::fromUTF8`.
- ⚠️ **Niveles de salida (bug que saturaba la interfaz, ya resuelto):** los `example_models` de NAM
  Core son **fixtures de prueba sin calibrar** — `wavenet_a2_max.nam` saca **~10× el máximo** con
  entrada normal (su `GetLoudness()` reporta -20 dB pero el pico real es +20 dB → metadata inservible).
  Eso mandaba ±10 a la interfaz → medidores congelados al máximo y sin sonido (NO daña el hardware,
  es clipping). **Usar modelos REALES** (capturas de tone3000; las Dunlop del usuario dan pico ~0.49).
  Fixes en el processor: (1) **normalizar la salida por loudness** — `if (HasLoudness()) gain =
  10^((-18 - GetLoudness())/20)`; (2) **limitador de seguridad** en `processBlock`: NaN/Inf→0 y
  `jlimit(-1,1)` para no mandar nunca una señal descontrolada; (3) **sumar todas las entradas a mono**
  (la guitarra puede estar en input 1/L o 2/R). El modelo por defecto es ahora un `.nam` real (path
  hardcodeado temporal; lo reemplaza el buscador tone3000 en Fase 2).
- Toolchain: CMake 3.30.5 (binario oficial) en **`~/.local/cmake-3.30.5`** con symlink en `~/.local/bin/cmake`
  (y `/usr/local/bin/cmake`). ⚠️ NO instalar en el scratchpad (se limpia entre sesiones y rompe el build).
  `brew install cmake` compila desde fuente en este Mac (lentísimo) → usar el binario precompilado.

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

**Codebase ÚNICO y compartido entre plataformas** (es para lo que sirve JUCE). El
mismo `src/` compila en Windows y macOS; lo específico de cada SO vive en `platform/`.

```
Music App/
├─ CLAUDE.md                   ← fuente de verdad (este archivo)
├─ README.md                   ← overview para GitHub
├─ CMakeLists.txt              ← build COMPARTIDO: JUCE 8.0.14 (FetchContent) + NAM Core + standalone
│                                 (bloques if(APPLE)/if(MSVC) para lo específico de SO)
├─ .gitignore
├─ src/                        ← CÓDIGO COMPARTIDO (un solo codebase, sin duplicar por SO)
│  ├─ PluginProcessor.{h,cpp}  ← motor headless: input → NAM → reverb → output (estado en APVTS)
│  └─ PluginEditor.{h,cpp}     ← UI del spike (gains, cargar .nam, preferencias de audio)
├─ platform/                   ← SOLO lo específico de cada plataforma (no duplica código)
│  ├─ windows/  build.ps1 + README.md   (MSVC, ASIO, packaging)
│  └─ macos/    build.sh  + README.md   (clang, entitlements, packaging)
├─ libs/
│  └─ NeuralAmpModelerCore/    ← submódulo git (NAM MIT) + Eigen/nlohmann/AudioDSPTools (anidados)
├─ design/mockups/main-screen.html  ← mockup de la UI definitiva (semilla de la futura WebView)
└─ build/                      ← artefactos (gitignored)
```

## Flujo multiplataforma (git) — clave

**No se duplica el código por plataforma.** La "sincronización" entre Windows y macOS
(y luego Android/iOS) **es git**, no copiar carpetas:

1. Desarrollas en **Windows** → `git commit` → `git push`.
2. En el **Mac** → `git pull` → el MISMO código compila nativo (es cross-platform).
3. Lo específico de cada SO se aísla con `if(WIN32)/if(APPLE)` en CMake y
   `#if JUCE_WINDOWS/JUCE_MAC` en código, dentro de `platform/`.
4. Trabajo de port/experimental → rama `windows-port` (o `feature/*`) → merge a `main`.

> Móvil (futuro): se reutiliza el core C++ de `src/`; se añade `platform/ios` y
> `platform/android` con el I/O de audio + UI de cada uno. El core NO se reescribe.

## Cómo construir y ejecutar

**Windows** (VS 2022 con "Desktop development with C++" + CMake):
```powershell
git clone --recursive https://github.com/AutomatizacionesBCH/MUSICAPP.git
powershell -ExecutionPolicy Bypass -File platform\windows\build.ps1
# -> build\app\MusicApp_artefacts\Release\Standalone\Music App.exe
```

**macOS** (CMake + Command Line Tools):
```bash
git clone --recursive https://github.com/AutomatizacionesBCH/MUSICAPP.git
platform/macos/build.sh
# -> open "build/app/MusicApp_artefacts/Release/Standalone/Music App.app"
```

La 1ª vez se descarga JUCE (FetchContent) → tarda. Al ejecutar: conectar la interfaz
→ **Preferencias de audio** (entrada / 48 kHz / buffer 128) → tocar en vivo.

**Modelo por defecto al arrancar** (multiplataforma, sin rutas hardcodeadas — resuelto en
`PluginProcessor.cpp::resolveDefaultModel`): `MUSICAPP_DEFAULT_MODEL` (env var) →
primer `.nam` en `~/Documents/Music App/models/` → si no hay, *passthrough*. Lo reemplaza
el buscador tone3000 en Fase 2.

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
