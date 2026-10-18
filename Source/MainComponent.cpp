#include "MainComponent.h"

#include "Controls.h"

#include <algorithm>
#include <limits>

namespace
{
    constexpr int toolbarHeight = 44;
    constexpr int toolbarGap = 6;               // either side of MP3, which stands apart from the buttons for playing
    constexpr int sidebarWidth = 280;
    constexpr int sidebarPadding = 14;
    constexpr int keyboardHeight = 140;
    constexpr int volumeColumnWidth = 64;
    constexpr int volumeDialSize = 44;
    constexpr int volumeTextHeight = 16;
    constexpr int deleteButtonSize = 22;        // to the right of the staves, in line with the measure buttons
    constexpr int addPartButtonSize = 28;       // in the space under the last part's staves, at the left
    constexpr int addPartBottom = 8;            // up from the bottom of the last part
    constexpr int measureButtonSize = 28;
    // The note length, dynamic and eraser buttons, over the top of the staves
    constexpr int toolButtonsTop = 8, toolButtonsHeight = 28;

    // Above the first system, so even when its chord buttons are as near its top as they go,
    // half a staff space below it, there's a gap between them and the buttons above
    constexpr int stavesTopPadding = toolButtonsTop + toolButtonsHeight + 10 - 6;
    constexpr int measureButtonsWidth = 84;     // the measure buttons, after the final barline
    constexpr int measureButtonsGap = 14;       // between the final barline and them
    constexpr int eraserMargin = 10;            // from the eraser to the scroll bar
    constexpr int editButtonWidth = 44, editButtonGap = 6;     // the Edit button, to the eraser's left
    constexpr int selectButtonWidth = 56;                       // and Select, between them
    constexpr int selectionButtonHeight = 26, deleteMeasuresWidth = 64, moveMeasuresWidth = 56;
    constexpr int cloneButtonWidth = 56, cloneButtonHeight = 26;
    constexpr int rightMargin = measureButtonsGap + measureButtonsWidth + 12;      // after the final barline, for the measure buttons

    // A full 88-key piano, A0 to C8
    constexpr int lowestKey = 21;
    constexpr int highestKey = 108;
    constexpr int numWhiteKeys = 52;

    // Where Loop is kept in the settings
    const char* const loopKey = "loop";

    // And the metronome
    const char* const metronomeKey = "metronome";

    // And whether Play plays the whole piece, or the page being edited
    const char* const playWholePieceKey = "playWholePiece";

    // And the effects, as JSON
    const char* const effectsKey = "effects";

    // And how saved progressions were last applied: the chord type and clef
    const char* const progressionTypeKey = "progressionChordType";
    const char* const progressionClefKey = "progressionClef";

    // The note length and rhythm they were kept under, before each started out Automatic and No Rhythm
    const char* const oldProgressionLengthKey = "progressionNoteLength";
    const char* const oldProgressionRhythmKey = "progressionRhythm";

    // The MIDI menu's items are numbered from here, in the order of the inputs they're for.
    constexpr int firstMidiInputItem = 1000;

    // The File menu's Open Recent items, numbered from here in the order of the recent scores
    constexpr int firstRecentScoreItem = 2000;
    constexpr int noRecentScoresItem = firstRecentScoreItem + 90;
    constexpr int clearRecentScoresItem = firstRecentScoreItem + 91;

    // Where a part's volume is kept in the settings, such as "volume1"
    juce::String getVolumeKey (int part)
    {
        return "volume" + juce::String (part + 1);
    }

    // And whether it's muted, such as "muted1"
    juce::String getMutedKey (int part)
    {
        return "muted" + juce::String (part + 1);
    }

    /** A number without trailing zeros, e.g. "120" or "92.5". */
    juce::String formatNumber (double number)
    {
        auto text = juce::String (number, 2);

        while (text.endsWithChar ('0'))
            text = text.dropLastCharacters (1);

        return text.trimCharactersAtEnd (".");
    }
}

//==============================================================================
MainComponent::StaffSystems::StaffSystems (Score& scoreToShow)
    : score (scoreToShow)
{
    // The + below the last part adds another part after it.
    addPartButton.setTooltip ("Add an instrument, with its own staves, below the last");
    addPartButton.onClick = [this] { if (onAddPart != nullptr) onAddPart(); };

    addPartButton.setLookAndFeel (&roundButtonLookAndFeel);
    addPartButton.setColour (juce::TextButton::textColourOffId, controls::accent);
    addPartButton.setWantsKeyboardFocus (false);
    addAndMakeVisible (addPartButton);

    layOut();
}

MainComponent::StaffSystems::~StaffSystems()
{
    addPartButton.setLookAndFeel (nullptr);
}

StaffView& MainComponent::StaffSystems::insertView (int index, int part)
{
    auto& view = **views.insert (views.begin() + index, std::make_unique<StaffView> (score, part));
    addAndMakeVisible (view);
    layOut();
    return view;
}

void MainComponent::StaffSystems::removeView (int index)
{
    views.erase (views.begin() + index);
    layOut();
}

void MainComponent::StaffSystems::setMinimumSize (int width, int height)
{
    minimumWidth = width;
    minimumHeight = height;
    layOut();
}

std::optional<int> MainComponent::StaffSystems::getPartAt (int y) const
{
    if (views.empty())
        return {};

    for (const auto& view : views)
        if (y < view->getBottom())
            return view->getPart();

    return views.back()->getPart();
}

void MainComponent::StaffSystems::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu() || onPartClicked == nullptr)
        return;

    if (const auto part = getPartAt (e.getPosition().y); part.has_value() && *part >= 0)
        onPartClicked (*part);
}

void MainComponent::StaffSystems::childBoundsChanged (juce::Component*)
{
    // A view grows or shrinks with its score.
    layOut();
}

void MainComponent::StaffSystems::layOut()
{
    if (layingOut || views.empty())
        return;

    const juce::ScopedValueSetter<bool> guard (layingOut, true);

    auto width = 0;

    for (auto& view : views)
        width = juce::jmax (width, view->getContentWidth());

    // Each system is as tall as its music needs, stacked from the top, under the note length
    // buttons, with any spare room below.
    auto y = stavesTopPadding;

    for (auto& view : views)
    {
        const auto height = view->getContentHeight();
        view->setBounds (0, y, width, height);
        y += height;
    }

    // The + for adding a part, below the last one's staves, at the left, in the room left under
    // them for low notes, so it takes no more height
    addPartButton.setBounds (12, y - addPartBottom - addPartButtonSize, addPartButtonSize, addPartButtonSize);
    addPartButton.toFront (false);
    y += 4;

    setSize (juce::jmax (width + rightMargin, minimumWidth), juce::jmax (y, minimumHeight));

    if (onLayoutChanged != nullptr)
        onLayoutChanged();
}

