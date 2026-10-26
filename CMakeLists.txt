cmake_minimum_required(VERSION 3.22)
project(LiveAudioHost VERSION 0.7.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
  add_compile_options(/utf-8)
endif()

include(CTest)
add_executable(lah_core_tests tests/test_core.cpp)
target_include_directories(lah_core_tests PRIVATE src/audio)
if(MSVC)
  target_compile_options(lah_core_tests PRIVATE /W4 /permissive-)
else()
  target_compile_options(lah_core_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME lah_core COMMAND lah_core_tests)
add_executable(lah_extended_tests tests/test_extended.cpp)
if(MSVC)
  target_compile_options(lah_extended_tests PRIVATE /W4 /permissive-)
else()
  target_compile_options(lah_extended_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME lah_extended COMMAND lah_extended_tests)
add_executable(lah_validation_tests tests/test_validation.cpp)
if(MSVC)
  target_compile_options(lah_validation_tests PRIVATE /W4 /permissive-)
else()
  target_compile_options(lah_validation_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME lah_validation COMMAND lah_validation_tests)
add_executable(lah_engineering_tests tests/test_engineering.cpp)
if(MSVC)
  target_compile_options(lah_engineering_tests PRIVATE /W4 /permissive-)
else()
  target_compile_options(lah_engineering_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME lah_engineering COMMAND lah_engineering_tests)
add_executable(lah_spl_fir_tests tests/test_spl_fir.cpp)
if(MSVC)
  target_compile_options(lah_spl_fir_tests PRIVATE /W4 /permissive-)
else()
  target_compile_options(lah_spl_fir_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME lah_spl_fir COMMAND lah_spl_fir_tests)
add_executable(lah_polar_tests tests/test_polar.cpp)
if(MSVC)
  target_compile_options(lah_polar_tests PRIVATE /W4 /permissive-)
else()
  target_compile_options(lah_polar_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME lah_polar COMMAND lah_polar_tests)
add_executable(lah_fir_cli tools/lah_fir_cli.cpp)
if(MSVC)
  target_compile_options(lah_fir_cli PRIVATE /W4 /permissive-)
else()
  target_compile_options(lah_fir_cli PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_executable(lah_offline_benchmark tools/lah_offline_benchmark.cpp)
add_executable(lah_tf_cli tools/lah_tf_cli.cpp)
add_executable(lah_dsp_cli tools/lah_dsp_cli.cpp)
add_executable(lah_prediction_cli tools/lah_prediction_cli.cpp)
add_executable(lah_builtin_cli tools/lah_builtin_cli.cpp)
add_executable(lah_builtin_benchmark tools/lah_builtin_benchmark.cpp)
add_executable(lah_show_rack_benchmark tools/lah_show_rack_benchmark.cpp)
add_executable(lah_feedback_cli tools/lah_feedback_cli.cpp)
add_executable(lah_feedback_model_cli tools/lah_feedback_model_cli.cpp)
if(MSVC)
  target_compile_options(lah_tf_cli PRIVATE /W4 /permissive-)
else()
  target_compile_options(lah_tf_cli PRIVATE -Wall -Wextra -Wpedantic)
endif()

# v0.6 acceptance suites for safety-critical additions.
find_package(Threads REQUIRED)
foreach(name realtime_fir spl_statistics capture_worker mixer_controls panic_clean latency_compensation coverage phase sub_array engineering_extended builtin_rack builtin_scene feedback_suppression console_modules usb_return_profile live_plugin_parameters builtin_telemetry insert_sound_changes smart_comp_macro bus_master extended_eq rack_operations native_spectrum factory_presets show_rack_acceptance digital_fx_pro dynamics_pro native_pro_rack)
  add_executable(lah_${name}_tests tests/test_${name}.cpp)
  target_link_libraries(lah_${name}_tests PRIVATE Threads::Threads)
  if(MSVC)
    target_compile_options(lah_${name}_tests PRIVATE /W4 /permissive-)
  else()
    target_compile_options(lah_${name}_tests PRIVATE -Wall -Wextra -Wpedantic)
  endif()
  add_test(NAME lah_${name} COMMAND lah_${name}_tests)
endforeach()
# v0.7 synthetic end-to-end acceptance: driver-free, deterministic.
add_executable(lah_operational_acceptance_tests tests/test_operational_acceptance.cpp)
if(MSVC)
  target_compile_options(lah_operational_acceptance_tests PRIVATE /W4 /permissive-)
else()
  target_compile_options(lah_operational_acceptance_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME lah_operational_acceptance COMMAND lah_operational_acceptance_tests)

# Python QA log analyzer is stdlib-only and optional on build hosts.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
  add_test(NAME lah_qa_log_analyzer
           COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/test_qa_analyze.py")
endif()

option(LAH_ENABLE_SCIPY_TESTS "Run independent NumPy/SciPy reference comparisons" OFF)
if(LAH_ENABLE_SCIPY_TESTS)
  if(NOT Python3_Interpreter_FOUND)
    message(FATAL_ERROR "Independent reference tests require Python with NumPy and SciPy")
  endif()
  execute_process(COMMAND "${Python3_EXECUTABLE}" -c "import numpy, scipy"
                  RESULT_VARIABLE LAH_SCIPY_IMPORT_RESULT OUTPUT_QUIET ERROR_QUIET)
  if(NOT LAH_SCIPY_IMPORT_RESULT EQUAL 0)
    message(FATAL_ERROR "Selected Python cannot import NumPy/SciPy; install QA dependencies or disable LAH_ENABLE_SCIPY_TESTS")
  endif()
  add_test(NAME lah_independent_tf COMMAND "${Python3_EXECUTABLE}"
           "${CMAKE_SOURCE_DIR}/tools/independent_reference_check.py" $<TARGET_FILE:lah_tf_cli>)
  add_test(NAME lah_independent_fir COMMAND "${Python3_EXECUTABLE}"
           "${CMAKE_SOURCE_DIR}/tools/independent_fir_check.py" $<TARGET_FILE:lah_fir_cli>)
  add_test(NAME lah_independent_dsp COMMAND "${Python3_EXECUTABLE}"
           "${CMAKE_SOURCE_DIR}/tools/independent_dsp_check.py" $<TARGET_FILE:lah_dsp_cli>)
  add_test(NAME lah_independent_prediction COMMAND "${Python3_EXECUTABLE}"
           "${CMAKE_SOURCE_DIR}/tools/independent_prediction_check.py" $<TARGET_FILE:lah_prediction_cli>)
  add_test(NAME lah_independent_builtin COMMAND "${Python3_EXECUTABLE}"
           "${CMAKE_SOURCE_DIR}/tools/independent_builtin_check.py" $<TARGET_FILE:lah_builtin_cli>)
  add_test(NAME lah_independent_feedback COMMAND "${Python3_EXECUTABLE}"
           "${CMAKE_SOURCE_DIR}/tools/independent_feedback_check.py" $<TARGET_FILE:lah_feedback_cli>)
  add_test(NAME lah_independent_feedback_model COMMAND "${Python3_EXECUTABLE}"
           "${CMAKE_SOURCE_DIR}/tools/independent_feedback_model_check.py" $<TARGET_FILE:lah_feedback_model_cli>)
  set_tests_properties(lah_independent_tf lah_independent_fir lah_independent_dsp lah_independent_prediction lah_independent_builtin lah_independent_feedback lah_independent_feedback_model PROPERTIES LABELS "offline;independent" TIMEOUT 180)
endif()

# CRITICAL: Release normally defines NDEBUG, silently disabling <cassert>.
# Explicitly re-enable assertions for EVERY executable labeled *_tests so
# CI does not report meaningless "passing" tests compiled without assertions.
foreach(test_target
    lah_core_tests lah_extended_tests lah_validation_tests
    lah_engineering_tests lah_spl_fir_tests lah_polar_tests
    lah_realtime_fir_tests lah_spl_statistics_tests lah_capture_worker_tests
    lah_mixer_controls_tests lah_panic_clean_tests lah_latency_compensation_tests lah_operational_acceptance_tests
    lah_coverage_tests lah_phase_tests lah_sub_array_tests lah_engineering_extended_tests lah_builtin_rack_tests lah_builtin_scene_tests lah_feedback_suppression_tests lah_console_modules_tests lah_usb_return_profile_tests lah_live_plugin_parameters_tests lah_builtin_telemetry_tests lah_insert_sound_changes_tests lah_smart_comp_macro_tests lah_bus_master_tests lah_extended_eq_tests lah_rack_operations_tests lah_native_spectrum_tests lah_factory_presets_tests lah_show_rack_acceptance_tests lah_digital_fx_pro_tests lah_dynamics_pro_tests lah_native_pro_rack_tests)
  if(MSVC)
    target_compile_options(${test_target} PRIVATE /UNDEBUG)
  else()
    target_compile_options(${test_target} PRIVATE -UNDEBUG)
  endif()
endforeach()

# Pure C++ tests may be built in an environment that does not have JUCE.
option(LAH_BUILD_GUI "Build native GUI (requires JUCE)" ON)
option(LAH_FETCH_JUCE "Download pinned JUCE source during CMake configure" OFF)
set(JUCE_ROOT "" CACHE PATH "Path to extracted JUCE root folder")
if(NOT LAH_BUILD_GUI)
  return()
endif()

if(JUCE_ROOT AND EXISTS "${JUCE_ROOT}/CMakeLists.txt")
  add_subdirectory("${JUCE_ROOT}" "${CMAKE_BINARY_DIR}/juce")
elseif(LAH_FETCH_JUCE)
  include(FetchContent)
  FetchContent_Declare(JUCE
    URL https://github.com/juce-framework/JUCE/archive/refs/tags/8.0.15.zip
    URL_HASH SHA256=72838128c730eba22c63dfdc8bf155304887a9f4e9d9f61ec569c7f8e4c3fab1
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  FetchContent_MakeAvailable(JUCE)
else()
  message(FATAL_ERROR "JUCE source missing. Set -DJUCE_ROOT=C:/SDK/JUCE or -DLAH_FETCH_JUCE=ON, or -DLAH_BUILD_GUI=OFF to test core only.")
endif()

include(cmake/JuceWindowsResources.cmake)
option(LAH_ENABLE_ASIO "Enable ASIO backend on Windows (review bundled ASIO license)" ON)
juce_add_gui_app(LiveAudioHost
  PRODUCT_NAME "LiveAudioHost"
  COMPANY_NAME "LiveAudioHost"
  BUNDLE_ID "com.liveaudiohost.desktop")

target_sources(LiveAudioHost PRIVATE
  src/app/Main.cpp
  src/app/MutedStreamCheck.h
  src/app/M32MutedStreamCheck.h
  src/audio/LiveAudioEngine.cpp
  src/audio/LiveAudioEngine.h
  src/audio/PassThroughCore.h
  src/ui/MeterBank.cpp
  src/ui/MeterBank.h
  src/ui/MixerPanel.cpp
  src/ui/MixerPanel.h
  src/ui/BusMasterPanel.h
  src/ui/BusMasterPanel.cpp
  src/ui/BusMasterSceneState.h
  src/ui/SessionState.h
  src/ui/SceneLibraryPanel.h
  src/ui/SceneLibraryPanel.cpp
  src/ui/PluginPanel.cpp
  src/ui/PluginPanel.h
  src/ui/EffectsPanel.cpp
  src/ui/EffectsPanel.h
  src/ui/MeasurementPanel.cpp
  src/spl/CalibratedSplCore.h
  src/spl/SplStatisticsCore.h
  src/spl/SplWorker.h
  src/measurement/MeasurementWorker.h
  src/fir/FIRCore.h
  src/ui/SPLPanel.h
  src/ui/SPLPanel.cpp
  src/ui/FIRPanel.h
  src/ui/FIRPanel.cpp
  src/ui/MeasurementPanel.h
  src/ui/SubPanel.cpp
  src/ui/SubPanel.h
  src/engineering/EngineeringCore.h
  src/engineering/CoverageCore.h
  src/ui/EngineeringPanel.h
  src/ui/EngineeringPanel.cpp
  src/ui/EngineeringExtended.cpp
  src/dsp/BuiltinRackCore.h
  src/ui/BuiltinRackPanel.h
  src/ui/BuiltinRackPanel.cpp
  src/ui/NativePluginEditor.h
  src/ui/NativePluginEditor.cpp
  src/ui/NativePresetControls.h
  src/ui/NativePresetControls.cpp
  src/ui/DigitalFxProPanel.h
  src/ui/DigitalFxProPanel.cpp
  src/ui/DynamicsProPanel.h
  src/ui/DynamicsProPanel.cpp
  src/dsp/DigitalFxPro.h
  src/dsp/DynamicsPro.h
  src/dsp/BuiltinFactoryPresets.h
  src/dsp/NativeSpectrum.h
  src/ui/NativePluginWindow.h
  src/ui/NativePluginAudit.h
  src/ui/NativePluginAudit.cpp
  src/ui/StudioLookAndFeel.h
  src/ui/FrontFillPanel.h
  src/ui/FrontFillPanel.cpp
  src/engineering/PhaseCore.h
  src/ui/PhasePanel.h
  src/ui/PhasePanel.cpp
  src/dsp/MixDspCore.h
  src/measurement/MeasurementCore.h
  src/simulation/SubArrayCore.h
  src/simulation/MeasuredSpeakerCore.h
  src/ui/MainComponent.cpp
  src/ui/MainComponent.h)

target_compile_definitions(LiveAudioHost PRIVATE
  LAH_VERSION_STRING="${PROJECT_VERSION}"
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_PLUGINHOST_VST3=1)
if(WIN32 AND LAH_ENABLE_ASIO)
  target_compile_definitions(LiveAudioHost PRIVATE JUCE_ASIO=1)
endif()
if(MSVC)
  target_compile_options(LiveAudioHost PRIVATE /utf-8)
endif()

target_link_libraries(LiveAudioHost PRIVATE
  juce::juce_audio_processors
  juce::juce_audio_utils
  juce::juce_recommended_config_flags
  juce::juce_recommended_warning_flags
  juce::juce_recommended_lto_flags)

# Exercise the actual device callback and hosted-processor boundary without
# opening hardware. Keep the original JUCE-free suites usable on build hosts.
if(BUILD_TESTING)
  juce_add_console_app(lah_session_state_tests PRODUCT_NAME "LiveAudioHost Session Tests")
  target_sources(lah_session_state_tests PRIVATE tests/test_session_state.cpp)
  target_compile_definitions(lah_session_state_tests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
  target_link_libraries(lah_session_state_tests PRIVATE juce::juce_core juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
  if(MSVC)
    target_compile_options(lah_session_state_tests PRIVATE /UNDEBUG /utf-8)
  else()
    target_compile_options(lah_session_state_tests PRIVATE -UNDEBUG)
  endif()
  add_test(NAME lah_session_state COMMAND lah_session_state_tests)
  juce_add_console_app(lah_audio_engine_tests PRODUCT_NAME "LiveAudioHost Engine Tests")
  target_sources(lah_audio_engine_tests PRIVATE tests/test_audio_engine.cpp src/audio/LiveAudioEngine.cpp)
  target_compile_definitions(lah_audio_engine_tests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_PLUGINHOST_VST3=0)
  target_link_libraries(lah_audio_engine_tests PRIVATE juce::juce_audio_processors juce::juce_audio_devices
                        juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
  if(MSVC)
    target_compile_options(lah_audio_engine_tests PRIVATE /UNDEBUG /utf-8)
  else()
    target_compile_options(lah_audio_engine_tests PRIVATE -UNDEBUG)
  endif()
  add_test(NAME lah_audio_engine COMMAND lah_audio_engine_tests)
  set_tests_properties(lah_audio_engine PROPERTIES LABELS "engine;synthetic" TIMEOUT 60)
  add_test(NAME lah_ui_smoke COMMAND LiveAudioHost --smoke-test "${CMAKE_BINARY_DIR}/ui-smoke")
  set_tests_properties(lah_ui_smoke PROPERTIES LABELS "gui;synthetic" TIMEOUT 120)
  add_test(NAME lah_plugin_audio_audit COMMAND LiveAudioHost --plugin-audio-check "${CMAKE_BINARY_DIR}/plugin-audio-audit")
  set_tests_properties(lah_plugin_audio_audit PROPERTIES LABELS "gui;engine;synthetic" TIMEOUT 60)
endif()

