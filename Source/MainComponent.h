#pragma once

#include "ChordEditor.h"
#include "Controls.h"
#include "InstrumentHost.h"
#include "InstrumentPanel.h"
#include "MidiInputs.h"
#include "DynamicPicker.h"
#include "EffectRack.h"
#include "EffectsWindow.h"
#include "EraserButton.h"
#include "NoteLengthPicker.h"
#include "PageTabs.h"
#include "PianoKeyboard.h"
#include "Piece.h"
#include "PositionRuler.h"
#include "ProgressionPanels.h"
#include "Progressions.h"
#include "RhythmPanels.h"
#include "Rhythms.h"
#include "Recorder.h"
#include "Score.h"
#include "ScoreDocument.h"
#include "ScoreHistory.h"
#include "ScorePanel.h"
#include "StaffView.h"

#include <juce_audio_utils/juce_audio_utils.h>

/** The main window's contents: a toolbar, a sidebar with the score's and chords' settings, the
    scrolling grand staff, and a piano keyboard, all playing through an instrument plugin. It
    also puts the File, Edit and MIDI menus in the menu bar, for saving and opening scores,
    undoing and redoing changes to them, and choosing MIDI controllers.
*/
class MainComponent final : public juce::Component,
                            public juce::ApplicationCommandTarget,
                            private juce::MenuBarModel,
                            private juce::Timer,
                            private juce::ChangeListener,
                            private juce::KeyListener
{
public:
    /** The settings are where the app keeps what it needs next time, such as the instrument. */
    explicit MainComponent (juce::PropertiesFile& settings);
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void parentHierarchyChanged() override;

    /** Offers to save unsaved changes, then does the action, unless the user cancels. */
    void saveChangesThen (std::function<void()> action);

    ApplicationCommandTarget* getNextCommandTarget() override;
    void getAllCommands (juce::Array<juce::CommandID>&) override;
    void getCommandInfo (juce::CommandID, juce::ApplicationCommandInfo&) override;
    bool perform (const InvocationInfo&) override;

    static constexpr int minimumWidth = 1388;     // room for the note length, staccato, dynamic, pedal and repeat buttons, and Edit, Select and the eraser
    static constexpr int minimumHeight = 640;

private:
    /** The parts' staff systems, one above the other, their measures lined up, with a + below the
        last part for adding another.
    */
    struct StaffSystems final : public juce::Component
    {
        explicit StaffSystems (Score&);
        ~StaffSystems() override;

        /** At least this big, to fill the view it's in, so a click anywhere in it is beside a part's staves. */
        void setMinimumSize (int width, int height);

        /** The part whose staves are level with a point this far down, or the nearest, above
            the first or below the last.
        */
        std::optional<int> getPartAt (int y) const;

        /** Called when it's clicked beside a part's staves, past the end of the score. */
        std::function<void (int part)> onPartClicked;

        /** Each part's view, from the top, in the order of the score's parts. */
        std::vector<std::unique_ptr<StaffView>> views;

        /** Adds a view for a part at a place among the others, or takes one out. */
        StaffView& insertView (int index, int part);
        void removeView (int index);

        /** Called when the systems move or change size. */
        std::function<void()> onLayoutChanged;

        /** Called when the + below the last part is clicked. */
        std::function<void()> onAddPart;

        void layOut();

    private:
        void childBoundsChanged (juce::Component*) override;
        void mouseDown (const juce::MouseEvent&) override;

        Score& score;
        int minimumWidth = 0, minimumHeight = 0;
        bool layingOut = false;

        controls::RoundButtonLookAndFeel roundButtonLookAndFeel;
        juce::TextButton addPartButton { "+" };
    };

    /** What each part has besides its staves: its instrument, the volume dial to the left of its
        staves, and a button to the right of them, past the end of the score, for taking the part out.
    */
    struct Track
    {
        int partId;
        std::unique_ptr<InstrumentPanel> panel;
        std::unique_ptr<controls::ClickableDial> dial;
        std::unique_ptr<controls::BinButton> deleteButton;
    };

    /** The column to the left of the staves, holding each part's volume dial and delete button,
        which says when it's clicked beside them.
    */
    struct VolumeColumn final : public juce::Component
    {
        std::function<void (const juce::MouseEvent&)> onClicked;

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (onClicked != nullptr)
                onClicked (e);
        }
    };

    /** A viewport that says when it scrolls. */
    struct ScrollingViewport final : public juce::Viewport
    {
        std::function<void()> onScroll;

        void visibleAreaChanged (const juce::Rectangle<int>&) override
        {
            if (onScroll != nullptr)
                onScroll();
        }
    };

    enum Commands
    {
        newScore = 1,
        openScore,
        saveScore,
        saveScoreAs,
        undoChange,
        redoChange
    };

    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex (int menuIndex, const juce::String& menuName) override;
    void menuItemSelected (int menuItemID, int topLevelMenuIndex) override;

    /** The File menu's Open Recent menu, listing the recent scores. */
    juce::PopupMenu getRecentScoresMenu() const;

    /** Opens a recent score, after asking to save any changes to this one. */
    void openRecentScore (const juce::File&);

    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    using juce::Component::keyPressed;
    bool keyPressed (const juce::KeyPress&, juce::Component*) override;

    /** Clicking something that doesn't take the keyboard focus, such as Play or the staves, leaves
        the focus where it was, rather than giving it to the first control that would take it.
    */
    std::unique_ptr<juce::ComponentTraverser> createKeyboardFocusTraverser() override;


    /** Makes a part the active one: the keyboard plays its instrument, and the toolbar and the
        sidebar's alternate staff show and set its.
    */
    void setActivePart (int part);

    /** Gives each of the score's parts a track, and takes away the tracks of parts that have
        gone, e.g. when a part's added or taken out, a score's opened, or a change is undone.
    */
    void updateTracks();
    void insertTrack (int index, bool reloadFromSettings);
    void removeTrack (int index);
    void updatePartBox();

    /** Gives each track's controls its part's number, e.g. in their tooltips. */
    void numberTracks();

    /** What a part's called: its number and its instrument, e.g. "Instrument 1 · Pianoteq". */
    juce::String getPartTitle (int part) const;

    /** Opens the effects page, or brings it to the front. */
    void showEffects();

    /** Keeps the effects, and where everything goes, in the settings, for next time. */
    void saveEffects();

    /** Says which effects couldn't be loaded, if any. */
    static void reportFailedEffects (const juce::StringArray& failed, const juce::String& from);

    /** Adds a part after the last, makes it the active one, and scrolls down to it. */
    void addPart();

    /** Takes a part out, after asking, unless there's nothing in it to lose. */
    void deletePart (int part);

    /** Chooses the length of the notes that clicking the staff adds, in beats: 4, 3, 2, 1.5, 1, 0.75, 0.5, 0.25 or 0.125.
        Clicks add notes again, if they were marking a dynamic.
    */
    void setNoteLength (double beats);

    /** Makes the notes that clicking the staff adds staccato, or not, whatever their length.
        Clicks add notes again, if they were marking something or erasing.
    */
    void setStaccato (bool);

    /** Chooses a dynamic or hairpin for clicking or dragging on the score to mark, instead of
        adding notes, or none to go back to adding notes of the length chosen.
    */
    void setMarking (std::optional<music::Marking>);

    /** Turns the eraser on, for taking out whatever the mouse passes over with its button held
        down, or off, back to adding notes of the length chosen.
    */
    void setErasing (bool);

    /** Turns editing notes on, for moving notes by dragging them and changing their lengths or
        taking them out by clicking them, or off. Clicks anywhere else go on adding notes or
        marking as before. It turns the eraser off, and the eraser turns it off.
    */
    void setEditing (bool);

    //==============================================================================
    /** Turns selecting measures on, for dragging across the staves to select whole measures, and
        then deleting or moving them, or off, back to adding notes. It turns editing notes and
        the eraser off, and they turn it off, as choosing a note length, dynamic or the like does.
        Turned off, nothing's selected.
    */
    void setSelecting (bool);
    bool isSelecting() const noexcept { return selectButton.getToggleState(); }

    /** Turns selecting off, leaving whatever else is chosen as it is. */
    void stopSelecting();

    /** Selects measures on the page being edited, from one to another, or none, in every part.
        Once they're selected, with the mouse let go, Delete and Move show over them. A new
        selection being dragged out stops moving the last.
    */
    void selectMeasures (std::optional<std::pair<int, int>>, bool finished);

    /** Takes out the selected measures, in every part, with everything in them; the measures after them move up. */
    void deleteSelectedMeasures();

    /** Turns moving the selected measures on or off, as Move does: while it's on, they can be dragged to another place. */
    void setMovingMeasures (bool);

    /** Moves the selected measures to before a barline, as the measures are numbered before they
        move, keeping them selected where they go, to move again.
    */
    void moveSelectedMeasures (int barline);

    /** Puts an empty measure in before the selected measures, pushing them on, or takes out the
        one before them, as they're dragged on past the end, and back. They stay selected.
    */
    void pushSelectedMeasures (int change);

    /** Shows where the selected measures would go, as they're dragged, in every part. */
    void showMoveTarget (std::optional<int> barline);

    /** Puts Delete and Move over the selected measures, halfway down what's in view, once
        they're selected, and says what they can do, or hides them.
    */
    void positionSelectionButtons();

    /** Shows the Scale button's settings, in a box beside it. */
    void showScalePanel (juce::Component& button);

    /** Makes the next click on a staff add notes from the key's scale, as the settings say. */
    void startPlacingScale (const ScaleSettings&);

    /** Goes back to clicks doing what they did before, without adding notes from the scale. */
    void stopPlacingScale();
    bool isPlacingScale() const noexcept { return placingScale; }

    /** Shows how many measures to save the active part's rhythm of, from the measure it plays
        from, and a name for it, in a box beside Save Rhythm.
    */
    void showSaveRhythmPanel (juce::Component& button);

    /** Keeps the rhythm of the active part's measures, from one, under a name, for every score,
        asking first if it would take the place of one saved with that name.
    */
    void saveRhythm (const juce::String& name, int firstMeasure, int numMeasures);

    /** Shows the saved rhythms, and how many times to write one, in a box beside Apply Saved Rhythm. */
    void showApplyRhythmPanel (juce::Component& button);

    /** Makes the next click on a staff write a rhythm there, so many times over. */
    void startPlacingRhythm (const SavedRhythm&, int times);
    void stopPlacingRhythm();
    bool isPlacingRhythm() const noexcept { return placingRhythm; }

    /** Goes back to clicks doing what they did before, without adding notes from the scale or a rhythm. */
    void stopPlacingNotes();

    /** Takes a rhythm out of the ones saved, asking first. */
    void deleteSavedRhythm (const juce::String& name);

    /** Gives another part the active part's chords, asking first if it has notes of its own. */
    void copyProgression (int toPart);

    /** Shows Save Progression's name for the active part's chords, in a box beside its button. */
    void showSaveProgressionPanel (juce::Component& button);

    /** Keeps the active part's chord progression under a name, for every score, asking first
        if it would take the place of one saved with that name.
    */
    void saveProgression (const juce::String& name);

    /** Shows the saved progressions, and how to write them, in a box beside Apply Saved Progression. */
    void showApplyProgressionPanel (juce::Component& button);

    /** Writes a saved progression's chords into the active part, one after another, each in so
        many measures, or as many as its rhythm takes, from the measure it plays from, adding
        measures at the end if they don't all fit, and asking first if they'd take the place of
        notes or chords already there.
    */
    void applyProgression (const SavedProgression&, const ChordStyle&, int measuresPerChord = 1);

    /** Takes a progression out of the ones saved, asking first. */
    void deleteSavedProgression (const juce::String& name);

    /** The measure of the page being edited that it's playing, if it's playing it, or else plays
        from: up to the number of measures, after the last.
    */
    int getPlaybackMeasure() const;

    /** Opens the chord window for a measure of a part, to add a chord or edit the one it has.
        The part becomes the active one.
    */
    void editChord (int part, int measure);

    /** The chord being edited, if the chord window is open. */
    ChordEditor* getChordEditor() const;

    /** Closes the chord window, if it's open, without changing the chord. */
    void closeChordEditor();
    void scrollToMeasure (int measure);
    void pianoKeyClicked (int midiNote);
    void togglePlayback();

    /** Plays the piece, or the page, from where the page being edited plays from. */
    void startPlaying();

    /** Chooses the beat the page plays from, counted from its start, jumping there if it's playing. */
    void choosePlaybackStart (int beat);

    /** Stops playing where it's got to, as Pause does, so it carries on from there, the same
        time through, when it plays again: the page it's on is shown, its marker there.
    */
    void pausePlayback();

    /** Back to the beginning, as stopping goes, of the piece with Piece, the first page shown, or
        of the page with Page: every page plays from its start again.
    */
    void returnToBeginning();

    /** Shows Play as Stop while it's playing, and Pause greyed out unless it is, with their tips. */
    void updateTransportButtons();

    //==============================================================================
    /** Turns MP3 on or off: while it's on, what's heard as the score plays is written to a sound
        file, which is saved as an MP3 once it stops. On, it needs LAME, and without it, says how
        to install it, and stays off. Turned off while it's writing one, that one finishes there.
    */
    void setMakingMP3 (bool);
    bool isMakingMP3() const noexcept { return mp3Button.getToggleState(); }

    /** Starts writing what's heard to a sound file, if MP3 is on and it's playing, unless one's being written already. */
    void startSoundFileIfWanted();

    /** Finishes the sound file being written, once what's heard has died away, and then offers to save it. */
    void finishSoundFile();
    void waitForSoundFile (int tries);

    /** Called with each sound file of what was heard, a WAV, once the playback it's of has
        stopped. To start with, it asks where to save it as an MP3.
    */
    std::function<void (const juce::File& wav)> onSoundFileReady;

    /** Asks where to save a sound file as an MP3, one at a time, as they come. Cancelled, it's thrown away. */
    void chooseWhereToSaveMP3 (const juce::File& wav);
    void showNextMP3Dialog();

    /** Makes an MP3 of a sound file with LAME, on a thread of its own, then deletes the sound
        file, and says so if it couldn't. Calls back on the message thread with whether it did.
    */
    void saveMP3 (const juce::File& wav, const juce::File& mp3, std::function<void (bool saved)> onSaved = {});

    /** LAME, as Homebrew installs it or on the path, if it's installed, or else a file that doesn't exist. */
    static juce::File findLame();
    juce::File getLame();

    /** Goes back to the start of the page being edited: playing, it carries on from there, and
        stopped, the page plays from there.
    */
    void goToPageStart();

    /** Goes back a measure, or on one, to the start of the measure before the one it's at, or the
        one after. Playing, it jumps there, through the measures as they're played, on from one
        page to the next with Piece, and round from one end to the other when looping. Stopped,
        the page plays from there; with Piece, from the start of the next page, or the last
        measure of the one before, past either end of the page.
    */
    void stepMeasure (int direction);

    /** While playing, jumps to a beat of what's played, the keys held while recording ending where it jumps from. */
    void jumpPlayback (double playedBeats);

    /** Whether Play plays every page, one after another, or just the page being edited. */
    bool isPlayingWholePiece() const noexcept { return playWholePiece; }
    void setPlayingWholePiece (bool);

    /** The pages played, the whole piece or the page being edited, and each of them written out
        as it's played, with its repeats, for the instruments to play.
    */
    void updatePerformance();
    std::vector<const Score*> getPerformancePages() const;

    /** A measure as it's played, and where it is in the pages as written. */
    struct PlayedMeasure
    {
        int page, measure, beatsPerMeasure;
        double start;           // in beats from the start of what's played
        double pageStart;       // where its page starts, the time it's played
    };

    /** A point in what's played, as a page, a beat of the page as it's written, and a beat of the
        page as it's played, counting from its start.
    */
    struct WrittenPosition
    {
        int page;
        double beat, playedBeat;
    };

    std::optional<WrittenPosition> toWrittenPosition (double playedBeats) const;

    /** Where playback was paused: the page and measure, as written, how many times the measure
        had been played before, and how far into it it had got.
    */
    struct PausedPosition
    {
        int page, measure, timesBefore;
        double beatsIn;
    };

    /** Where it was paused in what's played now, if it's still played that many times. */
    std::optional<double> findPausedBeat (const PausedPosition&) const;

    /** Where a beat of a page as it's written is first played, if the page is being played. */
    std::optional<double> toPlayedBeats (int page, double writtenBeats) const;

    /** After what's played has changed, carries on from the same place in the same page, the
        same time through, or, if that page isn't played any more, from where the page being
        edited plays from.
    */
    void followPerformanceChange (const std::vector<PlayedMeasure>& before);

    /** After the piece, or the page being edited, has changed. */
    void scoreChanged();
    void pagesChanged();

    /** Adds a copy of a page just after it, with all its music and settings, and goes to it. */
    void copyPage (int page);

    /** Takes out a page, after asking, unless it's the only one. */
    void deletePage (int page);

    /** Moves a page to another place, counting places as they are once it's moved, and goes to it. */
    void movePage (int from, int to);

    /** Starts recording what's played into the active part, playing the score if it isn't
        already, or stops recording, playing on.
    */
    void toggleRecording();

    /** Writes the keys played since last time into the score, while recording. */
    void recordPlayedKeys();

    /** Stops recording, writing the notes of the keys still down as ending at a point, or at the
        end of the score, so it's all undone in one go.
    */
    void finishRecording (std::optional<double> playedBeats);
    void showPlaybackPosition();
    void addMeasure();
    void removeMeasure();

    /** Lets − take the last measure away only when it shows nothing but rests, in every part. */
    void updateRemoveMeasureButton();
    void cloneMeasures();
    void showHeldNotes();
    void tempoEdited (bool finished);

    /** Steps the tempo to the next whole number up or down, within its range. */
    void stepTempo (int direction);

    /** Greys out the arrow that would take the tempo past its range. */
    void updateTempoArrows();
    void scoreReplaced();
    void showDocumentTitle();
    void showPartTitles();

    /** Changes a part's volume, and shows it on its dial. A change made on the dial counts as a
        change to the score.
    */
    void setVolume (int part, float decibels, bool changedOnDial);

    /** Mutes or unmutes a part, and shows it on its dial. A change made on the dial counts as a
        change to the score.
    */
    void setMuted (int part, bool muted, bool changedOnDial);

    /** Keeps each part's volume dial beside the middle of its staves, as they scroll up and down. */
    void positionVolumeDials();

    /** Keeps the measure buttons just after the final barline, halfway down the staves in view,
        and in line with them, each part's button for taking it out, level with the middle of its
        staves. The measure buttons move up or down out of the way of those.
    */
    void positionMeasureButtons();

    /** Keeps each part's volume and muting in the settings, by its number. */
    void saveVolumes();

    juce::PropertiesFile& settings;
    juce::AudioDeviceManager audioDeviceManager;
    InstrumentHost instrumentHost;
    MidiInputs midiInputs { audioDeviceManager, instrumentHost, settings };
    ProgressionLibrary savedProgressions { settings };
    RhythmLibrary savedRhythms { settings };
    EffectRack effectRack { instrumentHost };

    /** The MIDI inputs the MIDI menu showed, the last time it was opened */
    std::vector<MidiInputs::Device> midiMenuDevices;
    Score score;                        // the page being edited
    Piece piece { score };              // every page
    ScoreDocument document;
    ScoreHistory history { piece };
    std::vector<std::unique_ptr<Score>> performances;     // the pages being played, each written out as it's played, which the instruments play
    std::vector<PlayedMeasure> playedMeasures;              // in the order they're played
    int lastPlayingPage = -1;           // the page the music was playing, last time it was shown
    int shownPage = 0;                  // the page being edited, last time the pages changed
    Recorder recorder { piece };
    juce::ApplicationCommandManager commandManager;

    // Sound files of what was heard, waiting to be saved as MP3s, the dialog asking where, and
    // LAME, which makes them, on a thread of its own
    std::vector<juce::File> soundFilesToSave;
    std::unique_ptr<juce::FileChooser> mp3Chooser;
    juce::File lameExecutable;
    juce::ThreadPool mp3Encoder { juce::ThreadPoolOptions{}.withThreadName ("MP3").withNumberOfThreads (1) };

    // Shows the tip for whatever control the pointer rests on, in this window or any other of
    // the app's, such as the chord and effects windows
    controls::TooltipLookAndFeel tooltipLookAndFeel;
    juce::TooltipWindow tooltipWindow;

    controls::TransportButton playButton { controls::TransportButton::Symbol::play };          // a stop button while playing
    controls::TransportButton pauseButton { controls::TransportButton::Symbol::pause };        // beside it, greyed out unless it's playing
    controls::TransportButton recordButton { controls::TransportButton::Symbol::record };
    controls::TransportButton backButton { controls::TransportButton::Symbol::toStart };          // beside it: back to the start of the page,
    controls::TransportButton rewindButton { controls::TransportButton::Symbol::rewind };         // back a measure,
    controls::TransportButton fastForwardButton { controls::TransportButton::Symbol::fastForward };     // and on a measure
    controls::LoopButton loopButton;
    juce::TextButton playModeButton;    // beside Loop: Piece, to play every page, or Page, just the one being edited
    bool playWholePiece = true;
    juce::TextButton mp3Button { "MP3" };           // beside it: writing what's heard as it plays to an MP3
    controls::MetronomeButton metronomeButton;     // beside them
    juce::Label tempoLabel;
    juce::TextEditor tempoEditor;
    controls::StepButton tempoUpButton { true }, tempoDownButton { false };     // beside it, stacked
    juce::ComboBox partBox;             // the active part
    juce::TextButton effectsButton { "Effects" };     // at the right of the toolbar, opening the effects page
    std::vector<Track> tracks;          // for each part, in order; only the active part's instrument panel shows

    ScorePanel scorePanel { score };
    juce::Component sidebarContent;
    juce::Viewport sidebar;

    StaffSystems staffSystems { score };
    ScrollingViewport staffViewport;
    PageTabs pageTabs;                  // along the top of the staves, a tab for each page
    PositionRuler positionRuler { score };     // under them, for choosing where the page plays from
    std::vector<double> playbackStarts { 0.0 };     // where Play and Record start on each page, in beats from its start
    std::optional<PausedPosition> pausedAt;          // where it was paused, until it plays again or the marker moves

    controls::DialLookAndFeel dialLookAndFeel;
    NoteLengthPicker noteLengthPicker;  // at the top left of the score, over the staves: the length of the notes that clicks add
    DynamicPicker dynamicPicker;        // beside it: a dynamic or hairpin to mark instead
    EraserButton eraserButton;          // at the top right of the score, over the staves: the eraser
    controls::TileTextButton editButton { "Edit" };     // to its left: editing notes
    controls::TileTextButton selectButton { "Select" };     // between them: selecting measures
    juce::TextButton deleteMeasuresButton { "Delete" }, moveMeasuresButton { "Move" };     // over the selected measures
    std::optional<std::pair<int, int>> selectedMeasures;    // on the page being edited, from one measure to another
    bool selectionFinished = false;     // the mouse has been let go, so Delete and Move show

    // After the final barline, halfway down what's in view: + and − for adding a measure at the
    // end and taking the last away, and Clone for repeating them all, in every part
    controls::RoundButtonLookAndFeel roundButtonLookAndFeel;
    juce::TextButton addMeasureButton { "+" }, removeMeasureButton { juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92")) };
    juce::TextButton cloneButton { "Clone" };
    VolumeColumn volumeColumn;          // to the left of the staves, holding each part's volume dial

    PianoKeyboard keyboard { instrumentHost.getKeyboardState() };

    int activePart = 0;
    ScaleSettings lastScale;            // what the Scale button's settings start with: the ones used last
    bool placingScale = false;
    bool placingRhythm = false;
    int lastRhythmTimes = 1;            // how many times over a rhythm's written: as many as last time
    juce::String lastRhythmPlaced;
    juce::Component::SafePointer<ApplyRhythmPanel> applyRhythmPanel;     // while it's showing
    int activePartId = 0;               // so the active part can be found again when parts are added or taken out
    ChordStyle lastChordStyle;          // what a new chord starts with: whichever was chosen last
    ChordStyle progressionStyle;        // what a saved progression was applied with last: its chord type and clef start the next
    juce::String lastProgressionApplied;
    juce::Component::SafePointer<ApplyProgressionPanel> applyProgressionPanel;     // while it's showing
    juce::Component::SafePointer<juce::Component> keyListenerTarget;

    // Last, so it goes before the score it edits
    std::unique_ptr<ChordWindow> chordWindow;

    // And before the effects it shows
    std::unique_ptr<EffectsWindow> effectsWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