//==============================================================================
MainComponent::MainComponent (juce::PropertiesFile& settingsToUse)
    : settings (settingsToUse),
      document (piece, settings)
{
    // Each part has its own instrument, which is saved with the score, sound and all.
    document.getInstrumentToSave = [this] (int part) { return tracks[(size_t) part].panel->saveToJSON(); };
    document.loadInstrument = [this] (int part, const juce::var& json) { tracks[(size_t) part].panel->loadFromJSON (json); };

    // Every control has a tip, which shows when the pointer rests on it, drawn quietly.
    tooltipWindow.setLookAndFeel (&tooltipLookAndFeel);
    tooltipWindow.setOpaque (false);

    playButton.setButtonText ("Play");
    playButton.onClick = [this] { togglePlayback(); };

    // Pause stops where it's got to, to carry on from there, where Stop goes back to the beginning.
    pauseButton.setButtonText ("Pause");
    pauseButton.onClick = [this] { pausePlayback(); };
    updateTransportButtons();

    // Record plays the score too, and writes what's played on the keyboard into the active part.
    recordButton.setButtonText ("Record");
    recordButton.setTooltip ("Record: play the score, and write what you play on the piano or a MIDI controller into the active instrument's staves");
    recordButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xfffbe2e0));
    recordButton.onClick = [this] { toggleRecording(); };

    // Beside Record, back to the start of the page, and back or on a measure, playing or stopped
    backButton.setButtonText ("Back to Start");
    backButton.setTooltip ("Back to the start of the page you're on, playing or stopped");
    backButton.onClick = [this] { goToPageStart(); };
    rewindButton.setButtonText ("Rewind");
    rewindButton.setTooltip ("Rewind: back to the start of the measure before, playing or stopped");
    rewindButton.onClick = [this] { stepMeasure (-1); };
    fastForwardButton.setButtonText ("Fast Forward");
    fastForwardButton.setTooltip ("Fast forward: on to the start of the next measure, playing or stopped");
    fastForwardButton.onClick = [this] { stepMeasure (1); };

    // Loop is remembered from one run of the app to the next.
    playButton.setConnectedEdges (juce::Button::ConnectedOnRight);

    for (auto* button : { &pauseButton, &recordButton, &backButton, &rewindButton, &fastForwardButton })
        button->setConnectedEdges (juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);

    loopButton.setConnectedEdges (juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
    loopButton.setClickingTogglesState (true);
    loopButton.setToggleState (settings.getBoolValue (loopKey), juce::dontSendNotification);
    loopButton.setColour (juce::TextButton::buttonOnColourId, controls::accentLight);
    loopButton.setColour (juce::TextButton::textColourOnId, controls::accent);
    loopButton.setTooltip ("Play the piece, or the page, over and over, rather than stopping at the end");
    loopButton.onClick = [this]
    {
        settings.setValue (loopKey, loopButton.getToggleState());
        instrumentHost.setLooping (loopButton.getToggleState());
    };

    // Piece plays every page, one after another, and Page just the one being edited. Loop goes
    // back to the start of whichever it is. It's remembered too.
    playModeButton.setConnectedEdges (juce::Button::ConnectedOnLeft);
    playModeButton.setWantsKeyboardFocus (false);
    playModeButton.onClick = [this] { setPlayingWholePiece (! isPlayingWholePiece()); };
    setPlayingWholePiece (settings.getBoolValue (playWholePieceKey, true));

    // MP3 writes what's heard as the score plays to a sound file, saved as an MP3 when it stops.
    // It's not remembered, as each playback while it's on asks where to save what was heard.
    mp3Button.setClickingTogglesState (true);
    mp3Button.setWantsKeyboardFocus (false);
    mp3Button.setColour (juce::TextButton::buttonOnColourId, controls::accentLight);
    mp3Button.setColour (juce::TextButton::textColourOnId, controls::accent);
    mp3Button.setTooltip ("MP3: record what's heard as the score plays, to save as an MP3 when it stops");
    mp3Button.onClick = [this] { setMakingMP3 (mp3Button.getToggleState()); };
    onSoundFileReady = [this] (const juce::File& wav) { chooseWhereToSaveMP3 (wav); };

    // The metronome ticks each beat as the score plays or records, and is remembered too.
    metronomeButton.setButtonText ("Metronome");
    metronomeButton.setClickingTogglesState (true);
    metronomeButton.setToggleState (settings.getBoolValue (metronomeKey), juce::dontSendNotification);
    metronomeButton.setColour (juce::TextButton::buttonOnColourId, controls::accentLight);
    metronomeButton.setTooltip ("Metronome: a clang on the first beat of each measure, and a click on the others, as the score plays or records");
    metronomeButton.onClick = [this]
    {
        settings.setValue (metronomeKey, metronomeButton.getToggleState());
        instrumentHost.setMetronome (metronomeButton.getToggleState());
    };
    instrumentHost.setMetronome (metronomeButton.getToggleState());

    tempoLabel.setText ("BPM", juce::dontSendNotification);
    tempoLabel.setFont (juce::FontOptions (12.5f));
    tempoLabel.setColour (juce::Label::textColourId, controls::secondaryText);
    tempoLabel.setJustificationType (juce::Justification::centredRight);

    tempoEditor.setInputRestrictions (6, "0123456789.");
    tempoEditor.setJustification (juce::Justification::centred);
    tempoEditor.setText (formatNumber (score.getBeatsPerMinute()), false);
    tempoEditor.setTooltip ("The tempo, in quarter notes per minute, from 20 to 300");
    tempoEditor.onTextChange = [this] { tempoEdited (false); };
    tempoEditor.onReturnKey = [this] { tempoEdited (true); tempoEditor.giveAwayKeyboardFocus(); };
    tempoEditor.onFocusLost = [this] { tempoEdited (true); };

    // The arrows beside it step it up and down, as the up and down arrow keys do while typing in
    // it. Holding an arrow down goes on stepping, and is undone in one go.
    // Their tips say when the tempo can't go any further.
    tempoEditor.addKeyListener (this);

    for (auto [button, direction] : { std::pair { &tempoUpButton, 1 }, { &tempoDownButton, -1 } })
    {
        button->onClick = [this, step = direction] { stepTempo (step); };
        button->onPress = [this] { history.beginGesture(); };
        button->onRelease = [this] { history.endGesture(); };
        addAndMakeVisible (*button);
    }

    updateTempoArrows();


    // The active part, which the keyboard plays
    partBox.setTooltip ("The instrument the keyboard plays, and whose alternate staff and progression the sidebar sets");
    partBox.setWantsKeyboardFocus (false);
    partBox.onChange = [this] { if (partBox.getSelectedId() > 0) setActivePart (partBox.getSelectedId() - 1); };

    // Clicking a button shouldn't take the keyboard focus away from the piano, which the
    // computer keyboard can play too.
    for (auto* button : std::initializer_list<juce::Button*> { &playButton, &pauseButton, &recordButton, &backButton, &rewindButton, &fastForwardButton,
                                                               &loopButton, &metronomeButton })
        button->setWantsKeyboardFocus (false);

    for (auto* component : std::initializer_list<juce::Component*> { &playButton, &pauseButton, &recordButton, &backButton, &rewindButton, &fastForwardButton,
                                                                     &loopButton, &playModeButton, &mp3Button, &metronomeButton, &tempoLabel, &tempoEditor, &partBox })
        addAndMakeVisible (component);

    staffSystems.onAddPart = [this] { addPart(); };

    // Clicking beside an instrument's staves, past the end of the score or to the left of them,
    // by its volume dial, makes it the active instrument, as clicking its staves does.
    staffSystems.onPartClicked = [this] (int part) { setActivePart (part); };
    volumeColumn.onClicked = [this] (const juce::MouseEvent& e)
    {
        if (e.mods.isPopupMenu())
            return;

        if (const auto part = staffSystems.getPartAt (staffSystems.getLocalPoint (&volumeColumn, e.getPosition()).y); part.has_value() && *part >= 0)
            setActivePart (*part);
    };

    // + adds a measure at the end and − takes the last one away, and Clone repeats every measure
    // after the last, in every part.
    addMeasureButton.setTooltip ("Add a measure at the end");
    cloneButton.setTooltip ("Repeat all the measures after the last one, with everything in them");
    addMeasureButton.onClick = [this] { addMeasure(); };
    removeMeasureButton.onClick = [this] { removeMeasure(); };
    cloneButton.onClick = [this] { cloneMeasures(); };
    cloneButton.setColour (juce::TextButton::buttonColourId, juce::Colours::white);
    cloneButton.setColour (juce::TextButton::textColourOffId, controls::accent);
    cloneButton.setWantsKeyboardFocus (false);

    for (auto* button : { &addMeasureButton, &removeMeasureButton })
    {
        button->setLookAndFeel (&roundButtonLookAndFeel);
        button->setColour (juce::TextButton::textColourOffId, controls::accent);
        button->setWantsKeyboardFocus (false);
    }
    // Note lengths for clicking into the staff, quarter notes to start with, staccato or not, or dynamics and hairpins to mark
    noteLengthPicker.onChange = [this] (double beats) { setNoteLength (beats); };
    noteLengthPicker.onStaccatoChange = [this] (bool staccato) { setStaccato (staccato); };
    dynamicPicker.onChange = [this] (std::optional<music::Marking> marking) { setMarking (marking); };
    eraserButton.onClick = [this] { setErasing (! eraserButton.getToggleState()); };
    editButton.setTooltip ("Edit notes: drag a note to move it, or click one to change its length, make it staccato, or delete it");
    editButton.onClick = [this] { setEditing (! editButton.getToggleState()); };

    // Select picks out whole measures, to delete or move, with the buttons over them.
    selectButton.setTooltip ("Select measures: drag across them, then delete them, or move them between others");
    selectButton.onClick = [this] { setSelecting (! isSelecting()); };
    deleteMeasuresButton.onClick = [this] { deleteSelectedMeasures(); };
    moveMeasuresButton.onClick = [this] { setMovingMeasures (! moveMeasuresButton.getToggleState()); };
    deleteMeasuresButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffd8413a));
    moveMeasuresButton.setColour (juce::TextButton::textColourOffId, controls::accent);
    moveMeasuresButton.setColour (juce::TextButton::buttonOnColourId, controls::accentLight);
    moveMeasuresButton.setColour (juce::TextButton::textColourOnId, controls::accent);

    for (auto* button : { &deleteMeasuresButton, &moveMeasuresButton })
    {
        button->setColour (juce::TextButton::buttonColourId, juce::Colours::white);
        button->setWantsKeyboardFocus (false);
        staffSystems.addChildComponent (*button);
    }

    setNoteLength (1.0);

    scorePanel.onCopyProgression = [this] (int toPart) { copyProgression (toPart); };
    scorePanel.onScale = [this] (juce::Component& button) { showScalePanel (button); };
    scorePanel.onSaveProgression = [this] (juce::Component& button) { showSaveProgressionPanel (button); };
    scorePanel.onApplyProgression = [this] (juce::Component& button) { showApplyProgressionPanel (button); };
    scorePanel.setNumSavedProgressions ((int) savedProgressions.getAll().size());
    scorePanel.onSaveRhythm = [this] (juce::Component& button) { showSaveRhythmPanel (button); };
    scorePanel.onApplyRhythm = [this] (juce::Component& button) { showApplyRhythmPanel (button); };
    scorePanel.setNumSavedRhythms ((int) savedRhythms.getAll().size());

    progressionStyle.type = (music::ChordType) juce::jlimit (0, 4, settings.getIntValue (progressionTypeKey, 0));
    progressionStyle.staff = settings.getValue (progressionClefKey) == "bass" ? Staff::bass : Staff::treble;

    settings.removeValue (oldProgressionLengthKey);
    settings.removeValue (oldProgressionRhythmKey);
    sidebarContent.addAndMakeVisible (scorePanel);
    sidebar.setViewedComponent (&sidebarContent, false);
    sidebar.setScrollBarsShown (true, false);
    addAndMakeVisible (sidebar);

    staffViewport.setViewedComponent (&staffSystems, false);
    staffViewport.setScrollBarsShown (true, true);
    addAndMakeVisible (staffViewport);

    addAndMakeVisible (volumeColumn);
    addAndMakeVisible (noteLengthPicker);
    addAndMakeVisible (dynamicPicker);
    addAndMakeVisible (eraserButton);
    addAndMakeVisible (editButton);
    addAndMakeVisible (selectButton);

    for (auto* component : std::initializer_list<juce::Component*> { &addMeasureButton, &removeMeasureButton, &cloneButton })
        staffSystems.addAndMakeVisible (component);

    updateRemoveMeasureButton();
    staffSystems.onLayoutChanged = [this] { positionVolumeDials(); positionMeasureButtons(); positionSelectionButtons(); positionRuler.repaint(); };
    staffViewport.onScroll = [this] { positionVolumeDials(); positionMeasureButtons(); positionSelectionButtons(); positionRuler.repaint(); };

    // The ruler follows the first part's staves, as they all line up, and where they've scrolled to.
    positionRuler.getView = [this] { return staffSystems.views.empty() ? nullptr : staffSystems.views[0].get(); };
    positionRuler.getScrolledX = [this] { return staffViewport.getViewPositionX(); };
    positionRuler.onBeatChosen = [this] (int beat) { choosePlaybackStart (beat); };
    addAndMakeVisible (positionRuler);

    // A tab for each page, choosing which is edited, with a + for adding another
    pageTabs.onPageChosen = [this] (int page) { piece.choosePage (page); };
    pageTabs.onAddPage = [this] { piece.addPage(); };
    pageTabs.onCopyPage = [this] (int page) { copyPage (page); };
    pageTabs.onDeletePage = [this] (int page) { deletePage (page); };
    pageTabs.onMovePage = [this] (int from, int to) { movePage (from, to); };
    pageTabs.onRenamePage = [this] (int page, const juce::String& name) { piece.renamePage (page, name); };
    piece.onPagesChanged = [this] { pagesChanged(); };
    addAndMakeVisible (pageTabs);

    // A track for each part of the new score, with the instruments, volumes and muting it had last time
    for (int part = 0; part < score.getNumParts(); ++part)
        insertTrack (part, true);

    numberTracks();
    updatePartBox();

    // The effects, and where the instruments go, as they were last time, saved with the score and
    // loaded with it. Changes to them count as changes to the score, which needs saving.
    effectsButton.setTooltip ("Effects: chains of effect plugins, called buses, and which instruments go through them");
    effectsButton.setWantsKeyboardFocus (false);
    effectsButton.onClick = [this] { showEffects(); };
    addAndMakeVisible (effectsButton);

    effectRack.onChanged = [this] (bool byUser)
    {
        if (byUser)
            document.changed();

        saveEffects();

        if (effectsWindow != nullptr)
            effectsWindow->refresh();
    };

    if (const auto saved = juce::JSON::parse (settings.getValue (effectsKey)); saved.isObject())
        reportFailedEffects (effectRack.loadJSON (saved), "were used last time");

    document.getEffectsToSave = [this] { return effectRack.toJSON(); };
    document.loadEffects = [this] (const juce::var& json) { reportFailedEffects (effectRack.loadJSON (json), "the score was saved with"); };

    document.getVolumeToSave = [this] (int part) { return instrumentHost.getVolume (part); };
    document.loadVolume = [this] (int part, float decibels) { setVolume (part, decibels, false); };
    document.getMutedToSave = [this] (int part) { return instrumentHost.isMuted (part); };
    document.loadMuted = [this] (int part, bool muted) { setMuted (part, muted, false); };

    keyboard.setAvailableRange (lowestKey, highestKey);
    keyboard.setOctaveForMiddleC (4);
    keyboard.setKeyPressBaseOctave (5);     // the computer keyboard's A key plays middle C
    keyboard.setScrollButtonsVisible (false);
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, juce::Colour (0xff4a8fe0));
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, juce::Colour (0x264a8fe0));
    keyboard.onKeyClicked = [this] (int midiNote) { pianoKeyClicked (midiNote); };
    keyboard.setTooltip ("Click or drag to play the active instrument. Once it's clicked, the computer's A to L keys play it too, from middle C.");

    // A MIDI controller plays like the on-screen piano, including adding notes to a chord.
    midiInputs.onNoteOn = [this] (int midiNote) { pianoKeyClicked (midiNote); };
    midiInputs.onDevicesChanged = [this] { menuItemsChanged(); };
    addAndMakeVisible (keyboard);

    piece.addChangeListener (this);
    document.addChangeListener (this);

    // The File and Edit menus, with their keyboard shortcuts. Undo and Redo are greyed out when
    // there's nothing to undo or redo.
    history.onChange = [this] { commandManager.commandStatusChanged(); };
    commandManager.registerAllCommandsForTarget (this);
    commandManager.setFirstCommandTarget (this);
    setApplicationCommandManagerToWatch (&commandManager);
    juce::MenuBarModel::setMacMainMenu (this);

    setActivePart (0);
    showPartTitles();
    setSize (1676, 974);    // wide enough for four measures of most music, and the buttons after them, and tall enough for two instruments

    juce::AudioDeviceManager::AudioDeviceSetup preferredSetup;
    preferredSetup.bufferSize = 256;    // small enough for the keyboard to feel immediate

    if (const auto error = audioDeviceManager.initialise (0, 2, nullptr, true, {}, &preferredSetup); error.isNotEmpty())
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't open the audio output", error);

    audioDeviceManager.addAudioCallback (&instrumentHost);
}

MainComponent::~MainComponent()
{
    // The effects page goes, and the effects are kept, as they may have been changed in their editors.
    effectsWindow = nullptr;
    saveEffects();

    // The menu bar stops watching the command manager before the manager goes, which is before
    // the menu bar is.
    juce::MenuBarModel::setMacMainMenu (nullptr);
    setApplicationCommandManagerToWatch (nullptr);
    commandManager.setFirstCommandTarget (nullptr);

    // The dials' and buttons' look goes before they do.
    for (auto& track : tracks)
        track.dial->setLookAndFeel (nullptr);

    for (auto* button : { &addMeasureButton, &removeMeasureButton })
        button->setLookAndFeel (nullptr);

    tooltipWindow.setLookAndFeel (nullptr);

    tempoEditor.removeKeyListener (this);
    audioDeviceManager.removeAudioCallback (&instrumentHost);

    // What was heard and hasn't been saved as an MP3 goes.
    mp3Chooser = nullptr;
    instrumentHost.closeSoundFile().deleteFile();

    for (const auto& wav : soundFilesToSave)
        wav.deleteFile();

    piece.removeChangeListener (this);
    document.removeChangeListener (this);

    if (keyListenerTarget != nullptr)
    {
        keyListenerTarget->removeKeyListener (this);
        keyListenerTarget->removeKeyListener (commandManager.getKeyMappings());
    }
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (controls::sidebarBackground);
    g.fillRect (sidebar.getBounds());

    // The staff view doesn't cover the space for the scroll bar, so fill that in to match, and the
    // volume dials sit on the same paper.
    g.setColour (StaffView::paperColour);
    g.fillRect (staffViewport.getBounds());
    g.fillRect (volumeColumn.getBounds());

    // The ruler's band goes on over the volume dials, so the chosen page's tab joins it all along.
    const auto rulerBand = positionRuler.getBounds().withLeft (volumeColumn.getX());
    g.setColour (PageTabs::chosenColour);
    g.fillRect (rulerBand);
    g.setColour (juce::Colours::black.withAlpha (0.15f));
    g.fillRect (rulerBand.withTop (rulerBand.getBottom() - 1));

    g.setColour (juce::Colours::black.withAlpha (0.15f));
    g.fillRect (0, toolbarHeight - 1, getWidth(), 1);
    g.fillRect (sidebar.getRight(), toolbarHeight, 1, getHeight() - toolbarHeight);
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds();

    auto toolbar = bounds.removeFromTop (toolbarHeight).reduced (12, 8);
    playButton.setBounds (toolbar.removeFromLeft (46));
    pauseButton.setBounds (toolbar.removeFromLeft (40));
    recordButton.setBounds (toolbar.removeFromLeft (46));

    for (auto* button : { &backButton, &rewindButton, &fastForwardButton })
        button->setBounds (toolbar.removeFromLeft (40));

    loopButton.setBounds (toolbar.removeFromLeft (44));
    playModeButton.setBounds (toolbar.removeFromLeft (56));
    toolbar.removeFromLeft (toolbarGap);
    mp3Button.setBounds (toolbar.removeFromLeft (44));
    toolbar.removeFromLeft (toolbarGap);
    metronomeButton.setBounds (toolbar.removeFromLeft (44));
    toolbar.removeFromLeft (16);
    tempoLabel.setBounds (toolbar.removeFromLeft (34));
    toolbar.removeFromLeft (4);
    tempoEditor.setBounds (toolbar.removeFromLeft (56));
    toolbar.removeFromLeft (2);
    auto arrows = toolbar.removeFromLeft (18);
    tempoUpButton.setBounds (arrows.removeFromTop (arrows.getHeight() / 2));
    tempoDownButton.setBounds (arrows);
    toolbar.removeFromLeft (20);
    partBox.setBounds (toolbar.removeFromLeft (150));
    toolbar.removeFromLeft (20);

    effectsButton.setBounds (toolbar.removeFromRight (84));
    toolbar.removeFromRight (8);

    for (auto& track : tracks)
        track.panel->setBounds (toolbar);

    sidebar.setBounds (bounds.removeFromLeft (sidebarWidth));
    bounds.removeFromLeft (1);

    const auto contentWidth = sidebar.getMaximumVisibleWidth();
    const auto panelWidth = contentWidth - 2 * sidebarPadding;
    scorePanel.setBounds (sidebarPadding, sidebarPadding, panelWidth, scorePanel.getIdealHeight());
    sidebarContent.setSize (contentWidth, scorePanel.getBottom() + sidebarPadding);

    keyboard.setBounds (bounds.removeFromBottom (keyboardHeight));
    keyboard.setKeyWidth ((float) keyboard.getWidth() / (float) numWhiteKeys);

    // The pages' tabs go along the top, the ruler under them, and the volume dials and staves under that.
    pageTabs.setBounds (bounds.removeFromTop (PageTabs::height));
    auto rulerRow = bounds.removeFromTop (PositionRuler::height);
    volumeColumn.setBounds (bounds.removeFromLeft (volumeColumnWidth));
    staffViewport.setBounds (bounds);
    positionRuler.setBounds (rulerRow.withLeft (staffViewport.getX()));

    // The note length and dynamic buttons stay in the top left corner of the score, over the
    // staves as they scroll, and the eraser in the top right corner, clear of the scroll bar.
    noteLengthPicker.setTopLeftPosition (volumeColumn.getX() + 10, staffViewport.getY() + toolButtonsTop);
    dynamicPicker.setTopLeftPosition (noteLengthPicker.getRight() + 14, noteLengthPicker.getY());
    eraserButton.setBounds (staffViewport.getRight() - staffViewport.getScrollBarThickness() - eraserMargin - EraserButton::buttonWidth,
                            noteLengthPicker.getY(), EraserButton::buttonWidth, EraserButton::buttonHeight);
    selectButton.setBounds (eraserButton.getBounds().withWidth (selectButtonWidth).translated (-selectButtonWidth - editButtonGap, 0));
    editButton.setBounds (selectButton.getBounds().withWidth (editButtonWidth).translated (-editButtonWidth - editButtonGap, 0));

    staffSystems.setMinimumSize (staffViewport.getWidth() - staffViewport.getScrollBarThickness(),
                                 staffViewport.getHeight() - staffViewport.getScrollBarThickness());
    positionVolumeDials();
    positionMeasureButtons();
}

