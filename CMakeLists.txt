cmake_minimum_required(VERSION 3.22)

project(MukkemannRetroTrax VERSION 0.19.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(libs/JUCE)

# --- reSIDfp: echter MOS-6581/8581-SID-Emulator (Dag Lem / Leandro Nini, GPL-2.0+) ---
# Liegt vendored in libs/residfp. POSITION_INDEPENDENT_CODE, weil es ins VST3
# (shared module) gelinkt wird; Threads fuer die std::call_once-Caches.
find_package(Threads REQUIRED)
file(GLOB RESIDFP_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/residfp/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/residfp/*.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/residfp/resample/*.cpp)
add_library(residfp STATIC ${RESIDFP_SOURCES})
target_include_directories(residfp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/libs/residfp)
target_compile_definitions(residfp PUBLIC HAVE_CXX17)
target_compile_features(residfp PUBLIC cxx_std_17)
target_link_libraries(residfp PUBLIC Threads::Threads)
set_target_properties(residfp PROPERTIES POSITION_INDEPENDENT_CODE ON)

juce_add_plugin(RetroTrax
    PRODUCT_NAME "Mukkemann RetroTrax"
    VERSION ${PROJECT_VERSION}
    COMPANY_NAME "Mukkemann"
    COMPANY_WEBSITE "https://printvision-future.de"
    BUNDLE_ID de.mukkemann.retrotrax
    PLUGIN_MANUFACTURER_CODE Mukk
    PLUGIN_CODE Rtrx
    FORMATS VST3 Standalone
    NEEDS_CURL TRUE
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

target_sources(RetroTrax PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/PatternGrid.cpp
    src/SampleDiskBrowser.cpp
    src/HelpPanel.cpp
    src/SidPanel.cpp
)

target_include_directories(RetroTrax PRIVATE src)

target_compile_definitions(RetroTrax PUBLIC
    JUCE_WEB_BROWSER=0
    # curl wird fuer die ST-XX-Downloads gebraucht (HTTPS unter Linux)
    JUCE_USE_CURL=1
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_USE_MP3AUDIOFORMAT=1
)

target_link_libraries(RetroTrax
    PRIVATE
        residfp
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
