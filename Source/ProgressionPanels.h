#pragma once

#include "Controls.h"
#include "Progressions.h"

#include <juce_gui_basics/juce_gui_basics.h>

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

/** What Apply Saved Progression shows: the progressions saved, to choose one, and the chord
    type, clef and note length to write its chords in, as the chord window has them, with Apply,
    Cancel, and Delete for the one chosen.
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

    /** The progression chosen, if any */
    const SavedProgression* getChosen() const;

    ChordStyle getStyle() const;

    /** Called when Apply is clicked, or a progression double-clicked. */
    std::function<void (const SavedProgression&, const ChordStyle&)> onApply;

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

    std::vector<SavedProgression> progressions;
    void updateLength();

    juce::Label heading, targetLabel, typeLabel, clefLabel, lengthLabel, rhythmLabel;
    juce::ListBox list { "Saved progressions", this };
    juce::ComboBox typeBox, lengthBox, rhythmBox;
    std::vector<Rhythm> rhythms;
    juce::TextButton trebleButton { "Treble clef" }, bassButton { "Bass clef" };
    juce::TextButton deleteButton { "Delete" }, cancelButton { "Cancel" }, applyButton { "Apply" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ApplyProgressionPanel)
};