void MainComponent::parentHierarchyChanged()
{
    // Esc is listened for on the whole window, whichever control has the keyboard focus.
    auto* top = getTopLevelComponent();

    if (top == keyListenerTarget.getComponent())
        return;

    if (keyListenerTarget != nullptr)
    {
        keyListenerTarget->removeKeyListener (this);
        keyListenerTarget->removeKeyListener (commandManager.getKeyMappings());
    }

    keyListenerTarget = top;
    top->addKeyListener (this);
    top->addKeyListener (commandManager.getKeyMappings());   // the menu's shortcuts
    showDocumentTitle();
}

bool MainComponent::keyPressed (const juce::KeyPress& key, juce::Component* originatingComponent)
{
    // Typing the tempo, the up and down arrow keys step it.
    if (originatingComponent == &tempoEditor && (key == juce::KeyPress::upKey || key == juce::KeyPress::downKey))
    {
        stepTempo (key == juce::KeyPress::upKey ? 1 : -1);
        return true;
    }

    // The space bar plays, or pauses what's playing, even while the tempo's being typed, which
    // can't have a space in it. A box that can, such as a page's name, keeps it for itself.
    if (key == juce::KeyPress::spaceKey)
    {
        if (instrumentHost.getPlaybackPosition().has_value())
            pausePlayback();
        else
            togglePlayback();

        return true;
    }

    // Esc lets go of the selected measures.
    if (key == juce::KeyPress::escapeKey && selectedMeasures.has_value())
    {
        selectMeasures ({}, true);
        return true;
    }

    if (key == juce::KeyPress::escapeKey && (placingScale || placingRhythm))
    {
        stopPlacingNotes();
        return true;
    }

    if (key == juce::KeyPress::escapeKey && getChordEditor() != nullptr)
    {
        closeChordEditor();
        return true;
    }

    return false;
}

std::unique_ptr<juce::ComponentTraverser> MainComponent::createKeyboardFocusTraverser()
{
    // Otherwise a click on Play or the staves, with nothing focused, gives the focus to the
    // tempo, which keeps the space bar and the keys that play the piano for itself.
    struct Traverser final : public juce::KeyboardFocusTraverser
    {
        juce::Component* getDefaultComponent (juce::Component*) override { return nullptr; }
    };

    return std::make_unique<Traverser>();
}

void MainComponent::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source == &document)
    {
        // The file, whether there are unsaved changes, or the recent scores have changed.
        showDocumentTitle();
        menuItemsChanged();
        return;
    }

    scoreChanged();
}

void MainComponent::scoreChanged()
{
    // Parts may have been added or taken out, or a whole new score loaded.
    updateTracks();
    updateRemoveMeasureButton();

    // The tempo can change by loading a score, so show it unless it's being typed.
    if (! tempoEditor.hasKeyboardFocus (false))
        tempoEditor.setText (formatNumber (score.getBeatsPerMinute()), false);

    updateTempoArrows();

    // The selected measures stay selected while they're there.
    if (selectedMeasures.has_value() && selectedMeasures->second >= score.getNumMeasures())
        selectMeasures ({}, true);
    else
        positionSelectionButtons();

    // Where each page plays from stays within it, as measures come and go and the time signature changes.
    playbackStarts.resize ((size_t) piece.getNumPages(), 0.0);

    for (int page = 0; page < piece.getNumPages(); ++page)
    {
        const auto& written = piece.getPage (page);
        auto& start = playbackStarts[(size_t) page];
        start = juce::jlimit (0.0, (double) (written.getNumMeasures() * written.getBeatsPerMeasure() - 1), start);
    }

    positionRuler.repaint();

    // Changes are heard as soon as playback reaches them. If the repeats or the pages have
    // changed what's played when, it carries on from the same place, the same time through.
    if (instrumentHost.getPlaybackPosition().has_value())
    {
        const auto before = playedMeasures;
        updatePerformance();
        followPerformanceChange (before);
    }

    showPlaybackPosition();
    showHeldNotes();
}

void MainComponent::pagesChanged()
{
    const auto switched = piece.getCurrentPage() != shownPage;
    shownPage = piece.getCurrentPage();
    juce::StringArray names;

    for (int page = 0; page < piece.getNumPages(); ++page)
        names.add (piece.getPageName (page));

    pageTabs.setPages (names, piece.getCurrentPage());

    if (switched)
    {
        // A chord being edited is on the page that was being edited, and the new page is shown
        // from its start. Another page plays from where it plays from, not where one was paused.
        // The measures selected were on the page that was being edited too.
        pausedAt.reset();
        selectMeasures ({}, true);
        closeChordEditor();
        staffViewport.setViewPosition (0, staffViewport.getViewPositionY());

        // Playing a page, it's the one being edited, so it goes on with the one chosen now, from where that plays from.
        if (instrumentHost.getPlaybackPosition().has_value() && ! isPlayingWholePiece())
        {
            if (instrumentHost.isRecording())
            {
                recordPlayedKeys();
                recorder.finish ({});
            }

            startPlaying();
        }
    }

    scoreChanged();
}

void MainComponent::copyPage (int page)
{
    if (! juce::isPositiveAndBelow (page, piece.getNumPages()))
        return;

    // The copy plays from where its page does.
    if (juce::isPositiveAndBelow (page, (int) playbackStarts.size()))
        playbackStarts.insert (playbackStarts.begin() + page + 1, playbackStarts[(size_t) page]);

    piece.copyPage (page);
}

void MainComponent::deletePage (int page)
{
    if (piece.getNumPages() <= 1 || ! juce::isPositiveAndBelow (page, piece.getNumPages()))
        return;

    // A page with a name is asked about by its name.
    const auto& name = piece.getPage (page).getName();
    const auto title = name.isNotEmpty() ? "Delete " + juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x9c")) + name
                                               + juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x9d?"))
                                         : "Delete page " + juce::String (page + 1) + "?";

    juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                      .withIconType (juce::MessageBoxIconType::QuestionIcon)
                                      .withTitle (title)
                                      .withMessage ("Its music and settings will be taken out of the score. Undo puts them back.")
                                      .withButton ("Delete")
                                      .withButton ("Cancel")
                                      .withAssociatedComponent (this),
                                  [safeThis = juce::Component::SafePointer (this), page] (int result)
                                  {
                                      // Delete is 1, and Cancel 0.
                                      if (safeThis == nullptr || result != 1 || safeThis->piece.getNumPages() <= 1
                                          || ! juce::isPositiveAndBelow (page, safeThis->piece.getNumPages()))
                                          return;

                                      if (auto& starts = safeThis->playbackStarts; juce::isPositiveAndBelow (page, (int) starts.size()))
                                          starts.erase (starts.begin() + page);

                                      safeThis->piece.removePage (page);
                                  });
}

void MainComponent::movePage (int from, int to)
{
    const auto numPages = piece.getNumPages();

    if (from == to || ! juce::isPositiveAndBelow (from, numPages) || ! juce::isPositiveAndBelow (to, numPages))
        return;

    // Each page keeps where it plays from.
    if ((int) playbackStarts.size() == numPages)
    {
        const auto start = playbackStarts[(size_t) from];
        playbackStarts.erase (playbackStarts.begin() + from);
        playbackStarts.insert (playbackStarts.begin() + to, start);
    }

    // The page being edited is still the one shown, though it may have another number now.
    if (shownPage == from)
        shownPage = to;
    else if (from < shownPage && to >= shownPage)
        --shownPage;
    else if (from > shownPage && to <= shownPage)
        ++shownPage;

    // The page moved is the one edited, as it's been picked up.
    piece.movePage (from, to);
    piece.choosePage (to);
}

//==============================================================================
void MainComponent::setActivePart (int part)
{
    activePart = part;
    activePartId = score.getPartId (part);
    instrumentHost.setActivePart (part);
    scorePanel.setPart (part);
    partBox.setSelectedId (part + 1, juce::dontSendNotification);

    for (size_t p = 0; p < tracks.size(); ++p)
    {
        tracks[p].panel->setVisible ((int) p == part);
        staffSystems.views[p]->setActive ((int) p == part);
    }

    // The chord window is for the active part's chords.
    if (const auto* editor = getChordEditor(); editor != nullptr && editor->getPart() != part)
        closeChordEditor();
}

void MainComponent::updatePartBox()
{
    partBox.clear (juce::dontSendNotification);

    for (int part = 0; part < score.getNumParts(); ++part)
        partBox.addItem ("Instrument " + juce::String (part + 1), part + 1);

    partBox.setSelectedId (activePart + 1, juce::dontSendNotification);
}

void MainComponent::insertTrack (int index, bool reloadFromSettings)
{
    // The part's found by its id in what the track's controls do, as its number can change.
    const auto id = score.getPartId (index);
    Track track { id, std::make_unique<InstrumentPanel> (instrumentHost, index, settings, reloadFromSettings),
                  std::make_unique<controls::ClickableDial>(), std::make_unique<controls::BinButton>() };

    auto& panel = *track.panel;
    panel.onInstrumentChanged = [this] { document.changed(); };
    panel.onStatusChanged = [this] { showPartTitles(); };
    addChildComponent (panel);

    // A volume dial beside the part's staves, from off up to +6 dB. Clicking it without turning it
    // mutes the part, or unmutes it. Both are remembered from one run of the app to the next, and
    // saved with the score.
    auto& dial = *track.dial;

    // The text comes first, so the dial shows it from the start.
    dial.textFromValueFunction = [this, id] (double decibels)
    {
        if (const auto part = score.findPart (id); part >= 0 && part < instrumentHost.getNumParts() && instrumentHost.isMuted (part))
            return juce::String ("Muted");

        if (decibels <= InstrumentHost::minVolume)
            return juce::String ("Off");

        return (decibels > 0.05 ? "+" : "") + juce::String (std::abs (decibels) < 0.05 ? 0.0 : decibels, 1) + " dB";
    };
    dial.valueFromTextFunction = [] (const juce::String& text)
    {
        return text.trim().equalsIgnoreCase ("off") ? (double) InstrumentHost::minVolume : text.getDoubleValue();
    };

    dial.setLookAndFeel (&dialLookAndFeel);
    dial.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    dial.setTextBoxStyle (juce::Slider::TextBoxBelow, true, volumeColumnWidth, volumeTextHeight);
    dial.setRange (InstrumentHost::minVolume, InstrumentHost::maxVolume, 0.1);
    dial.setSkewFactorFromMidPoint (-12.0);
    dial.setWantsKeyboardFocus (false);
    dial.setColour (juce::Slider::textBoxTextColourId, controls::secondaryText);
    dial.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    dial.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);

    // Turning a muted dial unmutes it, rather than changing a volume that can't be heard.
    dial.onValueChange = [this, id, &dial]
    {
        const auto part = score.findPart (id);

        if (part < 0)
            return;

        if (instrumentHost.isMuted (part))
            setMuted (part, false, true);

        setVolume (part, (float) dial.getValue(), true);
    };
    dial.onClick = [this, id]
    {
        if (const auto part = score.findPart (id); part >= 0)
            setMuted (part, ! instrumentHost.isMuted (part), true);
    };
    volumeColumn.addAndMakeVisible (dial);

    // And under it, a button for taking the part out
    auto& deleteButton = *track.deleteButton;
    deleteButton.onClick = [this, id]
    {
        if (const auto part = score.findPart (id); part >= 0)
            deletePart (part);
    };
    staffSystems.addAndMakeVisible (deleteButton);

    tracks.insert (tracks.begin() + index, std::move (track));

    // Clicking the part's staff, or its chord buttons, makes it the active part.
    auto& view = staffSystems.insertView (index, index);
    view.onClicked = [this, &view] { setActivePart (view.getPart()); };
    view.onChordButtonClicked = [this, &view] (int measure) { editChord (view.getPart(), measure); };

    // An eraser stroke is undone in one go, however much it takes out.
    view.onEraseStarted = [this] { history.beginGesture(); };
    view.onEraseFinished = [this] { history.endGesture(); };

    // Adding from the scale is done once the notes have gone in.
    view.onScalePlaced = [this] { stopPlacingScale(); };
    view.onRhythmPlaced = [this] { stopPlacingRhythm(); };

    // It does what clicks on the other parts do.
    view.setNoteLength (noteLengthPicker.getLength());
    view.setStaccato (noteLengthPicker.isStaccato());

    if (eraserButton.getToggleState())
        view.setErasing (true);
    else if (const auto marking = dynamicPicker.getChoice())
        view.setMarking (marking);

    view.setEditing (editButton.getToggleState());

    // Selecting measures in any part selects them in all of them, and moves them in all of them.
    view.onMeasuresSelected = [this] (std::optional<std::pair<int, int>> measures, bool finished) { selectMeasures (measures, finished); };
    view.onMoveTargetChanged = [this] (std::optional<int> barline) { showMoveTarget (barline); };
    view.onMeasuresMoved = [this] (int barline) { moveSelectedMeasures (barline); };
    view.onSelectionPushed = [this] (int change) { pushSelectedMeasures (change); };
    view.onSelectionDragStarted = [this] { history.beginGesture(); };
    view.onSelectionDragFinished = [this] { history.endGesture(); };
    view.setSelecting (isSelecting());
    view.setMeasureSelection (selectedMeasures);
    view.setMovingMeasures (moveMeasuresButton.getToggleState());

    // A part being added starts at 0 dB, rather than with whatever its number had before.
    setVolume (index, reloadFromSettings ? (float) settings.getDoubleValue (getVolumeKey (index), 0.0) : 0.0f, false);
    setMuted (index, reloadFromSettings && settings.getBoolValue (getMutedKey (index), false), false);
}

