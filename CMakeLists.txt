cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version")

project(EZComposer VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    URL https://github.com/juce-framework/JUCE/archive/refs/tags/9.0.2.tar.gz
    URL_HASH SHA256=16d01c27e8327f3644306cc5c223aa6e21af1cc1ec62faa709fc0dced57d208a
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_gui_app(EZComposer
    PRODUCT_NAME "EZ Composer"
    COMPANY_NAME "EZ Composer"
    ICON_BIG Resources/Icon.png         # a staff with two quarter notes
    ICON_SMALL Resources/Icon.png
    BUNDLE_ID "com.ez-composer.app")

target_sources(EZComposer
    PRIVATE
        Source/ChordEditor.cpp
        Source/DynamicPicker.cpp
        Source/EffectRack.cpp
        Source/EffectsWindow.cpp
        Source/EraserButton.cpp
        Source/InstrumentHost.cpp
        Source/InstrumentPanel.cpp
        Source/Main.cpp
        Source/MainComponent.cpp
        Source/MeasureContent.cpp
        Source/MidiInputs.cpp
        Source/Music.cpp
        Source/MusicGlyphs.cpp
        Source/NoteLengthPicker.cpp
        Source/PageTabs.cpp
        Source/PianoKeyboard.cpp
        Source/Piece.cpp
        Source/PluginWindow.cpp
        Source/PositionRuler.cpp
        Source/ProgressionPanels.cpp
        Source/Progressions.cpp
        Source/Recorder.cpp
        Source/RhythmPanels.cpp
        Source/Rhythms.cpp
        Source/Score.cpp
        Source/ScaleTool.cpp
        Source/ScoreDocument.cpp
        Source/ScoreHistory.cpp
        Source/ScorePanel.cpp
        Source/StaffView.cpp)

juce_add_binary_data(EZComposerResources
    SOURCES
        Resources/Fonts/Bravura.otf)

target_compile_definitions(EZComposer
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:EZComposer,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:EZComposer,JUCE_VERSION>"
        # Instruments are loaded from VST3 plugins. AU hosting is compiled in too, though nothing uses it yet.
        JUCE_PLUGINHOST_VST3=1
        JUCE_PLUGINHOST_AU=1)

target_link_libraries(EZComposer
    PRIVATE
        EZComposerResources
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
