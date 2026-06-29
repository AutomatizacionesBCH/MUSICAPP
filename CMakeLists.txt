cmake_minimum_required(VERSION 3.22)
project(MusicApp VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "macOS deployment target")

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
# NeuralAmpModelerCore (motor NAM, ya clonado en libs/)
# --------------------------------------------------------------------------
set(NAM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/NeuralAmpModelerCore")
file(GLOB NAM_SOURCES CONFIGURE_DEPENDS
  "${NAM_DIR}/NAM/*.cpp"
  "${NAM_DIR}/NAM/*/*.cpp")

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
  ${NAM_SOURCES}
)

target_compile_features(MusicApp PRIVATE cxx_std_20)

target_include_directories(MusicApp PRIVATE
  "${NAM_DIR}"
  "${NAM_DIR}/Dependencies/eigen"
  "${NAM_DIR}/Dependencies/nlohmann"
)

target_compile_definitions(MusicApp PRIVATE
  NAM_ENABLE_A2_FAST
  EIGEN_MPL2_ONLY
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  DONT_SET_USING_JUCE_NAMESPACE=1
)

target_link_libraries(MusicApp
  PRIVATE
    juce::juce_audio_utils
  PUBLIC
    juce::juce_recommended_config_flags
)
