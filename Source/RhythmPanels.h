#pragma once

#include "Controls.h"
#include "Rhythms.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

/** What Save Rhythm shows: how many measures to save the rhythm of, from the one it plays from,
    only as many as can be, what's in them, and a name, with Save and Cancel.
*/
class SaveRhythmPanel final : public juce::Component
{
public:
    /** For a part, from a measure, with the numbers of measures there are to choose from, the
        rhythm each would give, and a name to start with.
    */
    SaveRhythmPanel (int part, int firstMeasure, const std::vector<int>& lengthChoices,
                     std::function<SavedRhythm (int numMeasures)> getRhythmFor, const juce::String& name);

    /** Called when Save is clicked, or Return pressed, with the name, tidied, and the number of measures. */
    std::function<void (const juce::String& name, int numMeasures)> onSave;
    std::function<void()> onCancel;

    int getNumMeasures() const;

    void resized() override;

private:
    void parentHierarchyChanged() override;
    void save();
    void update();

    std::function<SavedRhythm (int)> getRhythmFor;
    juce::Label heading, fromLabel, measuresLabel, summaryLabel, nameLabel;
    juce::ComboBox measuresBox;
    juce::TextEditor nameEditor;
    juce::TextButton saveButton { "Save" }, cancelButton { "Cancel" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SaveRhythmPanel)
};

/** What Apply Saved Rhythm shows: the rhythms saved, to choose one, and how many times over to
    write it, with Place, which then waits for a click on a staff, Cancel, and Delete for the one
    chosen.
*/
class ApplyRhythmPanel final : public juce::Component,
                               private juce::ListBoxModel
{
public:
    static constexpr int maxTimes = 16;

    /** Starting with so many times, and a rhythm chosen, if it's among them, or else the first. */
    ApplyRhythmPanel (const std::vector<SavedRhythm>&, int times, const juce::String& chosenName);

    /** Shows the rhythms again, e.g. once one's been deleted, keeping the one chosen if it's still there. */
    void setRhythms (const std::vector<SavedRhythm>&);

    const SavedRhythm* getChosen() const;
    int getTimes() const;

    /** Called when Place is clicked, or a rhythm double-clicked. */
    std::function<void (const SavedRhythm&, int times)> onPlace;

    /** Called when Delete is clicked, with the chosen rhythm's name. */
    std::function<void (const juce::String& name)> onDelete;
    std::function<void()> onCancel;

    void resized() override;

private:
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void selectedRowsChanged (int lastRowSelected) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
    juce::String getTooltipForRow (int row) override;

    void place();
    void updateButtons();

    std::vector<SavedRhythm> rhythms;
    juce::Label heading, howLabel, timesLabel;
    juce::ListBox list { "Saved rhythms", this };
    juce::Slider timesSlider;
    juce::TextButton deleteButton { "Delete" }, cancelButton { "Cancel" }, placeButton { "Place" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ApplyRhythmPanel)
};