void MainComponent::removeTrack (int index)
{
    // Its instrument isn't kept in the settings, as another part will have its number.
    tracks[(size_t) index].panel->forgetInstrument();
    tracks.erase (tracks.begin() + index);
    staffSystems.removeView (index);
}

void MainComponent::updateTracks()
{
    // Tracks are matched to parts by their ids. One whose part has gone, or come before it, goes;
    // a part without one gets a new one, without an instrument. The instruments follow along.
    auto firstChange = std::numeric_limits<int>::max();

    for (int index = 0; index < juce::jmax (score.getNumParts(), (int) tracks.size());)
    {
        if (index < (int) tracks.size() && index < score.getNumParts() && tracks[(size_t) index].partId == score.getPartId (index))
        {
            ++index;
            continue;
        }

        firstChange = juce::jmin (firstChange, index);

        if (index < (int) tracks.size() && score.findPart (tracks[(size_t) index].partId) < index)
        {
            removeTrack (index);
            instrumentHost.removePart (index);
            continue;
        }

        instrumentHost.insertPart (index);
        insertTrack (index, false);
        ++index;
    }

    if (firstChange == std::numeric_limits<int>::max())
        return;

    // The tracks after the change have new numbers.
    numberTracks();

    // A chord being edited in a part that's moved, or gone, can't be any more.
    if (const auto* editor = getChordEditor(); editor != nullptr && editor->getPart() >= firstChange)
        closeChordEditor();

    // The active part stays active, wherever it is now, or the one after it takes over.
    const auto active = score.findPart (activePartId);
    activePart = active >= 0 ? active : juce::jmin (activePart, score.getNumParts() - 1);
    updatePartBox();
    setActivePart (activePart);

    showPartTitles();
    saveVolumes();
    resized();
}

void MainComponent::numberTracks()
{
    for (size_t index = 0; index < tracks.size(); ++index)
    {
        tracks[index].panel->setPart ((int) index);
        tracks[index].dial->setTooltip ("Instrument " + juce::String (index + 1) + "'s volume. Drag to turn it, or click to mute or unmute.");
        tracks[index].deleteButton->setTooltip (tracks.size() > 1 ? "Delete instrument " + juce::String (index + 1) + ", with its staves"
                                                                  : juce::String ("The only instrument can't be deleted"));
        tracks[index].deleteButton->setEnabled (tracks.size() > 1);
        staffSystems.views[index]->followPart();
    }
}

void MainComponent::addPart()
{
    const auto part = score.addPart();
    setActivePart (part);

    // Scrolled down far enough to see it, and the + under it
    const auto area = staffSystems.views[(size_t) part]->getBounds();
    const auto viewArea = staffViewport.getViewArea();

    if (area.getBottom() > viewArea.getBottom())
        staffViewport.setViewPosition (viewArea.getX(), area.getBottom() + 4 - viewArea.getHeight());
}

void MainComponent::deletePart (int part)
{
    if (score.getNumParts() <= 1)
        return;

    // An empty part, without an instrument, goes straight away. Otherwise, it's asked first.
    const auto* instrument = instrumentHost.getInstrument (part);

    if (piece.isPartEmpty (part) && instrument == nullptr)
    {
        score.removePart (part);
        return;
    }

    const auto name = "instrument " + juce::String (part + 1);
    auto message = juce::String ("Its notes, chords and dynamics will be taken out of the score, on every page");

    if (instrument != nullptr)
        message << ", and its " << instrument->getName() << " unloaded. Undo puts back the music, but the plugin will need loading again.";
    else
        message << ". Undo puts them back.";

    juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                      .withIconType (juce::MessageBoxIconType::QuestionIcon)
                                      .withTitle ("Delete " + name + "?")
                                      .withMessage (message)
                                      .withButton ("Delete")
                                      .withButton ("Cancel")
                                      .withAssociatedComponent (this),
                                  [safeThis = juce::Component::SafePointer (this), id = score.getPartId (part)] (int result)
                                  {
                                      // Delete is 1, and Cancel 0.
                                      if (safeThis != nullptr && result == 1)
                                          safeThis->score.removePart (safeThis->score.findPart (id));
                                  });
}

void MainComponent::copyProgression (int to)
{
    const auto from = activePart;

    if (to == from || ! juce::isPositiveAndBelow (to, score.getNumParts()) || ! score.hasChords (from))
        return;

    // A part with nothing in it can take the progression straight away.
    if (! score.hasNotes (to))
    {
        score.copyChords (from, to);
        return;
    }

    const auto fromName = "instrument " + juce::String (from + 1), toName = "instrument " + juce::String (to + 1);

    juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                      .withIconType (juce::MessageBoxIconType::QuestionIcon)
                                      .withTitle ("Replace " + toName + "'s notes?")
                                      .withMessage ("Copying " + fromName + "'s chord progression to " + toName + " replaces the notes and chords "
                                                    + toName + " has now.")
                                      .withButton ("Replace")
                                      .withButton ("Cancel")
                                      .withAssociatedComponent (this),
                                  [safeThis = juce::Component::SafePointer (this), from, to] (int result)
                                  {
                                      // Replace is 1, and Cancel 0.
                                      if (safeThis != nullptr && result == 1)
                                          safeThis->score.copyChords (from, to);
                                  });
}

void MainComponent::showSaveProgressionPanel (juce::Component& button)
{
    if (! score.hasChords (activePart))
        return;

    auto panel = std::make_unique<SaveProgressionPanel> (getProgression (score, activePart), activePart);
    auto* panelPointer = panel.get();
    const auto close = [panelPointer]
    {
        if (auto* box = panelPointer->findParentComponentOfClass<juce::CallOutBox>())
            box->dismiss();
    };

    panel->onCancel = close;
    panel->onSave = [this, close] (const juce::String& name)
    {
        close();
        saveProgression (name);
    };

    juce::CallOutBox::launchAsynchronously (std::move (panel), getLocalArea (&button, button.getLocalBounds()), this);
}

void MainComponent::saveProgression (const juce::String& name)
{
    auto progression = getProgression (score, activePart);

    if (progression.chords.empty())
        return;

    progression.name = name;
    const auto keep = [safeThis = juce::Component::SafePointer (this), progression]
    {
        if (safeThis == nullptr)
            return;

        safeThis->savedProgressions.save (progression);
        safeThis->scorePanel.setNumSavedProgressions ((int) safeThis->savedProgressions.getAll().size());
    };

    const auto* existing = savedProgressions.find (name);

    if (existing == nullptr)
    {
        keep();
        return;
    }

    juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                      .withIconType (juce::MessageBoxIconType::QuestionIcon)
                                      .withTitle ("Replace " + existing->name.quoted() + "?")
                                      .withMessage ("A progression called " + existing->name.quoted() + " is saved already. Saving this one replaces it.")
                                      .withButton ("Replace")
                                      .withButton ("Cancel")
                                      .withAssociatedComponent (this),
                                  [keep] (int result)
                                  {
                                      // Replace is 1, and Cancel 0.
                                      if (result == 1)
                                          keep();
                                  });
}

void MainComponent::showApplyProgressionPanel (juce::Component& button)
{
    if (savedProgressions.getAll().empty())
        return;

    // With the chord type and clef chosen last, but Automatic note lengths and no rhythm, each time
    auto startingStyle = progressionStyle;
    startingStyle.noteLength.reset();
    startingStyle.rhythm.reset();

    auto panel = std::make_unique<ApplyProgressionPanel> (savedProgressions.getAll(), startingStyle, activePart, getPlaybackMeasure(),
                                                          score.getBeatsPerMeasure(), lastProgressionApplied, savedRhythms.getAll());
    auto* panelPointer = panel.get();
    const auto close = [panelPointer]
    {
        if (auto* box = panelPointer->findParentComponentOfClass<juce::CallOutBox>())
            box->dismiss();
    };

    applyProgressionPanel = panelPointer;
    panel->onCancel = close;
    panel->onDelete = [this] (const juce::String& name) { deleteSavedProgression (name); };
    panel->onApply = [this, close] (const SavedProgression& progression, const ChordStyle& style, int measuresPerChord)
    {
        close();
        applyProgression (progression, style, measuresPerChord);
    };

    juce::CallOutBox::launchAsynchronously (std::move (panel), getLocalArea (&button, button.getLocalBounds()), this);
}

void MainComponent::applyProgression (const SavedProgression& progression, const ChordStyle& style, int measuresPerChord)
{
    // The chord type and clef are kept for next time.
    progressionStyle = style;
    lastProgressionApplied = progression.name;
    settings.setValue (progressionTypeKey, (int) style.type);
    settings.setValue (progressionClefKey, style.staff == Staff::bass ? "bass" : "treble");

    const auto part = activePart;
    const auto first = getPlaybackMeasure();
    const auto chords = progression.createChords (score.getKey(), style);
    const auto count = score.getChordsLength (chords, measuresPerChord);       // in measures

    if (chords.empty())
        return;

    const auto write = [safeThis = juce::Component::SafePointer (this), part, first, chords, measuresPerChord]
    {
        if (safeThis == nullptr || ! juce::isPositiveAndBelow (part, safeThis->score.getNumParts()))
            return;

        // The chord window's measure may be among them.
        safeThis->closeChordEditor();
        safeThis->score.setChords (part, juce::jmin (first, safeThis->score.getNumMeasures()), chords, measuresPerChord);
        safeThis->scrollToMeasure (first);
    };

    if (! score.chordsWouldReplace (part, first, count, style.staff))
    {
        write();
        return;
    }

    // The measures it goes into that are there already
    const auto last = juce::jmin (first + count, score.getNumMeasures()) - 1;
    const auto measures = last > first ? "Measures " + juce::String (first + 1) + controls::fromUTF8 ("\xe2\x80\x93") + juce::String (last + 1)
                                       : "Measure " + juce::String (first + 1);
    const auto partName = "instrument " + juce::String (part + 1);

    juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                      .withIconType (juce::MessageBoxIconType::QuestionIcon)
                                      .withTitle ("Replace " + partName + "'s notes?")
                                      .withMessage (measures + " of " + partName + " already have notes or chords, which the progression's chords take the place of.")
                                      .withButton ("Replace")
                                      .withButton ("Cancel")
                                      .withAssociatedComponent (this),
                                  [write] (int result)
                                  {
                                      // Replace is 1, and Cancel 0.
                                      if (result == 1)
                                          write();
                                  });
}

void MainComponent::deleteSavedProgression (const juce::String& name)
{
    if (savedProgressions.find (name) == nullptr)
        return;

    juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                      .withIconType (juce::MessageBoxIconType::QuestionIcon)
                                      .withTitle ("Delete " + name.quoted() + "?")
                                      .withMessage ("The progression is taken out of the ones saved, for every score. This can't be undone.")
                                      .withButton ("Delete")
                                      .withButton ("Cancel")
                                      .withAssociatedComponent (this),
                                  [safeThis = juce::Component::SafePointer (this), name] (int result)
                                  {
                                      if (safeThis == nullptr || result != 1)
                                          return;

                                      auto& library = safeThis->savedProgressions;
                                      library.remove (name);
                                      safeThis->scorePanel.setNumSavedProgressions ((int) library.getAll().size());

                                      // The box shows what's left, and closes once there's nothing.
                                      if (auto* panel = safeThis->applyProgressionPanel.getComponent())
                                      {
                                          if (library.getAll().empty())
                                          {
                                              if (auto* box = panel->findParentComponentOfClass<juce::CallOutBox>())
                                                  box->dismiss();
                                          }
                                          else
                                          {
                                              panel->setProgressions (library.getAll());
                                          }
                                      }
                                  });
}

int MainComponent::getPlaybackMeasure() const
{
    auto beat = playbackStarts[(size_t) piece.getCurrentPage()];

    if (const auto played = instrumentHost.getPlaybackPosition())
        if (const auto at = toWrittenPosition (*played); at.has_value() && at->page == piece.getCurrentPage())
            beat = at->beat;

    return juce::jlimit (0, score.getNumMeasures(), (int) std::floor (beat / score.getBeatsPerMeasure() + 1.0e-9));
}

void MainComponent::showScalePanel (juce::Component& button)
{
    auto panel = std::make_unique<ScalePanel> (lastScale);
    auto* panelPointer = panel.get();

    // Scale or Random closes the box, and the next click on a staff adds the notes.
    panel->onPlace = [this, panelPointer] (ScaleSettings chosen)
    {
        lastScale = chosen;

        if (auto* box = panelPointer->findParentComponentOfClass<juce::CallOutBox>())
            box->dismiss();

        startPlacingScale (chosen);
    };

    juce::CallOutBox::launchAsynchronously (std::move (panel), getLocalArea (&button, button.getLocalBounds()), this);
}

