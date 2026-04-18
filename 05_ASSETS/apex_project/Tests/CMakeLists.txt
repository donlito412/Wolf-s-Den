cmake_minimum_required(VERSION 3.21)

# TASK_002 deliverable: Tests/ hooked to CTest (ADD §9.5 WolfsDenTests pattern; minimal smoke).
enable_testing()

add_executable(WolfsDenTests SmokeTest.cpp)
target_compile_features(WolfsDenTests PRIVATE cxx_std_17)
target_compile_definitions(WolfsDenTests PRIVATE JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1)
target_include_directories(WolfsDenTests PRIVATE ../Source)

target_link_libraries(WolfsDenTests PRIVATE 
    juce::juce_core 
    juce::juce_events
    juce::juce_audio_basics 
    juce::juce_audio_processors
    juce::juce_dsp
)

target_sources(WolfsDenTests PRIVATE
    ../Source/engine/TheoryEngine.cpp
    ../ThirdParty/sqlite3/sqlite3.c
)

if(CMAKE_C_COMPILER_ID MATCHES "Clang|AppleClang|GNU")
    set_source_files_properties(../ThirdParty/sqlite3/sqlite3.c PROPERTIES COMPILE_OPTIONS "-w")
elseif(MSVC)
    set_source_files_properties(../ThirdParty/sqlite3/sqlite3.c PROPERTIES COMPILE_OPTIONS "/W0")
endif()

add_test(NAME WolfsDen_Theory COMMAND WolfsDenTests)
