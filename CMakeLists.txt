cmake_minimum_required(VERSION 3.22)

project(AuraBass VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Versão mínima do macOS")

# Universal binary (Apple Silicon + Intel) no Mac
if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
endif()

# ---------------------------------------------------------------------------
# JUCE: use uma cópia local (-DJUCE_DIR=/caminho/JUCE) ou baixe automaticamente
# ---------------------------------------------------------------------------
set(JUCE_DIR "" CACHE PATH "Pasta do JUCE (opcional)")
if(JUCE_DIR)
    add_subdirectory(${JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    ON)
    FetchContent_MakeAvailable(JUCE)
endif()

# Para AAX: baixe o AAX SDK (developer.avid.com), chame
# juce_set_aax_sdk_path(/caminho/aax-sdk) e adicione AAX em FORMATS.
set(AURA_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND AURA_FORMATS AU)
endif()

juce_add_plugin(AuraBass
    COMPANY_NAME                "Thiago Galvao"
    BUNDLE_ID                   "com.thiagogalvao.aurabass"
    PLUGIN_MANUFACTURER_CODE    Thgo
    PLUGIN_CODE                 Grdi
    PRODUCT_NAME                "AURA Bass"
    FORMATS                     ${AURA_FORMATS}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx Distortion
    AU_MAIN_TYPE                kAudioUnitType_Effect)

# Imagens da interface (montadas por Tools/build_ui_assets.py a partir de Resources/mockup)
juce_add_binary_data(AuraAssets SOURCES
    Resources/ui/background.jpg
    Resources/ui/pointer.png
    Resources/ui/led_drive_on.png
    Resources/ui/led_cab_on.png
    Resources/ui/cab_off.png
    Resources/ui/footswitch_down.png
    Resources/ui/arcs_lit.png
    Resources/fonts/Roboto.ttf
    Resources/ir/cab_4x10.wav
    Resources/ir/cab_1x15.wav)

target_sources(AuraBass PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(AuraBass PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(AuraBass
    PRIVATE
        AuraAssets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Teste offline do DSP (opcional): cmake -DAURA_BUILD_TESTS=ON ...
# ---------------------------------------------------------------------------
option(AURA_BUILD_TESTS "Compila o teste do DSP" OFF)
if(AURA_BUILD_TESTS)
    juce_add_console_app(AuraDSPTest PRODUCT_NAME "AuraDSPTest")
    target_sources(AuraDSPTest PRIVATE
        Tests/DspTest.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)
    target_compile_definitions(AuraDSPTest PRIVATE
        JucePlugin_Name="AURA Bass"
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)
    target_link_libraries(AuraDSPTest PRIVATE
        AuraAssets
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags)
endif()

# ---------------------------------------------------------------------------
# Renderizador do footswitch (opcional): cmake -DAURA_BUILD_TOOLS=ON ...
# A imagem gerada fica versionada em Resources/mockup, então não é preciso rodá-lo.
# ---------------------------------------------------------------------------
option(AURA_BUILD_TOOLS "Compila o renderizador do footswitch" OFF)
if(AURA_BUILD_TOOLS)
    juce_add_console_app(FootswitchRender PRODUCT_NAME "FootswitchRender")
    target_sources(FootswitchRender PRIVATE Tools/FootswitchRender/Main.cpp)
    target_compile_definitions(FootswitchRender PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(FootswitchRender PRIVATE juce::juce_graphics juce::juce_recommended_config_flags)
endif()
