cmake_minimum_required(VERSION 3.22)
project(GenreSpectrum VERSION 1.0.0)

# Runtime estatico: no requiere instalar Visual C++ Redistributable
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_CXX_STANDARD 17)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(GenreSpectrum
    COMPANY_NAME "GenreLab"
    PLUGIN_MANUFACTURER_CODE Glab
    PLUGIN_CODE Gspc
    FORMATS VST3
    PRODUCT_NAME "Genre Spectrum"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Analyzer Fx
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(GenreSpectrum PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(GenreSpectrum PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)

target_compile_options(GenreSpectrum PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

target_link_libraries(GenreSpectrum
    PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)