void MainComponent::startPlacingScale (const ScaleSettings& scale)
{
    // Whatever tool was chosen, it's back to notes afterwards.
    setNoteLength (noteLengthPicker.getLength());
    placingScale = true;

    for (auto& view : staffSystems.views)
        view->setScalePlacement (scale);
}

void MainComponent::stopPlacingScale()
{
    placingScale = false;

    for (auto& view : staffSystems.views)
        view->setScalePlacement ({});
}

void MainComponent::stopPlacingNotes()
{
    if (placingScale)
        stopPlacingScale();

    if (placingRhythm)
        stopPlacingRhythm();
}

void MainComponent::showSaveRhythmPanel (juce::Component& button)
{
    if (! score.hasNotes (activePart) && ! score.hasChords (activePart))
        return;

    // From the measure it plays from, or the last, if it plays from the end
    const auto part = activePart;
    const auto first = juce::jmin (getPlaybackMeasure(), score.getNumMeasures() - 1);

    // A name of its own to start with
    auto number = (int) savedRhythms.getAll().size() + 1;

    while (savedRhythms.find ("Rhythm " + juce::String (number)) != nullptr)
        ++number;

    auto panel = std::make_unique<SaveRhythmPanel> (part, first, getRhythmLengthChoices (score, part, first),
                                                    [this, part, first] (int count) { return getRhythm (score, part, first, count); },
                                                    "Rhythm " + juce::String (number));
    auto* panelPointer = panel.get();
    const auto close = [panelPointer]
    {
        if (auto* box = panelPointer->findParentComponentOfClass<juce::CallOutBox>())
            box->dismiss();
    };

    panel->onCancel = close;
    panel->onSave = [this, close, first] (const juce::String& name, int count)
    {
        close();
        saveRhythm (name, first, count);
    };

    juce::CallOutBox::launchAsynchronously (std::move (panel), getLocalArea (&button, button.getLocalBounds()), this);
}

void MainComponent::saveRhythm (const juce::String& name, int firstMeasure, int numMeasures)
{
    auto rhythm = getRhythm (score, activePart, firstMeasure, numMeasures);

    if (! rhythm.hasNotes() || name.isEmpty())
        return;

    rhythm.name = name;
    const auto keep = [safeThis = juce::Component::SafePointer (this), rhythm]
    {
        if (safeThis == nullptr)
            return;

        safeThis->savedRhythms.save (rhythm);
        safeThis->scorePanel.setNumSavedRhythms ((int) safeThis->savedRhythms.getAll().size());
    };

    const auto* existing = savedRhythms.find (name);

    if (existing == nullptr)
    {
        keep();
        return;
    }

    juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                      .withIconType (juce::MessageBoxIconType::QuestionIcon)
                                      .withTitle ("Replace " + existing->name.quoted() + "?")
                                      .withMessage ("A rhythm called " + existing->name.quoted() + " is saved already. Saving this one replaces it.")
                                      .withButton ("Replace")
                                      .withButton ("Cancel")
                                      .withAssociatedComponent (this),
                                  [keep] (int result)
                                  {
                                      // Replace is 1, and Cancel 0.
                                      if (result == 1)
                                          keep();
                                  });
}

void MainComponent::showApplyRhythmPanel (juce::Component& button)
{
    if (savedRhythms.getAll().empty())
        return;

    auto panel = std::make_unique<ApplyRhythmPanel> (savedRhythms.getAll(), lastRhythmTimes, lastRhythmPlaced);
    auto* panelPointer = panel.get();
    const auto close = [panelPointer]
    {
        if (auto* box = panelPointer->findParentComponentOfClass<juce::CallOutBox>())
            box->dismiss();
    };

    applyRhythmPanel = panelPointer;
    panel->onCancel = close;
    panel->onDelete = [this] (const juce::String& name) { deleteSavedRhythm (name); };
    panel->onPlace = [this, close] (const SavedRhythm& rhythm, int times)
    {
        close();
        startPlacingRhythm (rhythm, times);
    };

    juce::CallOutBox::launchAsynchronously (std::move (panel), getLocalArea (&button, button.getLocalBounds()), this);
}

void MainComponent::startPlacingRhythm (const SavedRhythm& rhythm, int times)
{
    // Whatever tool was chosen, it's back to notes afterwards.
    setNoteLength (noteLengthPicker.getLength());
    lastRhythmTimes = times;
    lastRhythmPlaced = rhythm.name;
    placingRhythm = true;

    for (auto& view : staffSystems.views)
        view->setRhythmPlacement (StaffView::RhythmPlacement { rhythm, times });
}

void MainComponent::stopPlacingRhythm()
{
    placingRhythm = false;

    for (auto& view : staffSystems.views)
        view->setRhythmPlacement ({});
}

void MainComponent::deleteSavedRhythm (const juce::String& name)
{
    if (savedRhythms.find (name) == nullptr)
        return;

    juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                      .withIconType (juce::MessageBoxIconType::QuestionIcon)
                                      .withTitle ("Delete " + name.quoted() + "?")
                                      .withMessage ("The rhythm is taken out of the ones saved, for every score. This can't be undone.")
                                      .withButton ("Delete")
                                      .withButton ("Cancel")
                                      .withAssociatedComponent (this),
                                  [safeThis = juce::Component::SafePointer (this), name] (int result)
                                  {
                                      if (safeThis == nullptr || result != 1)
                                          return;

                                      auto& library = safeThis->savedRhythms;
                                      library.remove (name);
                                      safeThis->scorePanel.setNumSavedRhythms ((int) library.getAll().size());

                                      // The box shows what's left, and closes once there's nothing.
                                      if (auto* panel = safeThis->applyRhythmPanel.getComponent())
                                      {
                                          if (library.getAll().empty())
                                          {
                                              if (auto* box = panel->findParentComponentOfClass<juce::CallOutBox>())
                                                  box->dismiss();
                                          }
                                          else
                                          {
                                              panel->setRhythms (library.getAll());
                                          }
                                      }
                                  });
}

void MainComponent::setNoteLength (double beats)
{
    stopPlacingNotes();

    stopSelecting();

    noteLengthPicker.setLength (beats);
    dynamicPicker.setChoice ({});
    eraserButton.setToggleState (false, juce::dontSendNotification);

    for (auto& view : staffSystems.views)
    {
        view->setNoteLength (beats);
        view->setMarking ({});
    }
}

void MainComponent::setStaccato (bool staccato)
{
    noteLengthPicker.setStaccato (staccato);

    for (auto& view : staffSystems.views)
        view->setStaccato (staccato);

    // Back to adding notes, with the length chosen
    setNoteLength (noteLengthPicker.getLength());
}

void MainComponent::setMarking (std::optional<music::Marking> marking)
{
    stopPlacingNotes();

    if (! marking.has_value())
    {
        setNoteLength (noteLengthPicker.getLength());
        return;
    }

    stopSelecting();
    dynamicPicker.setChoice (marking);
    noteLengthPicker.setChoiceShown (false);
    eraserButton.setToggleState (false, juce::dontSendNotification);

    for (auto& view : staffSystems.views)
        view->setMarking (marking);
}

void MainComponent::setErasing (bool shouldErase)
{
    stopPlacingNotes();

    if (! shouldErase)
    {
        setNoteLength (noteLengthPicker.getLength());
        return;
    }

    setEditing (false);
    stopSelecting();
    eraserButton.setToggleState (true, juce::dontSendNotification);
    dynamicPicker.setChoice ({});
    noteLengthPicker.setChoiceShown (false);

    for (auto& view : staffSystems.views)
        view->setErasing (true);
}

void MainComponent::setEditing (bool shouldEdit)
{
    // Editing goes with adding notes or marking, as before, but not with erasing.
    if (shouldEdit && eraserButton.getToggleState())
        setErasing (false);

    if (shouldEdit)
        stopSelecting();

    editButton.setToggleState (shouldEdit, juce::dontSendNotification);

    for (auto& view : staffSystems.views)
        view->setEditing (shouldEdit);
}

//==============================================================================
void MainComponent::setSelecting (bool shouldSelect)
{
    // Off, it's back to adding notes of the length chosen.
    if (! shouldSelect)
    {
        stopSelecting();
        setNoteLength (noteLengthPicker.getLength());
        return;
    }

    stopPlacingNotes();

    // Clicks select measures, rather than adding notes, marking, editing or erasing.
    setEditing (false);
    eraserButton.setToggleState (false, juce::dontSendNotification);
    dynamicPicker.setChoice ({});
    noteLengthPicker.setChoiceShown (false);
    selectButton.setToggleState (true, juce::dontSendNotification);

    for (auto& view : staffSystems.views)
    {
        view->setErasing (false);
        view->setSelecting (true);
    }
}

void MainComponent::stopSelecting()
{
    if (! isSelecting())
        return;

    selectMeasures ({}, true);
    selectButton.setToggleState (false, juce::dontSendNotification);
    noteLengthPicker.setChoiceShown (true);

    for (auto& view : staffSystems.views)
        view->setSelecting (false);
}

void MainComponent::selectMeasures (std::optional<std::pair<int, int>> measures, bool finished)
{
    // A new selection being dragged out isn't being moved.
    if (! finished || ! measures.has_value())
        setMovingMeasures (false);

    selectedMeasures = measures;
    selectionFinished = finished && measures.has_value();

    for (auto& view : staffSystems.views)
        view->setMeasureSelection (measures);

    positionSelectionButtons();
}

void MainComponent::deleteSelectedMeasures()
{
    if (! selectedMeasures.has_value())
        return;

    // The chord window's measure may be among them, or move.
    const auto [first, last] = *selectedMeasures;
    closeChordEditor();
    selectMeasures ({}, true);
    score.removeMeasures (first, last - first + 1);
}

void MainComponent::setMovingMeasures (bool shouldMove)
{
    moveMeasuresButton.setToggleState (shouldMove && selectedMeasures.has_value(), juce::dontSendNotification);

    for (auto& view : staffSystems.views)
        view->setMovingMeasures (moveMeasuresButton.getToggleState());

    showMoveTarget ({});
    positionSelectionButtons();
}

void MainComponent::moveSelectedMeasures (int barline)
{
    if (! selectedMeasures.has_value())
        return;

    const auto [first, last] = *selectedMeasures;
    const auto count = last - first + 1;
    showMoveTarget ({});
    closeChordEditor();

    // They stay selected where they go, the measures numbered as they are now.
    if (score.moveMeasures (first, count, barline))
    {
        const auto movedTo = barline < first ? barline : barline - count;
        selectMeasures (std::pair { movedTo, movedTo + count - 1 }, true);
    }
}

void MainComponent::pushSelectedMeasures (int change)
{
    if (! selectedMeasures.has_value() || change == 0)
        return;

    const auto [first, last] = *selectedMeasures;
    closeChordEditor();

    // An empty measure goes in before them, and they stay selected where they go; or the one
    // before them comes out, and they're selected where they'll be first, so they stay selected.
    if (change > 0)
    {
        if (score.insertMeasures (first, 1))
            selectMeasures (std::pair { first + 1, last + 1 }, true);
    }
    else if (first > 0)
    {
        selectMeasures (std::pair { first - 1, last - 1 }, true);
        score.removeMeasures (first - 1, 1);
        positionSelectionButtons();
    }
}

void MainComponent::showMoveTarget (std::optional<int> barline)
{
    for (auto& view : staffSystems.views)
        view->setMoveTarget (barline);
}

void MainComponent::positionSelectionButtons()
{
    const auto show = isSelecting() && selectionFinished && selectedMeasures.has_value() && ! staffSystems.views.empty()
                   && selectedMeasures->second < score.getNumMeasures();

    deleteMeasuresButton.setVisible (show);
    moveMeasuresButton.setVisible (show);

    if (! show)
        return;

    // Over the part of the selection that's in view, or the side of the view nearest it,
    // halfway down what's in view
    const auto& view = *staffSystems.views[0];
    const auto selection = view.getMeasureArea (selectedMeasures->first).getUnion (view.getMeasureArea (selectedMeasures->second)).translated (view.getX(), 0);
    const auto viewArea = staffViewport.getViewArea();
    const auto width = deleteMeasuresWidth + editButtonGap + moveMeasuresWidth;
    const auto left = juce::jmax (selection.getX(), viewArea.getX()), right = juce::jmin (selection.getRight(), viewArea.getRight());
    const auto centreX = left <= right ? (left + right) / 2
                       : selection.getRight() < viewArea.getX() ? viewArea.getX() + width / 2
                                                                : viewArea.getRight() - width / 2;
    auto pair = juce::Rectangle<int> (width, selectionButtonHeight).withCentre ({ centreX, viewArea.getCentreY() });
    deleteMeasuresButton.setBounds (pair.removeFromLeft (deleteMeasuresWidth));
    moveMeasuresButton.setBounds (pair.removeFromRight (moveMeasuresWidth));

    for (auto* button : { &deleteMeasuresButton, &moveMeasuresButton })
        button->toFront (false);

    // A page keeps at least one measure. All of them can still be pushed on, past the end.
    const auto all = selectedMeasures->first == 0 && selectedMeasures->second == score.getNumMeasures() - 1;
    deleteMeasuresButton.setEnabled (! all);
    deleteMeasuresButton.setTooltip (all ? "All the measures are selected, and a page keeps at least one"
                                         : "Delete the selected measures, in every instrument; the measures after them move up");
    moveMeasuresButton.setTooltip (moveMeasuresButton.getToggleState() ? "Drag the selected measures between others, or on past the end for empty measures before them. Click Move again to stop."
                                                                       : "Move the selected measures: then drag them between others, or on past the end for empty measures before them");
}

void MainComponent::showPartTitles()
{
    for (size_t part = 0; part < tracks.size(); ++part)
        staffSystems.views[part]->setTitle (getPartTitle ((int) part));

    // The effects page names the instruments too.
    if (effectsWindow != nullptr)
        effectsWindow->refresh();
}

juce::String MainComponent::getPartTitle (int part) const
{
    return "Instrument " + juce::String (part + 1) + juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 ")) + tracks[(size_t) part].panel->getStatus();
}

