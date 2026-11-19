cmake_minimum_required(VERSION 3.22)

# Universal binary (Apple Silicon + Intel) when building on a Mac — must be set before project()
if(CMAKE_HOST_APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
endif()

project(NoviSlice VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use a local JUCE checkout if one is given (-DNOVISLICE_JUCE_DIR=...), otherwise fetch JUCE 8.
if(NOVISLICE_JUCE_DIR)
    add_subdirectory(${NOVISLICE_JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

# Formats: VST3 everywhere, plus AU on macOS. Pass -DNOVISLICE_STANDALONE=ON for a test app.
set(NS_FORMATS VST3)
if(APPLE)
    list(APPEND NS_FORMATS AU)
endif()
if(NOVISLICE_STANDALONE)
    list(APPEND NS_FORMATS Standalone)
endif()

juce_add_plugin(NoviSlice
    COMPANY_NAME "Amir Novin"
    BUNDLE_ID "com.amirnovin.novislice"
    PRODUCT_NAME "NoviSlice"
    PLUGIN_MANUFACTURER_CODE Anov
    PLUGIN_CODE NovS
    FORMATS ${NS_FORMATS}
    # An instrument: load a loop, play slices on the white keys from C5 (MIDI 60)
    # and the performance effects on C3-D#4 (MIDI 36-51).
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Sampler
    AU_MAIN_TYPE kAudioUnitType_MusicDevice)

juce_add_binary_data(NoviSliceAssets SOURCES
    Resources/fonts/BarlowCondensed-Bold.ttf
    Resources/fonts/BarlowCondensed-SemiBold.ttf
    Resources/fonts/ShareTechMono-Regular.ttf)

target_sources(NoviSlice PRIVATE
    Source/SliceEngine.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(NoviSlice PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

if(MSVC)
    target_compile_options(NoviSlice PRIVATE /utf-8)
endif()

target_link_libraries(NoviSlice
    PRIVATE
        NoviSliceAssets
        juce::juce_audio_utils
        juce::juce_audio_formats
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Engine unit test (no JUCE needed): cmake --build build --target EngineTest && ./build/EngineTest
add_executable(EngineTest Tests/EngineTest.cpp Source/SliceEngine.cpp)

# Renders the editor to a PNG for checking the layout: -DNOVISLICE_SCREENSHOT=ON
if(NOVISLICE_SCREENSHOT)
    juce_add_console_app(RenderUI PRODUCT_NAME "RenderUI")
    target_sources(RenderUI PRIVATE Tests/RenderUI.cpp Source/SliceEngine.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_compile_definitions(RenderUI PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_USE_MP3AUDIOFORMAT=1
        JucePlugin_Name="NoviSlice" JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(RenderUI PRIVATE NoviSliceAssets juce::juce_audio_utils juce::juce_audio_formats
        juce::juce_recommended_config_flags)
endif()
