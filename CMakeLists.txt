cmake_minimum_required(VERSION 3.22)

project(GritDI VERSION 0.1.0)

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
set(GRIT_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND GRIT_FORMATS AU)
endif()

juce_add_plugin(GritDI
    COMPANY_NAME                "Thiago Galvao"
    BUNDLE_ID                   "com.thiagogalvao.gritdi"
    PLUGIN_MANUFACTURER_CODE    Thgo
    PLUGIN_CODE                 Grdi
    PRODUCT_NAME                "Grit DI"
    FORMATS                     ${GRIT_FORMATS}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx Distortion
    AU_MAIN_TYPE                kAudioUnitType_Effect)

target_sources(GritDI PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(GritDI PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(GritDI
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Teste offline do DSP (opcional): cmake -DGRIT_BUILD_TESTS=ON ...
# ---------------------------------------------------------------------------
option(GRIT_BUILD_TESTS "Compila o teste do DSP" OFF)
if(GRIT_BUILD_TESTS)
    juce_add_console_app(GritDSPTest PRODUCT_NAME "GritDSPTest")
    target_sources(GritDSPTest PRIVATE
        Tests/DspTest.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)
    target_compile_definitions(GritDSPTest PRIVATE
        JucePlugin_Name="Grit DI"
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)
    target_link_libraries(GritDSPTest PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags)
endif()