void MainComponent::showEffects()
{
    if (effectsWindow != nullptr)
    {
        effectsWindow->toFront (true);
        return;
    }

    effectsWindow = std::make_unique<EffectsWindow> (effectRack,
                                                     [this] (int part) { return juce::isPositiveAndBelow (part, (int) tracks.size()) ? getPartTitle (part) : juce::String(); },
                                                     [this] { effectsWindow = nullptr; });
}

void MainComponent::saveEffects()
{
    settings.setValue (effectsKey, juce::JSON::toString (effectRack.toJSON(), true));
    settings.saveIfNeeded();
}

void MainComponent::reportFailedEffects (const juce::StringArray& failed, const juce::String& from)
{
    if (! failed.isEmpty())
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't load some effects",
                                                "These effects " + from + ", but couldn't be loaded, so they've been left out: "
                                                    + failed.joinIntoString (", ") + ".");
}

void MainComponent::setVolume (int part, float decibels, bool changedOnDial)
{
    // To a tenth of a decibel, as the dial goes, without the dial's rounding errors
    decibels = juce::jlimit (InstrumentHost::minVolume, InstrumentHost::maxVolume, std::round (decibels * 10.0f) / 10.0f);
    instrumentHost.setVolume (part, decibels);
    settings.setValue (getVolumeKey (part), decibels);
    tracks[(size_t) part].dial->setValue (decibels, juce::dontSendNotification);

    if (changedOnDial)
        document.changed();
}

void MainComponent::setMuted (int part, bool muted, bool changedOnDial)
{
    instrumentHost.setMuted (part, muted);
    settings.setValue (getMutedKey (part), muted);

    auto& dial = *tracks[(size_t) part].dial;
    dial.getProperties().set ("muted", muted);
    dial.updateText();
    dial.repaint();

    if (changedOnDial)
        document.changed();
}

void MainComponent::positionVolumeDials()
{
    // Each dial is centred on its part's staves, wherever they've scrolled to.
    const auto scrolled = staffViewport.getViewPositionY();

    for (size_t part = 0; part < tracks.size() && part < staffSystems.views.size(); ++part)
    {
        const auto& view = *staffSystems.views[part];
        const auto staves = view.getStavesRange();
        const auto centreY = view.getBounds().getY() + staves.getStart() + staves.getLength() / 2 - scrolled;
        const auto dialHeight = volumeDialSize + volumeTextHeight;

        tracks[part].dial->setBounds (0, centreY - dialHeight / 2, volumeColumnWidth, dialHeight);
    }
}

void MainComponent::positionMeasureButtons()
{
    if (staffSystems.views.empty())
        return;

    // Just after the final barline, so they scroll sideways with the staves, but halfway down
    // what's in view, however far the staves have scrolled up or down: + and − side by side,
    // over Clone
    constexpr int gap = 6;
    const auto viewArea = staffViewport.getViewArea();
    const auto columnX = staffSystems.views[0]->getFinalBarlineX() + measureButtonsGap + measureButtonsWidth / 2;
    auto stack = juce::Rectangle<int> (measureButtonsWidth, measureButtonSize + 2 * gap + cloneButtonHeight)
                     .withCentre ({ columnX, viewArea.getCentreY() });

    // Each part's delete button, in line with them, level with the middle of its staves. They
    // scroll with the staves, and the measure buttons move up or down, as little as they need
    // to, to stay clear of them.
    for (size_t part = 0; part < tracks.size() && part < staffSystems.views.size(); ++part)
    {
        const auto& view = *staffSystems.views[part];
        const auto staves = view.getStavesRange();
        const auto bin = juce::Rectangle<int> (deleteButtonSize, deleteButtonSize)
                             .withCentre ({ columnX, view.getBounds().getY() + staves.getStart() + staves.getLength() / 2 });
        tracks[part].deleteButton->setBounds (bin);
        tracks[part].deleteButton->toFront (false);

        if (const auto clear = bin.expanded (0, 2 * gap); stack.intersects (clear))
        {
            const auto up = stack.getBottom() - clear.getY();
            const auto down = clear.getBottom() - stack.getY();
            stack.translate (0, up <= down ? -up : down);
        }
    }

    auto pair = stack.removeFromTop (measureButtonSize).withSizeKeepingCentre (measureButtonSize * 2 + gap, measureButtonSize);
    addMeasureButton.setBounds (pair.removeFromLeft (measureButtonSize));
    removeMeasureButton.setBounds (pair.removeFromRight (measureButtonSize));
    stack.removeFromTop (2 * gap);
    cloneButton.setBounds (stack.removeFromTop (cloneButtonHeight).withSizeKeepingCentre (cloneButtonWidth, cloneButtonHeight));
}

void MainComponent::saveVolumes()
{
    for (int part = 0; part < instrumentHost.getNumParts(); ++part)
    {
        settings.setValue (getVolumeKey (part), instrumentHost.getVolume (part));
        settings.setValue (getMutedKey (part), instrumentHost.isMuted (part));
    }
}

void MainComponent::editChord (int part, int measure)
{
    // One chord is edited at a time. The window stays where it was, if it's open already.
    std::optional<juce::Point<int>> position;

    if (chordWindow != nullptr && chordWindow->isVisible())
        position = chordWindow->getPosition();

    closeChordEditor();
    setActivePart (part);

    // A chord in several measures, as its rhythm takes, is edited where it starts.
    measure = score.getChordSpan (part, measure).first;

    auto editor = std::make_unique<ChordEditor> (score, part, measure, lastChordStyle, savedRhythms.getAll());
    auto* editorPointer = editor.get();

    editor->onChordChanged = [this, editorPointer]
    {
        lastChordStyle = editorPointer->getChord().style;
        showHeldNotes();
    };

    editor->onFinished = [this, editorPointer]
    {
        // The editor has finished, but it's still busy, so its window goes once it's done.
        if (chordWindow == nullptr || &chordWindow->getEditor() != editorPointer)
            return;

        chordWindow->setVisible (false);

        for (auto& view : staffSystems.views)
            view->setSelectedMeasure ({});

        showHeldNotes();

        juce::MessageManager::callAsync ([safeThis = juce::Component::SafePointer (this), editorPointer]
        {
            if (safeThis != nullptr && safeThis->chordWindow != nullptr && &safeThis->chordWindow->getEditor() == editorPointer)
                safeThis->chordWindow = nullptr;
        });
    };

    chordWindow = std::make_unique<ChordWindow> (std::move (editor), [this] { closeChordEditor(); });
    chordWindow->addKeyListener (commandManager.getKeyMappings());     // so Undo works from the chord window too

    // Over the sidebar, to start with, leaving the staff and the piano clear.
    const auto sidebarArea = localAreaToGlobal (sidebar.getBounds());
    chordWindow->setTopLeftPosition (position.value_or (sidebarArea.getTopLeft() + juce::Point<int> (8, 8)));
    chordWindow->setVisible (true);

    staffSystems.views[(size_t) part]->setSelectedMeasure (measure);
    scrollToMeasure (measure);
    showHeldNotes();
}

ChordEditor* MainComponent::getChordEditor() const
{
    return chordWindow != nullptr && chordWindow->isVisible() ? &chordWindow->getEditor() : nullptr;
}

void MainComponent::closeChordEditor()
{
    chordWindow = nullptr;

    for (auto& view : staffSystems.views)
        view->setSelectedMeasure ({});

    showHeldNotes();
}

void MainComponent::scrollToMeasure (int measure)
{
    // A measure out of view is brought to a quarter of the way across.
    const auto area = staffSystems.views[0]->getMeasureArea (measure);
    const auto viewArea = staffViewport.getViewArea();

    if (area.getX() < viewArea.getX() || area.getRight() > viewArea.getRight())
        staffViewport.setViewPosition (area.getX() - viewArea.getWidth() / 4, viewArea.getY());
}

void MainComponent::pianoKeyClicked (int midiNote)
{
    // While a chord's being edited, a key adds its note to the chord, or takes it out.
    if (auto* editor = getChordEditor())
        editor->toggleNote (midiNote);
}

void MainComponent::showHeldNotes()
{
    std::map<int, juce::String> heldNotes;

    if (const auto* editor = getChordEditor())
        if (const auto chord = score.getChordNotes (editor->getChord()))
            for (const auto& tone : chord->tones)
                heldNotes[tone.midi] = tone.getName();

    keyboard.setHeldNotes (heldNotes);
}

//==============================================================================
void MainComponent::timerCallback()
{
    recordPlayedKeys();
    showPlaybackPosition();
}

void MainComponent::togglePlayback()
{
    if (const auto position = instrumentHost.getPlaybackPosition())
    {
        if (instrumentHost.isRecording())
            finishRecording (position);

        // Stopping, it's back to the beginning.
        instrumentHost.stop();
        stopTimer();
        returnToBeginning();
        showPlaybackPosition();
    }
    else
    {
        startPlaying();
        showPlaybackPosition();
    }
}

void MainComponent::startPlaying()
{
    updatePerformance();
    const auto page = piece.getCurrentPage();
    lastPlayingPage = page;

    // From where it was paused, the same time through, if it's still played, or else from where the page plays from
    auto from = toPlayedBeats (page, playbackStarts[(size_t) page]).value_or (0.0);

    if (pausedAt.has_value() && pausedAt->page == page)
        if (const auto resumed = findPausedBeat (*pausedAt))
            from = *resumed;

    pausedAt.reset();
    instrumentHost.play (getPerformancePages(), loopButton.getToggleState(), from);

    // Followed as it plays, if it does, which it doesn't without the audio running
    if (instrumentHost.getPlaybackPosition().has_value())
        startTimerHz (30);

    startSoundFileIfWanted();
}

void MainComponent::pausePlayback()
{
    const auto played = instrumentHost.getPlaybackPosition();

    if (! played.has_value() || playedMeasures.empty())
        return;

    if (instrumentHost.isRecording())
        finishRecording (played);

    instrumentHost.stop();
    stopTimer();

    // Where it's got to: the measure as it's played, how many times it's been played before, and how far into it
    const auto after = std::upper_bound (playedMeasures.begin(), playedMeasures.end(), *played,
                                         [] (double beat, const PlayedMeasure& measure) { return beat < measure.start; });
    const auto index = (size_t) juce::jmax (0, (int) std::distance (playedMeasures.begin(), after) - 1);
    const auto measure = playedMeasures[index];
    const auto timesBefore = std::count_if (playedMeasures.begin(), playedMeasures.begin() + (std::ptrdiff_t) index,
                                            [&measure] (const PlayedMeasure& m) { return m.page == measure.page && m.measure == measure.measure; });
    const auto beatsIn = juce::jlimit (0.0, (double) measure.beatsPerMeasure, *played - measure.start);

    // The page it's paused on is shown, with its marker where it's paused, to play on from there.
    if (measure.page != piece.getCurrentPage())
        piece.choosePage (measure.page);

    playbackStarts[(size_t) measure.page] = measure.measure * measure.beatsPerMeasure + beatsIn;
    pausedAt = PausedPosition { measure.page, measure.measure, (int) timesBefore, beatsIn };
    showPlaybackPosition();
}

std::optional<double> MainComponent::findPausedBeat (const PausedPosition& paused) const
{
    auto times = 0;

    for (const auto& measure : playedMeasures)
    {
        if (measure.page != paused.page || measure.measure != paused.measure)
            continue;

        if (times++ == paused.timesBefore)
            return measure.start + juce::jmin (paused.beatsIn, (double) measure.beatsPerMeasure);
    }

    return {};
}

void MainComponent::updateTransportButtons()
{
    const auto playing = instrumentHost.getPlaybackPosition().has_value();
    playButton.setButtonText (playing ? "Stop" : "Play");
    playButton.setSymbol (playing ? controls::TransportButton::Symbol::stop : controls::TransportButton::Symbol::play);
    playButton.setTooltip (playing ? "Stop, and go back to the beginning" : "Play from the ruler's marker (space)");

    // Only what's playing can be paused, which the space bar does too.
    pauseButton.setEnabled (playing);
    pauseButton.setTooltip (playing ? "Pause: stop where it's got to, and carry on from there with Play (space)"
                                    : "Pause, while it plays: it stops where it's got to, for Play to carry on from there");
}

void MainComponent::setMakingMP3 (bool shouldMake)
{
    // MP3s are made with LAME, which has to be installed.
    if (shouldMake && ! getLame().existsAsFile())
    {
        mp3Button.setToggleState (false, juce::dontSendNotification);
        juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                          .withIconType (juce::MessageBoxIconType::InfoIcon)
                                          .withTitle ("MP3 needs LAME")
                                          .withMessage ("MP3 files are made with LAME, a free MP3 encoder, which isn't installed. "
                                                        "To install it with Homebrew, run this in Terminal, then turn MP3 on again:\n\n"
                                                        "brew install lame")
                                          .withButton ("OK")
                                          .withAssociatedComponent (this),
                                      [] (int) {});
        return;
    }

    mp3Button.setToggleState (shouldMake, juce::dontSendNotification);

    if (shouldMake)
        startSoundFileIfWanted();
    else
        finishSoundFile();
}

void MainComponent::startSoundFileIfWanted()
{
    if (! isMakingMP3() || ! instrumentHost.getPlaybackPosition().has_value() || instrumentHost.isWritingSoundFile())
        return;

    const auto wav = juce::File::getSpecialLocation (juce::File::tempDirectory).getNonexistentChildFile ("EZ Composer playback", ".wav");

    if (! instrumentHost.startSoundFile (wav))
        wav.deleteFile();
}

void MainComponent::finishSoundFile()
{
    if (! instrumentHost.isWritingSoundFile())
        return;

    instrumentHost.finishSoundFileWhenQuiet();
    waitForSoundFile (0);
}

