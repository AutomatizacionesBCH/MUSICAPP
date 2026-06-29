cmake_minimum_required(VERSION 3.24)   # 3.24+ para $<LINK_LIBRARY:WHOLE_ARCHIVE,...>
project(MusicApp VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "macOS deployment target")

# --------------------------------------------------------------------------
# WebView2 SDK (Windows): JUCE no lo incluye. Se descarga el NuGet a .webview2/
# (ver platform/windows/build.ps1). El include va ANTES de crear los targets de
# JUCE para que juce_gui_extra (que incluye <WebView2.h>) lo herede.
# --------------------------------------------------------------------------
set(WEBVIEW2_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.webview2/build/native")
if (WIN32 AND EXISTS "${WEBVIEW2_DIR}/include/WebView2.h")
  include_directories("${WEBVIEW2_DIR}/include")
endif()

# --------------------------------------------------------------------------
# JUCE (descargado automáticamente; primera vez tarda)
# --------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
  JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.14
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

# --------------------------------------------------------------------------
# Assets de la UI WebView (HTML/CSS/JS) + la librería JS frontend de JUCE,
# embebidos con binary data. La JS de JUCE se copia a ui/juce/ en configure.
# --------------------------------------------------------------------------
file(COPY
  "${juce_SOURCE_DIR}/modules/juce_gui_extra/native/javascript/index.js"
  "${juce_SOURCE_DIR}/modules/juce_gui_extra/native/javascript/check_native_interop.js"
  DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/ui/juce")

juce_add_binary_data(MusicAppAssets SOURCES
  ui/index.html
  ui/style.css
  ui/app.js
  ui/juce/index.js
  ui/juce/check_native_interop.js)

# --------------------------------------------------------------------------
# NeuralAmpModelerCore como librería estática propia.
# Las arquitecturas (WaveNet/LSTM/…) se auto-registran con objetos estáticos a
# nivel de archivo. Para que el linker NO los descarte hay que enlazar nam_core
# con WHOLE_ARCHIVE en el EJECUTABLE FINAL (ver más abajo). Sin esto, get_dsp
# falla con "No config parser registered for architecture: WaveNet".
# --------------------------------------------------------------------------
set(NAM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/NeuralAmpModelerCore")
file(GLOB NAM_SOURCES CONFIGURE_DEPENDS
  "${NAM_DIR}/NAM/*.cpp"
  "${NAM_DIR}/NAM/*/*.cpp")

add_library(nam_core STATIC ${NAM_SOURCES})
target_compile_features(nam_core PUBLIC cxx_std_20)
target_include_directories(nam_core PUBLIC
  "${NAM_DIR}"
  "${NAM_DIR}/Dependencies/eigen"
  "${NAM_DIR}/Dependencies/nlohmann")
target_compile_definitions(nam_core PUBLIC NAM_ENABLE_A2_FAST EIGEN_MPL2_ONLY)
if (APPLE)
  target_compile_options(nam_core PRIVATE -stdlib=libc++)
endif()
if (MSVC)
  # Windows/MSVC: /bigobj (Eigen+NAM templates generan objetos enormes),
  # /utf-8 (literales con acentos en UTF-8), y _USE_MATH_DEFINES (M_PI desde <cmath>).
  target_compile_options(nam_core PRIVATE /bigobj /utf-8)
  target_compile_definitions(nam_core PRIVATE _USE_MATH_DEFINES)
endif()

# --------------------------------------------------------------------------
# App (plugin con formato Standalone)
# --------------------------------------------------------------------------
juce_add_plugin(MusicApp
  PRODUCT_NAME "Music App"
  COMPANY_NAME "BCH"
  BUNDLE_ID com.bch.musicapp
  FORMATS Standalone
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  PLUGIN_MANUFACTURER_CODE Bchh
  PLUGIN_CODE Mapp
  MICROPHONE_PERMISSION_ENABLED TRUE
  MICROPHONE_PERMISSION_TEXT "Music App necesita acceso a tu entrada de audio (guitarra/interfaz)."
)

juce_generate_juce_header(MusicApp)

target_sources(MusicApp PRIVATE
  src/PluginProcessor.cpp
  src/PluginEditor.cpp
  src/ModelLibrary.cpp
  src/ModelBrowser.cpp
  src/WebUIEditor.cpp
)

target_compile_features(MusicApp PRIVATE cxx_std_20)

if (MSVC)
  target_compile_options(MusicApp PRIVATE /bigobj /utf-8)
  target_compile_definitions(MusicApp PRIVATE _USE_MATH_DEFINES)
endif()

# MusicApp compila PluginProcessor.cpp (incluye headers de NAM) → necesita los includes/defines.
# Los símbolos de NAM se resuelven en el link final contra nam_core.
target_include_directories(MusicApp PRIVATE
  "${NAM_DIR}"
  "${NAM_DIR}/Dependencies/eigen"
  "${NAM_DIR}/Dependencies/nlohmann")

target_compile_definitions(MusicApp PRIVATE
  NAM_ENABLE_A2_FAST
  EIGEN_MPL2_ONLY
  JUCE_WEB_BROWSER=1            # UI definitiva en WebView (Helix/AmpliTube style)
  JUCE_USE_WIN_WEBVIEW2=1       # backend Edge/Chromium en Windows
  JUCE_USE_CURL=0
  DONT_SET_USING_JUCE_NAMESPACE=1
)

target_link_libraries(MusicApp
  PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp          # juce::dsp::Convolution para el Cabinet IR
    juce::juce_gui_extra    # juce::WebBrowserComponent (UI WebView)
    MusicAppAssets          # HTML/CSS/JS embebidos (binary data)
  PUBLIC
    juce::juce_recommended_config_flags
)

# Clave: WHOLE_ARCHIVE de nam_core en el ejecutable final → incluye TODOS los .o
# (incluidos los registradores estáticos de WaveNet/LSTM/ConvNet/Linear).
if (TARGET MusicApp_Standalone)
  target_link_libraries(MusicApp_Standalone PRIVATE "$<LINK_LIBRARY:WHOLE_ARCHIVE,nam_core>")
  # JUCE carga WebView2Loader.dll en runtime (LoadLibrary). Hay que copiarla junto
  # al ejecutable; si falta, JUCE cae al backend IE (resource provider no funciona).
  if (WIN32 AND EXISTS "${WEBVIEW2_DIR}/x64/WebView2Loader.dll")
    add_custom_command(TARGET MusicApp_Standalone POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${WEBVIEW2_DIR}/x64/WebView2Loader.dll"
        "$<TARGET_FILE_DIR:MusicApp_Standalone>")
  endif()
endif()
