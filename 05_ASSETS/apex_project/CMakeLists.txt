cmake_minimum_required(VERSION 3.21)
project(WolfsDen VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif()

# AAX requires Avid AAX SDK; omit from default build so CI and dev machines compile cleanly.
set(WOLFS_DEN_PLUGIN_FORMATS VST3 AU Standalone)
if(DEFINED ENV{AAX_SDK_PATH})
    file(TO_CMAKE_PATH "$ENV{AAX_SDK_PATH}" _WOLFS_DEN_AAX_SDK)
    if(EXISTS "${_WOLFS_DEN_AAX_SDK}")
        list(APPEND WOLFS_DEN_PLUGIN_FORMATS AAX)
        message(STATUS "Wolf's Den: AAX SDK found — AAX format enabled")
    endif()
endif()

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(
    WolfsDen
    COMPANY_NAME "Wolf Productions"
    BUNDLE_ID com.wolfproductions.wolfsden
    PLUGIN_MANUFACTURER_CODE WLFD
    PLUGIN_CODE WfDn
    FORMATS ${WOLFS_DEN_PLUGIN_FORMATS}
    PRODUCT_NAME "Wolf's Den"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
)

target_sources(
    WolfsDen
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/engine/SynthEngine.cpp
        Source/engine/TheoryEngine.cpp
        Source/engine/MidiPipeline.cpp
        Source/engine/FxEngine.cpp
        Source/ui/MainComponent.cpp
)

target_compile_definitions(
    WolfsDen
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(
    WolfsDen
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/Source" PREFIX "Source" FILES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/engine/SynthEngine.cpp
    Source/engine/TheoryEngine.cpp
    Source/engine/MidiPipeline.cpp
    Source/engine/FxEngine.cpp
    Source/ui/MainComponent.cpp
)