void MainComponent::waitForSoundFile (int tries)
{
    // Once what's heard has died away, or if the audio's stopped, after long enough for it to have
    constexpr int tryEvery = 100;
    const auto giveUpAfter = (int) ((InstrumentHost::maxDyingAwaySeconds + 3.0) * 1000.0) / tryEvery;

    if (instrumentHost.isSoundFileFinished() || tries >= giveUpAfter)
    {
        if (const auto wav = instrumentHost.closeSoundFile(); wav.existsAsFile() && onSoundFileReady != nullptr)
            onSoundFileReady (wav);

        // Playing again while it died away, what's heard now goes in a file of its own.
        startSoundFileIfWanted();
        return;
    }

    juce::Timer::callAfterDelay (tryEvery, [safeThis = juce::Component::SafePointer (this), tries]
    {
        if (safeThis != nullptr)
            safeThis->waitForSoundFile (tries + 1);
    });
}

void MainComponent::chooseWhereToSaveMP3 (const juce::File& wav)
{
    soundFilesToSave.push_back (wav);

    if (mp3Chooser == nullptr)
        showNextMP3Dialog();
}

void MainComponent::showNextMP3Dialog()
{
    if (soundFilesToSave.empty())
        return;

    // Named after the score, beside it if it's been saved, or in Music
    const auto wav = soundFilesToSave.front();
    const auto scoreFile = document.getFile();
    const auto suggested = scoreFile.existsAsFile() ? scoreFile.withFileExtension (".mp3")
                                                    : juce::File::getSpecialLocation (juce::File::userMusicDirectory)
                                                          .getChildFile (document.getDocumentTitle()).withFileExtension (".mp3");

    mp3Chooser = std::make_unique<juce::FileChooser> ("Save the playback as an MP3", suggested, "*.mp3");
    mp3Chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                 | juce::FileBrowserComponent::warnAboutOverwriting,
                             [this, wav] (const juce::FileChooser& chooser)
                             {
                                 const auto chosen = chooser.getResult();

                                 if (chosen == juce::File())
                                     wav.deleteFile();
                                 else
                                     saveMP3 (wav, chosen.hasFileExtension ("mp3") ? chosen : chosen.withFileExtension (".mp3"));

                                 // The next, if there is one, once this dialog's gone
                                 juce::MessageManager::callAsync ([safeThis = juce::Component::SafePointer (this)]
                                 {
                                     if (safeThis == nullptr)
                                         return;

                                     safeThis->mp3Chooser.reset();

                                     if (! safeThis->soundFilesToSave.empty())
                                         safeThis->soundFilesToSave.erase (safeThis->soundFilesToSave.begin());

                                     safeThis->showNextMP3Dialog();
                                 });
                             });
}

void MainComponent::saveMP3 (const juce::File& wav, const juce::File& mp3, std::function<void (bool)> onSaved)
{
    const auto lame = getLame();
    const auto title = document.getDocumentTitle();

    mp3Encoder.addJob ([safeThis = juce::Component::SafePointer (this), lame, wav, mp3, title, onSaved]
    {
        // Variable bit rate, quality 2, about 190 kbps, near enough to the sound itself
        juce::ChildProcess lameProcess;
        juce::String output;
        auto saved = false;

        if (lame.existsAsFile()
            && lameProcess.start (juce::StringArray { lame.getFullPathName(), "--quiet", "-V", "2", "--tt", title,
                                                      wav.getFullPathName(), mp3.getFullPathName() }))
        {
            output = lameProcess.readAllProcessOutput();
            lameProcess.waitForProcessToFinish (-1);
            saved = lameProcess.getExitCode() == 0 && mp3.getSize() > 0;
        }

        wav.deleteFile();

        juce::MessageManager::callAsync ([safeThis, saved, mp3, output, onSaved]
        {
            if (onSaved != nullptr)
                onSaved (saved);

            if (! saved && safeThis != nullptr)
                juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                                  .withIconType (juce::MessageBoxIconType::WarningIcon)
                                                  .withTitle ("Couldn't save the MP3")
                                                  .withMessage ("LAME couldn't make " + mp3.getFileName() + "."
                                                                + (output.trim().isNotEmpty() ? "\n\n" + output.trim() : juce::String()))
                                                  .withButton ("OK")
                                                  .withAssociatedComponent (safeThis.getComponent()),
                                              [] (int) {});
        });
    });
}

juce::File MainComponent::findLame()
{
    // Where Homebrew puts it, on Apple silicon or Intel, or anywhere on the path, which an app
    // opened from the Finder may not have Homebrew's folders in
    juce::StringArray folders { "/opt/homebrew/bin", "/usr/local/bin" };
    folders.addTokens (juce::SystemStats::getEnvironmentVariable ("PATH", {}), ":", {});

    for (const auto& folder : folders)
        if (const auto lame = juce::File (folder).getChildFile ("lame"); folder.isNotEmpty() && lame.existsAsFile())
            return lame;

    return {};
}

juce::File MainComponent::getLame()
{
    // Found when it's first needed, which may be after it's been installed
    if (! lameExecutable.existsAsFile())
        lameExecutable = findLame();

    return lameExecutable;
}

void MainComponent::returnToBeginning()
{
    // What was heard, if it's being written, is finished, to be saved.
    finishSoundFile();

    // Every page plays from its start, and with Piece, it's back to the first page.
    pausedAt.reset();
    std::fill (playbackStarts.begin(), playbackStarts.end(), 0.0);

    if (isPlayingWholePiece() && piece.getCurrentPage() != 0)
        piece.choosePage (0);

    scrollToMeasure (0);
}

void MainComponent::setPlayingWholePiece (bool whole)
{
    playWholePiece = whole;
    playModeButton.setButtonText (whole ? "Piece" : "Page");
    playModeButton.setTooltip (whole ? "Piece: Play plays every page, one after another, from where this page plays from. Click to play just this page."
                                     : "Page: Play plays just the page you're on. Click to play every page, one after another.");
    settings.setValue (playWholePieceKey, whole);

    // Playing, it carries on from where it is, if it can.
    if (instrumentHost.getPlaybackPosition().has_value())
    {
        const auto before = playedMeasures;
        updatePerformance();
        followPerformanceChange (before);
        showPlaybackPosition();
    }
}

void MainComponent::updatePerformance()
{
    performances.clear();
    playedMeasures.clear();

    // Every page, one after another, or just the one being edited
    const auto first = isPlayingWholePiece() ? 0 : piece.getCurrentPage();
    const auto last = isPlayingWholePiece() ? piece.getNumPages() - 1 : first;
    auto start = 0.0;

    for (int page = first; page <= last; ++page)
    {
        // At the tempo, which is every page's, so the music and where it's shown keep together
        const auto& written = piece.getPage (page);
        auto performance = std::make_unique<Score>();
        written.writePerformance (*performance);
        performance->setBeatsPerMinute (score.getBeatsPerMinute());
        performances.push_back (std::move (performance));

        const auto pageStart = start;

        for (auto measure : written.getPerformanceOrder())
        {
            playedMeasures.push_back ({ page, measure, written.getBeatsPerMeasure(), start, pageStart });
            start += written.getBeatsPerMeasure();
        }
    }
}

std::vector<const Score*> MainComponent::getPerformancePages() const
{
    std::vector<const Score*> pages;

    for (const auto& performance : performances)
        pages.push_back (performance.get());

    return pages;
}

std::optional<MainComponent::WrittenPosition> MainComponent::toWrittenPosition (double playedBeats) const
{
    if (playedMeasures.empty())
        return {};

    // The last measure starting at or before it
    const auto after = std::upper_bound (playedMeasures.begin(), playedMeasures.end(), playedBeats,
                                         [] (double beat, const PlayedMeasure& measure) { return beat < measure.start; });
    const auto& measure = playedMeasures[(size_t) juce::jmax (0, (int) std::distance (playedMeasures.begin(), after) - 1)];
    return WrittenPosition { measure.page, measure.measure * measure.beatsPerMeasure + (playedBeats - measure.start), playedBeats - measure.pageStart };
}

std::optional<double> MainComponent::toPlayedBeats (int page, double writtenBeats) const
{
    for (const auto& measure : playedMeasures)
        if (measure.page == page && measure.measure == (int) std::floor (writtenBeats / measure.beatsPerMeasure))
            return measure.start + (writtenBeats - measure.measure * measure.beatsPerMeasure);

    return {};
}

void MainComponent::followPerformanceChange (const std::vector<PlayedMeasure>& before)
{
    const auto played = instrumentHost.getPlaybackPosition();

    if (! played.has_value())
        return;

    // Where it was: the page and measure, how many times it had been there before, and how far into the measure
    std::optional<double> beat;

    if (! before.empty())
    {
        const auto after = std::upper_bound (before.begin(), before.end(), *played,
                                             [] (double b, const PlayedMeasure& measure) { return b < measure.start; });
        const auto index = juce::jmax (0, (int) std::distance (before.begin(), after) - 1);
        const auto& was = before[(size_t) index];
        const auto sameMeasure = [&was] (const PlayedMeasure& m) { return m.page == was.page && m.measure == was.measure; };
        const auto timesBefore = (size_t) std::count_if (before.begin(), before.begin() + index, sameMeasure);
        std::vector<size_t> times;

        for (size_t i = 0; i < playedMeasures.size(); ++i)
            if (sameMeasure (playedMeasures[i]))
                times.push_back (i);

        if (! times.empty())
        {
            const auto& now = playedMeasures[times[juce::jmin (timesBefore, times.size() - 1)]];
            beat = now.start + juce::jmin (*played - was.start, (double) now.beatsPerMeasure);
        }
    }

    if (beat.has_value())
    {
        instrumentHost.updatePlaying (getPerformancePages(), *beat);
        return;
    }

    // The page it was playing isn't played now, so it plays the page being edited, from where that plays from.
    if (instrumentHost.isRecording())
    {
        recordPlayedKeys();
        recorder.finish ({});
    }

    startPlaying();
}

void MainComponent::choosePlaybackStart (int beat)
{
    const auto page = piece.getCurrentPage();
    playbackStarts[(size_t) page] = beat;
    pausedAt.reset();

    if (instrumentHost.getPlaybackPosition().has_value())
    {
        if (const auto played = toPlayedBeats (page, beat))
        {
            jumpPlayback (*played);
            lastPlayingPage = page;
        }
    }

    showPlaybackPosition();
}

void MainComponent::jumpPlayback (double playedBeats)
{
    const auto position = instrumentHost.getPlaybackPosition();

    if (! position.has_value())
        return;

    // Keys held while recording end where it jumps from.
    if (instrumentHost.isRecording())
    {
        recordPlayedKeys();
        const auto at = toWrittenPosition (*position);
        recorder.finish (at.has_value() ? std::optional (std::pair { at->page, at->playedBeat }) : std::nullopt);
    }

    instrumentHost.jumpTo (playedBeats);
}

void MainComponent::goToPageStart()
{
    const auto page = piece.getCurrentPage();

    // Playing, it carries on from the start of the page, and stopped, the page plays from there.
    if (instrumentHost.getPlaybackPosition().has_value())
    {
        jumpPlayback (toPlayedBeats (page, 0.0).value_or (0.0));
        lastPlayingPage = page;
    }
    else
    {
        playbackStarts[(size_t) page] = 0.0;
        pausedAt.reset();
        scrollToMeasure (0);
    }

    showPlaybackPosition();
}

void MainComponent::stepMeasure (int direction)
{
    // Playing, it jumps to the start of the measure before or after the one playing, as they're played.
    if (const auto played = instrumentHost.getPlaybackPosition())
    {
        if (playedMeasures.empty())
            return;

        const auto after = std::upper_bound (playedMeasures.begin(), playedMeasures.end(), *played,
                                             [] (double beat, const PlayedMeasure& measure) { return beat < measure.start; });
        const auto count = (int) playedMeasures.size();
        auto index = juce::jmax (0, (int) std::distance (playedMeasures.begin(), after) - 1) + direction;

        // Looping, it goes round from one end to the other; otherwise there's nothing after the last.
        if (loopButton.getToggleState())
            index = music::mod (index, count);
        else if (index >= count)
            return;

        jumpPlayback (playedMeasures[(size_t) juce::jmax (0, index)].start);
        showPlaybackPosition();
        return;
    }

    // Stopped, the page plays from the start of the measure before or after the one it plays
    // from, or past either end of it, with Piece, from the next page's start, or the last measure
    // of the one before.
    auto page = piece.getCurrentPage();
    const auto& written = piece.getPage (page);
    const auto measure = juce::jlimit (0, written.getNumMeasures() - 1,
                                       (int) std::floor (playbackStarts[(size_t) page] / written.getBeatsPerMeasure() + 1.0e-9)) + direction;
    auto start = (double) (measure * written.getBeatsPerMeasure());

    if (measure >= written.getNumMeasures())
    {
        if (! isPlayingWholePiece() || page + 1 >= piece.getNumPages())
            return;

        ++page;
        start = 0.0;
    }
    else if (measure < 0)
    {
        start = 0.0;

        if (isPlayingWholePiece() && page > 0)
        {
            const auto& previous = piece.getPage (--page);
            start = (double) ((previous.getNumMeasures() - 1) * previous.getBeatsPerMeasure());
        }
    }

    playbackStarts[(size_t) page] = start;
    pausedAt.reset();

    if (page != piece.getCurrentPage())
        piece.choosePage (page);

    scrollToMeasure ((int) (start / piece.getPage (page).getBeatsPerMeasure()));
    showPlaybackPosition();
}

void MainComponent::toggleRecording()
{
    // Recording again stops recording, and the score plays on.
    if (instrumentHost.isRecording())
    {
        finishRecording (instrumentHost.getPlaybackPosition());
        return;
    }

    if (! instrumentHost.getPlaybackPosition().has_value())
        startPlaying();

    // Without the audio running, nothing plays, so there's nothing to record along with.
    if (instrumentHost.getPlaybackPosition().has_value())
    {
        // Everything recorded is undone in one go.
        history.beginGesture();
        instrumentHost.takeRecordedKeys();
        instrumentHost.setRecording (true);
        recordButton.setToggleState (true, juce::dontSendNotification);
    }

    showPlaybackPosition();
}

void MainComponent::recordPlayedKeys()
{
    // Each on the page it was played on, where that page had got to as it's played
    for (const auto& key : instrumentHost.takeRecordedKeys())
    {
        if (const auto at = toWrittenPosition (key.beat))
        {
            if (key.isDown)
                recorder.keyDown (key.noteNumber, at->page, at->playedBeat, activePartId);
            else
                recorder.keyUp (key.noteNumber, at->page, at->playedBeat);
        }
    }
}

