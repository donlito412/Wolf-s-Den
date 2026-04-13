cmake_minimum_required(VERSION 3.21)

# TASK_002 deliverable: Tests/ hooked to CTest (ADD §9.5 WolfsDenTests pattern; minimal smoke).
enable_testing()

add_executable(WolfsDenTests SmokeTest.cpp)
target_compile_features(WolfsDenTests PRIVATE cxx_std_17)
target_link_libraries(WolfsDenTests PRIVATE juce::juce_core)

add_test(NAME WolfsDen_Smoke COMMAND WolfsDenTests)
