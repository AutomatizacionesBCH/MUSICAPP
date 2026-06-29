# Music App 🎸

Herramienta personal para músicos: una app **nativa de escritorio** (luego móvil) que procesa la
señal de tu guitarra/bajo **en tiempo real** a través de modelos **NAM (Neural Amp Modeler)**, IRs
de cabinet y efectos — con un buscador de tonos conectado a [tone3000.com](https://www.tone3000.com).
Una especie de AmpliTube, pero usando el ecosistema **abierto** de NAM.

> **Estado:** en desarrollo temprano (spike). El motor de audio ya funciona y suena; la app
> standalone JUCE ya compila. Ver [`CLAUDE.md`](CLAUDE.md) para el detalle completo del proyecto.

## Qué hace (y qué hará)

- ✅ Carga y reproduce modelos `.nam` (arquitecturas A1 y A2) con `NeuralAmpModelerCore`.
- ✅ App standalone JUCE con **preferencias de audio** (interfaz externa, sample rate, buffer).
- 🔜 Cadena de rig: pre-FX → amp (NAM) → cabinet (IR) → post-FX, reordenable.
- 🔜 Buscador de tonos vía la **API oficial de tone3000** (sin scraping, bajo tu cuenta).
- 🔜 Presets/setlists e interfaz personalizada (UI en WebView).
- 🔜 Versión móvil (iOS AUv3 / Android).

## Stack

- **C++ / [JUCE 8](https://juce.com)** — app standalone (+ plugin a futuro), audio en tiempo real.
- **[NeuralAmpModelerCore](https://github.com/sdatkinson/NeuralAmpModelerCore)** (MIT) — motor de inferencia NAM.
- Arquitectura **desacoplada**: motor de audio headless ↔ UI reemplazable (ver `CLAUDE.md`).

## Construir (macOS)

Requiere CMake y las Command Line Tools de Xcode.

```bash
cmake -S . -B build/app -DCMAKE_BUILD_TYPE=Release        # 1ª vez descarga JUCE
cmake --build build/app --target MusicApp_Standalone -j4
open "build/app/MusicApp_artefacts/Release/Standalone/Music App.app"
```

## Licencias y aviso legal

- Proyecto **personal, no comercial**. Software NAM y `NeuralAmpModelerCore`: **MIT**. JUCE: AGPLv3.
- **No** se redistribuyen archivos `.nam` de terceros: el contenido de tone3000 se accede mediante
  su API oficial bajo la cuenta de cada usuario.
- Todas las marcas de amplificadores (Fender, Marshall, etc.) pertenecen a sus respectivos dueños;
  cualquier referencia es descriptiva y no implica afiliación.

## Créditos

Construido sobre el trabajo open source de Steven Atkinson (NAM) y la comunidad NAM / tone3000.