void MainComponent::finishRecording (std::optional<double> playedBeats)
{
    instrumentHost.setRecording (false);
    recordPlayedKeys();
    const auto at = playedBeats.has_value() ? toWrittenPosition (*playedBeats) : std::nullopt;
    recorder.finish (at.has_value() ? std::optional (std::pair { at->page, at->playedBeat }) : std::nullopt);
    recordButton.setToggleState (false, juce::dontSendNotification);
    history.endGesture();
}

void MainComponent::showPlaybackPosition()
{
    const auto played = instrumentHost.getPlaybackPosition();

    // Having just played to the end, by itself, it's back to the beginning, as stopping goes.
    if (! played.has_value() && isTimerRunning())
    {
        if (instrumentHost.isRecording())
            finishRecording ({});

        stopTimer();
        returnToBeginning();
    }

    const auto at = played.has_value() ? toWrittenPosition (*played) : std::nullopt;

    // When the music goes on to another page, the page being edited goes with it, if it was the
    // one playing, so it's followed along.
    if (at.has_value() && at->page != lastPlayingPage)
    {
        const auto follow = lastPlayingPage == piece.getCurrentPage() && at->page != piece.getCurrentPage();
        lastPlayingPage = at->page;

        if (follow)
        {
            piece.choosePage (at->page);
            return;
        }
    }

    updateTransportButtons();

    // Shown where it is in the page as it's written, if it's playing the page being edited
    const auto position = at.has_value() && at->page == piece.getCurrentPage() ? std::optional (at->beat) : std::nullopt;

    for (auto& view : staffSystems.views)
        view->setPlaybackPosition (position);

    // The ruler's marker is where it's playing, or where it'll start from.
    positionRuler.setMarker (played.has_value() ? position : std::optional (playbackStarts[(size_t) piece.getCurrentPage()]), played.has_value());

    if (! played.has_value())
    {
        // Recording stops when the score stops, at the end, the notes still held lasting to there.
        if (instrumentHost.isRecording())
            finishRecording ({});

        stopTimer();
        return;
    }

    if (! position.has_value())
        return;

    // When the beat that's playing goes out of view, scroll it back to a quarter of the way across.
    const auto playingArea = staffSystems.views[0]->getPlaybackArea();
    const auto viewArea = staffViewport.getViewArea();

    if (playingArea.getX() < viewArea.getX() || playingArea.getRight() > viewArea.getRight())
        staffViewport.setViewPosition (playingArea.getX() - viewArea.getWidth() / 4, viewArea.getY());
}

void MainComponent::tempoEdited (bool finished)
{
    const auto text = tempoEditor.getText().trim();
    const auto beatsPerMinute = text.getDoubleValue();
    const auto valid = text.containsOnly ("0123456789.") && text.containsAnyOf ("0123456789")
                    && text.indexOfChar ('.') == text.lastIndexOfChar ('.')
                    && beatsPerMinute >= Score::minBeatsPerMinute && beatsPerMinute <= Score::maxBeatsPerMinute;

    if (valid)
        score.setBeatsPerMinute (beatsPerMinute);
    else if (finished)
        tempoEditor.setText (formatNumber (score.getBeatsPerMinute()), false);

    // A value that won't do is outlined in red until it's fixed, or put back when editing stops.
    for (auto colourId : { juce::TextEditor::outlineColourId, juce::TextEditor::focusedOutlineColourId })
    {
        if (! valid && ! finished)
            tempoEditor.setColour (colourId, juce::Colours::red);
        else
            tempoEditor.removeColour (colourId);
    }

    tempoEditor.repaint();
}

void MainComponent::stepTempo (int direction)
{
    // From a fraction, to the whole number either side of it
    const auto beatsPerMinute = score.getBeatsPerMinute();
    const auto stepped = direction > 0 ? std::floor (beatsPerMinute) + 1.0 : std::ceil (beatsPerMinute) - 1.0;
    score.setBeatsPerMinute (juce::jlimit (Score::minBeatsPerMinute, Score::maxBeatsPerMinute, stepped));

    // Shown straight away, even while it's being typed, which it replaces
    tempoEditor.setText (formatNumber (score.getBeatsPerMinute()), false);

    for (auto colourId : { juce::TextEditor::outlineColourId, juce::TextEditor::focusedOutlineColourId })
        tempoEditor.removeColour (colourId);

    tempoEditor.repaint();
    updateTempoArrows();
}

void MainComponent::updateTempoArrows()
{
    const auto canGoFaster = score.getBeatsPerMinute() < Score::maxBeatsPerMinute;
    const auto canGoSlower = score.getBeatsPerMinute() > Score::minBeatsPerMinute;
    tempoUpButton.setEnabled (canGoFaster);
    tempoDownButton.setEnabled (canGoSlower);
    tempoUpButton.setTooltip (canGoFaster ? "Faster, a beat per minute at a time. Hold it down to keep going."
                                          : "The tempo's as fast as it goes, " + formatNumber (Score::maxBeatsPerMinute) + " BPM");
    tempoDownButton.setTooltip (canGoSlower ? "Slower, a beat per minute at a time. Hold it down to keep going."
                                            : "The tempo's as slow as it goes, " + formatNumber (Score::minBeatsPerMinute) + " BPM");
}

void MainComponent::addMeasure()
{
    score.addMeasure();

    // Scroll to the end, so the new measure is in view.
    staffViewport.setViewPosition (staffSystems.getWidth(), staffViewport.getViewPositionY());
}

void MainComponent::cloneMeasures()
{
    score.cloneMeasures();

    // Scroll to the end, so the new measures are in view.
    staffViewport.setViewPosition (staffSystems.getWidth(), staffViewport.getViewPositionY());
}

void MainComponent::removeMeasure()
{
    if (removeMeasureButton.isEnabled())
        score.removeLastMeasure();
}

void MainComponent::updateRemoveMeasureButton()
{
    // Only an empty last measure can be taken away, and not the only one.
    const auto last = score.getNumMeasures() - 1;
    const auto canRemove = last > 0 && score.isMeasureEmpty (last);
    removeMeasureButton.setEnabled (canRemove);
    removeMeasureButton.setTooltip (canRemove ? "Remove the last measure"
                                              : last == 0 ? "The only measure can't be removed"
                                                          : "Only an empty last measure, with nothing but rests in every instrument, can be removed");
}

//==============================================================================
juce::StringArray MainComponent::getMenuBarNames()
{
    return { "File", "Edit", "MIDI" };
}

juce::PopupMenu MainComponent::getMenuForIndex (int menuIndex, const juce::String&)
{
    juce::PopupMenu menu;

    if (menuIndex == 1)
    {
        menu.addCommandItem (&commandManager, undoChange);
        menu.addCommandItem (&commandManager, redoChange);
        return menu;
    }

    if (menuIndex == 2)
    {
        // Each MIDI input, ticked when it's on.
        midiMenuDevices = midiInputs.getDevices();

        if (midiMenuDevices.empty())
            menu.addItem (firstMidiInputItem - 1, "No MIDI Inputs", false);

        for (size_t i = 0; i < midiMenuDevices.size(); ++i)
            menu.addItem (firstMidiInputItem + (int) i, midiMenuDevices[i].info.name, true, midiMenuDevices[i].enabled);

        return menu;
    }

    menu.addCommandItem (&commandManager, newScore);
    menu.addCommandItem (&commandManager, openScore);
    menu.addSubMenu ("Open Recent", getRecentScoresMenu());
    menu.addSeparator();
    menu.addCommandItem (&commandManager, saveScore);
    menu.addCommandItem (&commandManager, saveScoreAs);
    return menu;
}

juce::PopupMenu MainComponent::getRecentScoresMenu() const
{
    // The scores that are still there, newest first, by name. Two with the same name say which folder they're in.
    const auto& recent = document.getRecentScores();
    juce::StringArray names;

    for (int i = 0; i < recent.getNumFiles(); ++i)
        names.add (recent.getFile (i).getFileNameWithoutExtension());

    juce::PopupMenu menu;

    for (int i = 0; i < recent.getNumFiles(); ++i)
    {
        const auto file = recent.getFile (i);

        if (! file.existsAsFile())
            continue;

        auto name = names[i];

        if (std::count (names.begin(), names.end(), name) > 1)
            name << juce::String (juce::CharPointer_UTF8 (" \xe2\x80\x94 ")) << file.getParentDirectory().getFileName();

        menu.addItem (firstRecentScoreItem + i, name);
    }

    if (menu.getNumItems() == 0)
        menu.addItem (noRecentScoresItem, "No Recent Scores", false);

    menu.addSeparator();
    menu.addItem (clearRecentScoresItem, "Clear Menu", recent.getNumFiles() > 0);
    return menu;
}

void MainComponent::openRecentScore (const juce::File& file)
{
    saveChangesThen ([this, file]
    {
        // One that's been moved, deleted or damaged is taken off the list.
        if (document.loadFrom (file, true).wasOk())
            scoreReplaced();
        else
            document.forgetRecentScore (file);
    });
}

void MainComponent::menuItemSelected (int menuItemID, int topLevelMenuIndex)
{
    if (topLevelMenuIndex == 0)
    {
        const auto& recent = document.getRecentScores();

        if (menuItemID == clearRecentScoresItem)
            document.clearRecentScores();
        else if (juce::isPositiveAndBelow (menuItemID - firstRecentScoreItem, recent.getNumFiles()))
            openRecentScore (recent.getFile (menuItemID - firstRecentScoreItem));

        return;
    }

    const auto index = (size_t) (menuItemID - firstMidiInputItem);

    if (topLevelMenuIndex == 2 && menuItemID >= firstMidiInputItem && index < midiMenuDevices.size())
        midiInputs.setEnabled (midiMenuDevices[index].info.identifier, ! midiMenuDevices[index].enabled);
}

juce::ApplicationCommandTarget* MainComponent::getNextCommandTarget()
{
    return nullptr;
}

void MainComponent::getAllCommands (juce::Array<juce::CommandID>& commands)
{
    commands.addArray ({ newScore, openScore, saveScore, saveScoreAs, undoChange, redoChange });
}

void MainComponent::getCommandInfo (juce::CommandID command, juce::ApplicationCommandInfo& info)
{
    const auto cmd = juce::ModifierKeys::commandModifier;

    switch (command)
    {
        case newScore:     info.setInfo ("New", "Starts a new, empty score", "File", 0);
                           info.addDefaultKeypress ('n', cmd); break;
        case openScore:    info.setInfo (juce::String (juce::CharPointer_UTF8 ("Open\xe2\x80\xa6")), "Opens a saved score", "File", 0);
                           info.addDefaultKeypress ('o', cmd); break;
        case saveScore:    info.setInfo ("Save", "Saves the score", "File", 0);
                           info.addDefaultKeypress ('s', cmd); break;
        case saveScoreAs:  info.setInfo (juce::String (juce::CharPointer_UTF8 ("Save As\xe2\x80\xa6")), "Saves the score in a new file", "File", 0);
                           info.addDefaultKeypress ('s', cmd | juce::ModifierKeys::shiftModifier); break;
        case undoChange:   info.setInfo ("Undo", "Undoes the last change to the score", "Edit", 0);
                           info.setActive (history.canUndo());
                           info.addDefaultKeypress ('z', cmd); break;
        case redoChange:   info.setInfo ("Redo", "Makes the last change undone again", "Edit", 0);
                           info.setActive (history.canRedo());
                           info.addDefaultKeypress ('z', cmd | juce::ModifierKeys::shiftModifier); break;
        default:           break;
    }
}

bool MainComponent::perform (const InvocationInfo& invocation)
{
    switch (invocation.commandID)
    {
        case newScore:
            saveChangesThen ([this] { document.startNewScore(); scoreReplaced(); });
            return true;

        case openScore:
            saveChangesThen ([this]
            {
                document.loadFromUserSpecifiedFileAsync (true, [safeThis = juce::Component::SafePointer (this)] (juce::Result result)
                {
                    if (safeThis != nullptr && result.wasOk())
                        safeThis->scoreReplaced();
                });
            });
            return true;

        case saveScore:
            document.saveAsync (true, true, nullptr);
            return true;

        case saveScoreAs:
            document.saveAsInteractiveAsync (true, nullptr);
            return true;

        // The measures selected may not be the same ones afterwards.
        case undoChange:
            selectMeasures ({}, true);
            history.undo();
            return true;

        case redoChange:
            selectMeasures ({}, true);
            history.redo();
            return true;

        default:
            return false;
    }
}

void MainComponent::saveChangesThen (std::function<void()> action)
{
    // An instrument's sound may have been changed in its editor, which might still be open.
    for (auto& track : tracks)
        track.panel->checkForSoundChanges();

    // So may an effect's, in its editor.
    if (effectRack.checkForSoundChanges())
        document.changed();

    document.saveIfNeededAndUserAgreesAsync ([safeThis = juce::Component::SafePointer (this), action] (juce::FileBasedDocument::SaveResult result)
    {
        if (safeThis != nullptr && result == juce::FileBasedDocument::savedOk)
            action();
    });
}

void MainComponent::scoreReplaced()
{
    // A new score starts from the beginning, with nothing playing, recording or selected, and
    // nothing to undo. Keys held while recording the old one aren't written into it.
    if (instrumentHost.isRecording())
    {
        instrumentHost.setRecording (false);
        recorder.clear();
        recordButton.setToggleState (false, juce::dontSendNotification);
        history.endGesture();
    }

    history.clear();
    instrumentHost.stop();
    playbackStarts.assign ((size_t) piece.getNumPages(), 0.0);
    pausedAt.reset();
    lastPlayingPage = -1;
    showPlaybackPosition();
    closeChordEditor();
    staffViewport.setViewPosition (0, 0);
}

void MainComponent::showDocumentTitle()
{
    if (auto* window = findParentComponentOfClass<juce::DocumentWindow>())
        window->setName (document.getDocumentTitle() + (document.hasChangedSinceSaved() ? juce::String (juce::CharPointer_UTF8 (" \xe2\x80\x94 Edited")) : juce::String()));
}
