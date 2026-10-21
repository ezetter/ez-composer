#pragma once

#include "Controls.h"
#include "Progressions.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>

/** What Save Progression shows: the chords to be saved, and a name for them, which starts as
    their numerals, with Save and Cancel.
*/
class SaveProgressionPanel final : public juce::Component
{
public:
    SaveProgressionPanel (const SavedProgression&, int part);

    /** Called when Save is clicked, or Return pressed, with the name, tidied: the numerals if it's blank. */
    std::function<void (const juce::String& name)> onSave;
    std::function<void()> onCancel;

    void resized() override;

private:
    void parentHierarchyChanged() override;
    void save();

    const juce::String numerals;
    juce::Label heading, summaryLabel, nameLabel;
    juce::TextEditor nameEditor;
    juce::TextButton saveButton { "Save" }, cancelButton { "Cancel" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SaveProgressionPanel)
};

/** What Search, in Apply Saved Progression, shows: the first letters of a name, a number of
    chords, and for each of the first four chords whether it's major or minor, its number, flat or
    sharp, its added note, sus, and augmented or diminished, each Any to start with, which leaves
    it out. It says how many progressions match, as it changes, with Clear and Done.
*/
class SearchProgressionsPanel final : public juce::Component
{
public:
    /** Starting with a search, with up to so many chords to choose from, and what to say of the
        progressions a search finds, e.g. "3 of 10 match".
    */
    SearchProgressionsPanel (const ProgressionSearch&, int maxChords, std::function<juce::String (const ProgressionSearch&)> describeFound);

    ProgressionSearch getSearch() const;

    /** Called whenever any of it changes, with the search as it is now. */
    std::function<void (const ProgressionSearch&)> onChange;
    std::function<void()> onDone;

    void resized() override;

private:
    enum Row { quality, number, accidental, addedNote, suspension, alteration, numRows };

    void parentHierarchyChanged() override;
    void setSearch (const ProgressionSearch&);
    void changed();

    std::function<juce::String (const ProgressionSearch&)> describeFound;
    juce::Label heading, nameLabel, countLabel, foundLabel;
    juce::TextEditor nameEditor;
    juce::ComboBox countBox;
    std::array<juce::Label, ProgressionSearch::numChordsSearched> columnLabels;
    std::array<juce::Label, numRows> rowLabels;
    std::array<std::array<juce::ComboBox, ProgressionSearch::numChordsSearched>, numRows> boxes;
    juce::TextButton clearButton { "Clear" }, doneButton { "Done" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SearchProgressionsPanel)
};

/** What Apply Saved Progression shows: the progressions saved, to choose one, and the chord
    type, clef and note length to write its chords in, as the chord window has them, with Apply,
    Cancel, and Delete for the one chosen. Search… narrows the list down to the progressions
    that match a search, until it's cleared.
*/
class ApplyProgressionPanel final : public juce::Component,
                                    private juce::ListBoxModel
{
public:
    /** For a part, from a measure, in measures of so many beats, starting with a style, and with
        a progression chosen, if it's among them, or else the first, and the saved rhythms to
        write its chords in to choose from.
    */
    ApplyProgressionPanel (const std::vector<SavedProgression>&, const ChordStyle&, int part, int measure, int beatsPerMeasure,
                           const juce::String& chosenName, std::vector<Rhythm> rhythms = {});

    /** Shows the progressions again, e.g. once one's been deleted, keeping the one chosen if it's still there. */
    void setProgressions (const std::vector<SavedProgression>&);

    /** Shows only the progressions that match, keeping the one chosen if it's among them. */
    void setSearch (const ProgressionSearch&);
    const ProgressionSearch& getSearch() const noexcept { return search; }

    /** The progression chosen, if any */
    const SavedProgression* getChosen() const;

    ChordStyle getStyle() const;

    static constexpr int maxMeasuresPerChord = 16;

    /** How many measures each chord goes in, one after another: 1 to start with */
    int getMeasuresPerChord() const;

    /** Called when Apply is clicked, or a progression double-clicked, with the progression, the
        style to write its chords in, and how many measures each goes in.
    */
    std::function<void (const SavedProgression&, const ChordStyle&, int measuresPerChord)> onApply;

    /** Called when Delete is clicked, with the chosen progression's name. */
    std::function<void (const juce::String& name)> onDelete;
    std::function<void()> onCancel;

    void resized() override;

private:
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void selectedRowsChanged (int lastRowSelected) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
    juce::String getTooltipForRow (int row) override;

    void apply();
    void updateButtons();
    void showSearch();
    void showProgressions (const std::vector<SavedProgression>& shown, const juce::String& chosenName, int chosenRow);
    void updateLength();

    std::vector<SavedProgression> allProgressions, progressions;     // all of them, and those the search finds, which are shown
    ProgressionSearch search;

    juce::Label heading, targetLabel, searchLabel, typeLabel, clefLabel, lengthLabel, rhythmLabel, measuresLabel;
    juce::Slider measuresSlider;
    juce::ListBox list { "Saved progressions", this };
    juce::ComboBox typeBox, lengthBox, rhythmBox;
    std::vector<Rhythm> rhythms;
    juce::TextButton trebleButton { "Treble clef" }, bassButton { "Bass clef" };
    juce::TextButton searchButton, clearSearchButton { "Clear" };
    juce::TextButton deleteButton { "Delete" }, cancelButton { "Cancel" }, applyButton { "Apply" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ApplyProgressionPanel)
};
