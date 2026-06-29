# WebView UI — Cadena de señal con fotos (Ciclo 1)

**Date:** 2026-06-29
**Status:** Approved (design)
**Branch:** `feature/webview-ui`

## 1. Purpose

Primera versión de la **UI definitiva** estilo AmpliTube/Helix: la **cadena de señal**
visible como bloques horizontales con **fotos de cada componente**, en **JUCE WebView**
(HTML/CSS/JS), desacoplada del motor. Cablea los controles que ya existen y muestra la
foto del amp/cab cargado (de las imágenes de tone3000 que viven junto al modelo).

Decomposición: este es el **Ciclo 1** (fundacional). Buscador in-WebView, bloque drive,
reordenar, presets y OAuth tone3000 son ciclos siguientes.

## 2. Arquitectura

`juce::WebBrowserComponent` reemplaza al editor nativo. La UI vive en HTML/CSS/JS
servidos por un **resource provider**; habla con el motor solo por **relays** (APVTS) y
**funciones nativas** (regla de oro del desacople). El motor no se toca.

Implementación en **2 etapas**:
- **Etapa 1 — Visual:** WebView + resource provider sirve `index.html`/`style.css`/
  `app.js` + **fotos** (`/amp-photo`, `/cab-photo`). Render de la cadena
  `IN → AMP·NAM → CAB·IR → REVERB → OUT` con fotos y nombres. Sin binding aún.
- **Etapa 2 — Controles:** `WebSliderRelay` (input/output/reverb) + `WebToggleButtonRelay`
  (irOn) ↔ APVTS, y función nativa `loadModel`/`loadIR` que abre el `ModelBrowser`
  existente y refresca nombre + foto.

## 3. Componentes

- **`WebUIEditor`** (`src/WebUIEditor.{h,cpp}`) — `AudioProcessorEditor` que hospeda el
  `WebBrowserComponent`. Construye Options (backend WebView2 en Windows,
  native-integration, resource provider, relays, native functions). Resuelve y sirve
  las fotos. Posee los relays + `WebSliderParameterAttachment`/`WebToggleButtonParameterAttachment`.
- **Assets** (`ui/index.html`, `ui/style.css`, `ui/app.js`) — embebidos como bytes en C++
  (cabecera generada o literales) y servidos por el resource provider.
- **Foto:** dado el `File` del modelo/IR cargado, buscar `parentDir/images/image_*.{jpg,png,webp}`;
  el provider devuelve esos bytes en `/amp-photo` y `/cab-photo`; sin foto → placeholder.
- Se conservan `ModelLibrary`/`ModelBrowser` (popup nativo reusado para cargar).

## 4. Mapeo de controles (params REALES, no los aspiracionales del mockup)

El NAM es caja negra (sin EQ). Los bloques exponen solo lo que el motor tiene:
- **IN**: knob `inputGain` · **AMP·NAM**: foto + nombre + *Cambiar* (loadModel) ·
  **CAB·IR**: foto + nombre + toggle `irOn` + *Cambiar* (loadIR) · **REVERB**: knob
  `reverbMix` · **OUT**: knob `outputGain`.

## 5. Build

`JUCE_WEB_BROWSER=1`, enlazar `juce::juce_gui_extra`. Windows usa **WebView2** (presente
en Win11). Assets embebidos vía `juce_add_binary_data` (o cabecera de bytes).

## 6. Verificación

1. Compila en Windows (exit 0).
2. La app abre la WebView con la cadena renderizada + la **foto del amp** cargado.
3. (Etapa 2) Mover un knob en la UI cambia el parámetro (medidores/sonido responden);
   el toggle IR activa/desactiva el cabinet; *Cambiar* abre el browser y actualiza foto/nombre.
4. Screenshot de la cadena.

## 7. Riesgos

- API de JUCE 8 WebView (relays, JS frontend lib, resource provider): territorio nuevo →
  implementación incremental, verificando cada build. La Etapa 1 (estática) de-riesga
  WebView2 + provider antes de añadir el binding.
- WebView2 runtime: si faltara en alguna máquina, la UI no carga (Win11 lo trae).

## 8. Archivos

- Nuevos: `src/WebUIEditor.h/.cpp`, `ui/index.html`, `ui/style.css`, `ui/app.js`.
- Editados: `CMakeLists.txt` (WebView flags + gui_extra + binary data), `PluginProcessor.cpp`
  (`createEditor()` devuelve `WebUIEditor`).
- Se conservan `PluginEditor.*` (referencia) y `ModelBrowser`/`ModelLibrary`.
