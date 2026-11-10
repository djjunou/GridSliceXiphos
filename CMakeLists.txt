cmake_minimum_required(VERSION 3.22)

project(GridSliceXiphos VERSION 1.2.0) # release label: Beta 1

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE is fetched automatically (no submodule to manage).
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# Bundled GUI skins (embedded into the plugin binary)
juce_add_binary_data(GridSliceXiphosSkins SOURCES
    assets/skins/skin_chrome_sky.png
    assets/skins/skin_chrome_bloom.png
    assets/skins/skin_iridescent.png
)

juce_add_plugin(GridSliceXiphos
    COMPANY_NAME            "JunouWorks"
    BUNDLE_ID               "jp.junouworks.gridslicexiphos"
    PLUGIN_MANUFACTURER_CODE JnWk
    PLUGIN_CODE              GsXh
    FORMATS                  AU VST3 Standalone
    PRODUCT_NAME             "GridSliceXiphos"
    IS_SYNTH                   TRUE
    NEEDS_MIDI_INPUT           TRUE
    NEEDS_MIDI_OUTPUT          FALSE
    IS_MIDI_EFFECT             FALSE
    COPY_PLUGIN_AFTER_BUILD    FALSE
    VST3_CATEGORIES            "Instrument" "Sampler"
)

target_sources(GridSliceXiphos PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
)

target_include_directories(GridSliceXiphos PRIVATE src)

# MSVC defaults to the local ANSI codepage for source files, which garbles
# any non-ASCII text (v1.0 title mojibake). Force UTF-8 on Windows builds.
if (MSVC)
    target_compile_options(GridSliceXiphos PRIVATE /utf-8)
endif()

target_compile_definitions(GridSliceXiphos PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(GridSliceXiphos PRIVATE
    GridSliceXiphosSkins
    juce::juce_audio_formats
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

# --- Slice-math unit test (no JUCE needed) -------------------------------
option(GRIDSLICEX_BUILD_TESTS "Build slice math tests" ON)
if(GRIDSLICEX_BUILD_TESTS)
    enable_testing()
    add_executable(test_slicegrid tests/test_slicegrid.cpp)
    target_include_directories(test_slicegrid PRIVATE src)
    add_test(NAME slicegrid COMMAND test_slicegrid)
endif()
