cmake_minimum_required(VERSION 3.22)
project(EpsilonExpansion VERSION 0.4.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(EPSILON_UI_TESTS "Build the editor snapshot tool" OFF)

enable_testing()
add_executable(epsilon_suite Tests/suite.cpp)
target_include_directories(epsilon_suite PRIVATE Source ThirdParty)
add_test(NAME epsilon_suite COMMAND epsilon_suite)
add_executable(epsilon_fuzz Tests/fuzz.cpp)
target_include_directories(epsilon_fuzz PRIVATE Source ThirdParty)
add_test(NAME epsilon_fuzz COMMAND epsilon_fuzz)

if(NOT DEFINED JUCE_PATH)
  # The bundled JUCE folder holds only the modules (enough for the Projucer projects).
  # For a CMake plugin build pass a full JUCE 9 checkout: -DJUCE_PATH=/path/to/JUCE
  message(STATUS "JUCE_PATH not set: building the DSP tests only")
  return()
endif()
add_subdirectory("${JUCE_PATH}" "${CMAKE_BINARY_DIR}/JUCE")

add_library(EpsilonCommon STATIC Source/PluginProcessor.cpp Source/PluginEditor.cpp
  Source/UI/Kit.cpp Source/UI/Displays.cpp Source/UI/EmbeddedFonts.cpp)
set_target_properties(EpsilonCommon PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(EpsilonCommon PUBLIC Source ThirdParty)
target_compile_definitions(EpsilonCommon PUBLIC JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(EpsilonCommon PRIVATE juce::juce_audio_utils juce::juce_dsp PUBLIC juce::juce_recommended_config_flags)
target_include_directories(EpsilonCommon PUBLIC "${JUCE_MODULES_DIR}")
# Compile the JUCE module sources once in the common archive instead of once per plugin.
foreach(module juce_audio_utils juce_audio_processors juce_audio_processors_headless juce_audio_basics juce_audio_devices
    juce_audio_formats juce_gui_extra juce_gui_basics juce_graphics juce_events juce_core juce_data_structures juce_dsp)
  get_target_property(module_sources ${module} INTERFACE_SOURCES)
  if(module_sources)
    target_sources(EpsilonCommon PRIVATE ${module_sources})
    set_property(TARGET ${module} PROPERTY INTERFACE_SOURCES "")
  endif()
endforeach()
target_compile_definitions(EpsilonCommon PUBLIC
  JUCE_MODULE_AVAILABLE_juce_audio_utils=1 JUCE_MODULE_AVAILABLE_juce_audio_processors=1
  JUCE_MODULE_AVAILABLE_juce_audio_processors_headless=1 JUCE_MODULE_AVAILABLE_juce_audio_devices=1
  JUCE_MODULE_AVAILABLE_juce_audio_formats=1 JUCE_MODULE_AVAILABLE_juce_audio_basics=1
  JUCE_MODULE_AVAILABLE_juce_core=1 JUCE_MODULE_AVAILABLE_juce_events=1 JUCE_MODULE_AVAILABLE_juce_dsp=1
  JUCE_MODULE_AVAILABLE_juce_data_structures=1 JUCE_MODULE_AVAILABLE_juce_graphics=1
  JUCE_MODULE_AVAILABLE_juce_gui_basics=1 JUCE_MODULE_AVAILABLE_juce_gui_extra=1)

set(formats VST3 Standalone)
if(APPLE)
  list(APPEND formats AU)
endif()
set(names Heat Valve Crush Prism Time Plate Space Tone Voice)
set(codes Eh01 Ev01 Ec01 Ep01 Et01 El01 Es01 Eq01 Eo01)
foreach(i RANGE 0 8)
  list(GET names ${i} name)
  list(GET codes ${i} code)
  juce_add_plugin(Epsilon${name} COMPANY_NAME "Epsilon Sounds"
    BUNDLE_ID "com.epsilonsounds.expansion.${name}" PLUGIN_MANUFACTURER_CODE Epsn PLUGIN_CODE ${code}
    PRODUCT_NAME "Epsilon ${name}" FORMATS ${formats}
    IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE VST3_CATEGORIES Fx COPY_PLUGIN_AFTER_BUILD FALSE)
  target_sources(Epsilon${name} PRIVATE Source/Factory.cpp)
  target_compile_definitions(Epsilon${name} PRIVATE EPSILON_EFFECT_INDEX=${i})
  target_link_libraries(Epsilon${name} PRIVATE EpsilonCommon)
endforeach()

if(EPSILON_UI_TESTS)
  juce_add_console_app(epsilon_snapshot PRODUCT_NAME "Epsilon Snapshot")
  target_sources(epsilon_snapshot PRIVATE Tests/ui_snapshot.cpp)
  target_link_libraries(epsilon_snapshot PRIVATE EpsilonCommon)
  juce_add_console_app(epsilon_ui_perf PRODUCT_NAME "Epsilon UI Perf")
  target_sources(epsilon_ui_perf PRIVATE Tests/ui_perf.cpp)
  target_link_libraries(epsilon_ui_perf PRIVATE EpsilonCommon)
endif()
