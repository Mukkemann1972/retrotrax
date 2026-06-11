cmake_minimum_required(VERSION 3.22)

project(MukkemannRetroTrax VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(libs/JUCE)

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
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
