# Model Browser — Design (Milestone 2b, parte 2)

**Date:** 2026-06-29
**Status:** Approved (design)
**Branch:** `feature/model-browser`

## 1. Purpose

Un **explorador de biblioteca de modelos** in-app: una ventana popup con buscador y
lista para navegar y cargar entre los miles de modelos `.nam` descargados, sin usar
el selector de archivos del SO. Escanea una **carpeta de librería configurable**
(default `~/Documents/Music App/models/`, recursivo) y filtra por texto.

Alcance de este ciclo: **solo modelos NAM** (`.nam`). IRs y la UI WebView definitiva
son ciclos siguientes; `ModelLibrary`/`ModelBrowser` se diseñan genéricos para reusar.

## 2. Goals / non-goals

**Goals**
- Escaneo recursivo de una carpeta de librería buscando `.nam`.
- Lista filtrable por substring (case-insensitive) sobre la ruta relativa.
- Cargar el modelo seleccionado vía el `loadNamModel` thread-safe existente.
- Carpeta de librería configurable y **persistida** entre sesiones.

**Non-goals (YAGNI)**
- Selector de IRs (reusar el componente luego).
- Metadatos/columnas (gear/arch como columnas), favoritos, ordenación avanzada.
- Integración con la API de tone3000 (Fase 2).
- Escaneo en background con índice (el escaneo síncrono basta para miles de archivos).

## 3. Componentes (límites claros)

### 3.1 `ModelLibrary` — capa de datos (`src/ModelLibrary.{h,cpp}`)
Sin UI. Responsable de descubrir y filtrar modelos.
- `struct Entry { juce::File file; juce::String relPath; };` (`relPath` = ruta
  relativa a la carpeta de librería, para mostrar y buscar).
- `void setFolder (const juce::File& folder);` — fija la carpeta y re-escanea.
- `juce::File getFolder() const;`
- `void rescan();` — `folder.findChildFiles(findFiles, recursive=true, "*.nam")`,
  ordena por `relPath`, guarda en `mEntries`.
- `juce::Array<Entry> filter (const juce::String& query) const;` — substring
  case-insensitive sobre `relPath`; query vacía devuelve todo.
- `int size() const;`

### 3.2 `ModelBrowser` — UI (`src/ModelBrowser.{h,cpp}`)
`juce::Component` + `juce::ListBoxModel`. Depende de `ModelLibrary`.
- `juce::TextEditor searchBox;` (placeholder "Buscar modelos...").
- `juce::ListBox listBox;` muestra `mFiltered` (resultado de `filter`).
- `juce::TextButton loadButton { "Cargar" }, changeFolderButton { "Cambiar carpeta..." };`
- `juce::Label countLabel;` (p. ej. "1240 modelos").
- `std::function<void(juce::File)> onLoad;` — callback al cargar.
- Búsqueda: `searchBox.onTextChange` → `mFiltered = library.filter(text)` →
  `listBox.updateContent()`.
- Cargar: doble-click en fila o botón **Cargar** → `onLoad(mFiltered[sel].file)`.
- Cambiar carpeta: `FileChooser` (modo directorios) → `library.setFolder` →
  refrescar lista + persistir (callback `onFolderChanged`).

### 3.3 Popup window
`juce::DocumentWindow` (no resizable necesario; ~560×440) que hospeda el
`ModelBrowser`. El `PluginEditor` lo posee (`std::unique_ptr`), lo crea al pulsar
**Modelos...** y lo libera al cerrarse (override de `closeButtonPressed`).

### 3.4 Persistencia
`juce::PropertiesFile` (`Music App/settings.xml` en
`userApplicationDataDirectory`). Clave `modelLibraryFolder`. Al arrancar, el editor
lee la carpeta guardada (o el default) y la pasa a `ModelLibrary`. Al cambiar
carpeta, se reescribe.

### 3.5 Cambios en `PluginEditor`
- Botón **"Modelos..."** junto al label de modelo (se encoge el label para dejar
  espacio). `onClick` abre el popup.
- Al cargar desde el browser: `processorRef.loadNamModel(file)` + actualizar el
  label "Modelo:" (mismo código que `chooseModel`).
- Miembros: `ModelLibrary mLibrary;`, `std::unique_ptr<juce::DocumentWindow> mBrowserWindow;`,
  `std::unique_ptr<juce::PropertiesFile> mSettings;`.

## 4. Data flow

```
[Modelos...] → crea DocumentWindow(ModelBrowser)
   ModelBrowser usa mLibrary (carpeta persistida) → rescan → lista
   teclear "fender" → library.filter("fender") → listBox.updateContent
   doble-click/Cargar → onLoad(file) → editor → processor.loadNamModel(file)
                       → label "Modelo:" actualizado
   [Cambiar carpeta] → FileChooser dir → setFolder → rescan + persistir
```

## 5. Rendimiento / threading

- `findChildFiles(recursive)` sobre miles de archivos tarda ~decenas de ms; se hace
  en el message thread al abrir el browser o cambiar carpeta (aceptable; sin índice).
- `filter()` es O(n) sobre strings en memoria — instantáneo para miles de entradas.
- La carga del modelo usa el staging atómico ya existente (`loadNamModel`): el swap
  ocurre fuera del audio thread; el browser nunca toca el DSP directamente.

## 6. Verificación

Coherente con la metodología del proyecto (build + live):
1. Compila en Windows (`platform\windows\build.ps1`, exit 0).
2. Abrir **Modelos...**, ver la lista de la carpeta de librería.
3. Buscar "fender" → la lista filtra; cargar uno → el label "Modelo:" cambia.
4. **Cambiar carpeta** a la de descargas → reescanea (miles de modelos), persiste;
   reabrir la app conserva la carpeta.
5. Screenshot del browser con resultados filtrados.

## 7. Archivos afectados

- **Nuevos:** `src/ModelLibrary.h`, `src/ModelLibrary.cpp`, `src/ModelBrowser.h`,
  `src/ModelBrowser.cpp`.
- **Editados:** `src/PluginEditor.h` (miembros + botón), `src/PluginEditor.cpp`
  (botón "Modelos...", apertura del popup, carga, persistencia, layout).
- **`CMakeLists.txt`:** añadir los dos `.cpp` nuevos a `target_sources(MusicApp …)`.

## 8. Dependencias

Solo módulos JUCE ya presentes (`juce_gui_basics` para ListBox/TextEditor/
DocumentWindow; `juce_data_structures` para PropertiesFile — incluidos vía
`juce_audio_utils`). Ninguna dependencia externa nueva.
