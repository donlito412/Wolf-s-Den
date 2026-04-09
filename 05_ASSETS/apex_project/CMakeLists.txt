cmake_minimum_required(VERSION 3.21)
project(WolfsDen VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(WOLFS_DEN_PRODUCTS_DIR "${CMAKE_SOURCE_DIR}/Products" CACHE PATH "Wolf's Den: VST3, AU, Standalone (and AAX if built) — all under apex_project/Products")

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif()

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

# Mirror installable artefacts under apex_project/Products for quick testing (JUCE also installs to ~/Library when COPY_PLUGIN_AFTER_BUILD is TRUE).
# cmake -P avoids shell quoting issues (e.g. paths with apostrophes).
set(WOLFS_DEN_COPY_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CopyProductBundle.cmake")
if(APPLE)
    if(TARGET WolfsDen_VST3)
        add_custom_command(
            TARGET WolfsDen_VST3
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${WOLFS_DEN_PRODUCTS_DIR}"
            COMMAND
                ${CMAKE_COMMAND} "-DSRC=$<TARGET_BUNDLE_DIR:WolfsDen_VST3>"
                "-DDST=${WOLFS_DEN_PRODUCTS_DIR}/WolfsDen.vst3" -P
                "${WOLFS_DEN_COPY_SCRIPT}"
            COMMENT "Wolf's Den: VST3 -> Products/"
            VERBATIM
        )
    endif()
    if(TARGET WolfsDen_AU)
        add_custom_command(
            TARGET WolfsDen_AU
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${WOLFS_DEN_PRODUCTS_DIR}"
            COMMAND
                ${CMAKE_COMMAND} "-DSRC=$<TARGET_BUNDLE_DIR:WolfsDen_AU>"
                "-DDST=${WOLFS_DEN_PRODUCTS_DIR}/WolfsDen.component" -P
                "${WOLFS_DEN_COPY_SCRIPT}"
            COMMENT "Wolf's Den: AU -> Products/"
            VERBATIM
        )
    endif()
    if(TARGET WolfsDen_Standalone)
        add_custom_command(
            TARGET WolfsDen_Standalone
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${WOLFS_DEN_PRODUCTS_DIR}"
            COMMAND
                ${CMAKE_COMMAND} "-DSRC=$<TARGET_BUNDLE_DIR:WolfsDen_Standalone>"
                "-DDST=${WOLFS_DEN_PRODUCTS_DIR}/WolfsDen.app" -P
                "${WOLFS_DEN_COPY_SCRIPT}"
            COMMENT "Wolf's Den: Standalone -> Products/"
            VERBATIM
        )
    endif()
    if(TARGET WolfsDen_AAX)
        add_custom_command(
            TARGET WolfsDen_AAX
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${WOLFS_DEN_PRODUCTS_DIR}"
            COMMAND
                ${CMAKE_COMMAND} "-DSRC=$<TARGET_BUNDLE_DIR:WolfsDen_AAX>"
                "-DDST=${WOLFS_DEN_PRODUCTS_DIR}/WolfsDen.aaxplugin" -P
                "${WOLFS_DEN_COPY_SCRIPT}"
            COMMENT "Wolf's Den: AAX -> Products/"
            VERBATIM
        )
    endif()
endif()

target_sources(
    WolfsDen
    PRIVATE
        ThirdParty/sqlite3/sqlite3.c
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/parameters/WolfsDenParameterLayout.cpp
        Source/engine/SynthEngine.cpp
        Source/engine/ModMatrix.cpp
        Source/engine/synth/VoiceLayer.cpp
        Source/engine/TheoryEngine.cpp
        Source/engine/MidiPipeline.cpp
        Source/engine/FxEngine.cpp
        Source/ui/MainComponent.cpp
        Source/ui/PerfXyPad.cpp
)

if(CMAKE_C_COMPILER_ID MATCHES "Clang|AppleClang|GNU")
    set_source_files_properties(ThirdParty/sqlite3/sqlite3.c PROPERTIES COMPILE_OPTIONS "-w")
elseif(MSVC)
    set_source_files_properties(ThirdParty/sqlite3/sqlite3.c PROPERTIES COMPILE_OPTIONS "/W0")
endif()

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
        juce::juce_dsp
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
    Source/parameters/WolfsDenParameterLayout.cpp
    Source/engine/SynthEngine.cpp
    Source/engine/ModMatrix.cpp
    Source/engine/synth/VoiceLayer.cpp
    Source/engine/TheoryEngine.cpp
    Source/engine/MidiPipeline.cpp
    Source/engine/FxEngine.cpp
    Source/ui/MainComponent.cpp
    Source/ui/PerfXyPad.cpp
)
