cmake_minimum_required(VERSION 3.21)
project(WolfsDen VERSION 3.5 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(WOLFS_DEN_PRODUCTS_DIR "${CMAKE_SOURCE_DIR}/Products" CACHE PATH "Howling Wolves: VST3, AU, Standalone (and AAX if built) — all under apex_project/Products")

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

juce_add_binary_data(
    WolfsDenFonts
    HEADER_NAME WolfsDenFonts.h
    NAMESPACE WolfsDenFonts
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Fonts/InterVariable.ttf"
)

juce_add_plugin(
    WolfsDen
    COMPANY_NAME "Wolf Productions"
    BUNDLE_ID com.wolfproductions.wolfsden
    PLUGIN_MANUFACTURER_CODE WLFD
    PLUGIN_CODE WfDn
    VERSION ${PROJECT_VERSION}
    FORMATS ${WOLFS_DEN_PLUGIN_FORMATS}
    PRODUCT_NAME "Howling Wolves"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
)

message(
    STATUS
    "Howling Wolves: run cmake --build with target WolfsDen_ship (or WolfsDen_VST3). "
    "Target WolfsDen is JUCE shared code only — it does not produce or redeploy the .vst3."
)

# One deploy step per format: merge DistributionContent into the built bundle, then copy the *complete*
# bundle to apex_project/Products/ AND to ~/Library/Audio/Plug-Ins/ (same bits in both places).
# JUCE COPY_PLUGIN_AFTER_BUILD still runs first; this overwrites those installs with the final bundle.
set(WOLFS_DEN_DEPLOY_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/cmake/DeployWolfsDenPlugin.cmake")
set(WOLFS_DEN_FACTORY_SAMPLES "${CMAKE_CURRENT_SOURCE_DIR}/DistributionContent")

if(APPLE)
    set(WOLFS_DEN_CLEAN_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CleanOldPlugins.cmake")
    if(TARGET WolfsDen_VST3)
        add_custom_command(
            TARGET WolfsDen_VST3
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${WOLFS_DEN_PRODUCTS_DIR}"
            # Remove any old versioned VST3 bundles (e.g. "Howling Wolves 3.2.vst3") that
            # share the same plugin code and would cause CoreAudio to load the wrong binary.
            COMMAND ${CMAKE_COMMAND}
                "-DSEARCH_DIR=$ENV{HOME}/Library/Audio/Plug-Ins/VST3"
                "-DPLUGIN_NAME=Howling Wolves"
                "-DEXT=vst3"
                -P "${WOLFS_DEN_CLEAN_SCRIPT}"
            COMMAND ${CMAKE_COMMAND}
                "-DSEARCH_DIR=${WOLFS_DEN_PRODUCTS_DIR}"
                "-DPLUGIN_NAME=Howling Wolves"
                "-DEXT=vst3"
                -P "${WOLFS_DEN_CLEAN_SCRIPT}"
            COMMAND
                ${CMAKE_COMMAND}
                "-DSRC=$<TARGET_BUNDLE_DIR:WolfsDen_VST3>"
                "-DDST_PRODUCTS=${WOLFS_DEN_PRODUCTS_DIR}/Howling Wolves.vst3"
                "-DDST_USER=$ENV{HOME}/Library/Audio/Plug-Ins/VST3/Howling Wolves.vst3"
                "-DFACTORY=${WOLFS_DEN_FACTORY_SAMPLES}"
                -P "${WOLFS_DEN_DEPLOY_SCRIPT}"
            COMMENT "Wolf's Den: VST3 -> Products/ + ~/Library/Audio/Plug-Ins/VST3/"
            VERBATIM
        )
    endif()
    if(TARGET WolfsDen_AU)
        add_custom_command(
            TARGET WolfsDen_AU
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${WOLFS_DEN_PRODUCTS_DIR}"
            # Remove any old versioned AU bundles (e.g. "Howling Wolves 3.2.component") that
            # share the same plugin code and would cause Logic Pro to load the wrong binary.
            COMMAND ${CMAKE_COMMAND}
                "-DSEARCH_DIR=$ENV{HOME}/Library/Audio/Plug-Ins/Components"
                "-DPLUGIN_NAME=Howling Wolves"
                "-DEXT=component"
                -P "${WOLFS_DEN_CLEAN_SCRIPT}"
            COMMAND ${CMAKE_COMMAND}
                "-DSEARCH_DIR=${WOLFS_DEN_PRODUCTS_DIR}"
                "-DPLUGIN_NAME=Howling Wolves"
                "-DEXT=component"
                -P "${WOLFS_DEN_CLEAN_SCRIPT}"
            COMMAND
                ${CMAKE_COMMAND}
                "-DSRC=$<TARGET_BUNDLE_DIR:WolfsDen_AU>"
                "-DDST_PRODUCTS=${WOLFS_DEN_PRODUCTS_DIR}/Howling Wolves.component"
                "-DDST_USER=$ENV{HOME}/Library/Audio/Plug-Ins/Components/Howling Wolves.component"
                "-DFACTORY=${WOLFS_DEN_FACTORY_SAMPLES}"
                -P "${WOLFS_DEN_DEPLOY_SCRIPT}"
            COMMENT "Wolf's Den: AU -> Products/ + ~/Library/Audio/Plug-Ins/Components/"
            VERBATIM
        )
    endif()
    if(TARGET WolfsDen_Standalone)
        add_custom_command(
            TARGET WolfsDen_Standalone
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${WOLFS_DEN_PRODUCTS_DIR}"
            COMMAND
                ${CMAKE_COMMAND}
                "-DSRC=$<TARGET_BUNDLE_DIR:WolfsDen_Standalone>"
                "-DDST_PRODUCTS=${WOLFS_DEN_PRODUCTS_DIR}/Howling Wolves.app"
                "-DDST_USER="
                "-DFACTORY=${WOLFS_DEN_FACTORY_SAMPLES}"
                -P "${WOLFS_DEN_DEPLOY_SCRIPT}"
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
                ${CMAKE_COMMAND}
                "-DSRC=$<TARGET_BUNDLE_DIR:WolfsDen_AAX>"
                "-DDST_PRODUCTS=${WOLFS_DEN_PRODUCTS_DIR}/WolfsDen.aaxplugin"
                "-DDST_USER="
                "-DFACTORY=${WOLFS_DEN_FACTORY_SAMPLES}"
                -P "${WOLFS_DEN_DEPLOY_SCRIPT}"
            COMMENT "Wolf's Den: AAX -> Products/"
            VERBATIM
        )
    endif()
endif()

# -----------------------------------------------------------------------------
# Ship target — IDE / CMake Tools often default to building "WolfsDen", which is
# JUCE *shared code only* (no .vst3 / .component). Deploy POST_BUILD hooks live on
# WolfsDen_VST3 / WolfsDen_AU / WolfsDen_Standalone. Building those (or this) is
# what actually refreshes ~/Library and Products/.
# -----------------------------------------------------------------------------
set(WOLFS_DEN_SHIP_DEPS "")
if(TARGET WolfsDen_VST3)
    list(APPEND WOLFS_DEN_SHIP_DEPS WolfsDen_VST3)
endif()
if(TARGET WolfsDen_AU)
    list(APPEND WOLFS_DEN_SHIP_DEPS WolfsDen_AU)
endif()
if(TARGET WolfsDen_Standalone)
    list(APPEND WOLFS_DEN_SHIP_DEPS WolfsDen_Standalone)
endif()
if(WOLFS_DEN_SHIP_DEPS)
    add_custom_target(
        WolfsDen_ship
        DEPENDS ${WOLFS_DEN_SHIP_DEPS}
        COMMENT "Wolf's Den: ship plug-ins to Products/ + user plug-in folders (build this target)"
    )
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
        Source/engine/samples/WDSampleLibrary.cpp
        Source/engine/samples/WDSamplePlayer.cpp
        Source/engine/samples/SampleKeymap.cpp
        Source/ui/MainComponent.cpp
        Source/ui/PerfXyPad.cpp
        Source/ui/theme/UITheme.cpp
        Source/ui/theme/WolfsDenLookAndFeel.cpp
        Source/ui/chrome/TopBar.cpp
        Source/ui/chrome/BottomBar.cpp
        Source/ui/pages/CompositionPage.cpp
        Source/ui/pages/SynthPage.cpp
        Source/ui/pages/WDSampleBrowserOverlay.cpp
        Source/ui/pages/FxPage.cpp
        Source/ui/pages/ModPage.cpp
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
        WolfsDenFonts
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
    Source/engine/samples/WDSampleLibrary.cpp
    Source/engine/samples/WDSamplePlayer.cpp
    Source/ui/MainComponent.cpp
    Source/ui/PerfXyPad.cpp
    Source/ui/theme/UITheme.cpp
    Source/ui/theme/WolfsDenLookAndFeel.cpp
    Source/ui/chrome/TopBar.cpp
    Source/ui/chrome/BottomBar.cpp
    Source/ui/pages/CompositionPage.cpp
    Source/ui/pages/SynthPage.cpp
    Source/ui/pages/WDSampleBrowserOverlay.cpp
    Source/ui/pages/FxPage.cpp
    Source/ui/pages/ModPage.cpp
)

option(WOLFS_DEN_BUILD_TESTS "Build WolfsDenTests and register CTest" ON)
if(WOLFS_DEN_BUILD_TESTS)
    enable_testing()
    add_subdirectory(Tests)
endif()
