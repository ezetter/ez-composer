#pragma once

#include "Controls.h"
#include "Score.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

/** The settings for the whole score, its key, major or minor, and time signature, and for the
    active part, what its alternate staff shows, a button to copy its chords to another part (the
    other one, when there are two, or one chosen from a menu, when there are more), buttons to
    save its chord progression and to apply a saved one, and a button for generating notes from
    the key's scale.
*/
class ScorePanel final : public juce::Component,
                         private juce::ChangeListener
{
public:
    explicit ScorePanel (Score&);
    ~ScorePanel() override;

    /** Chooses the part whose alternate staff the panel shows and sets, and whose chords it copies. */
    void setPart (int);

    /** Called when Copy Chords is clicked, to copy the part's chords to another part. */
    std::function<void (int toPart)> onCopyProgression;

    /** Called when Generate Notes from Key is clicked, with the button, to show the scale's settings beside it. */
    std::function<void (juce::Component& button)> onScale;

    /** Called when Save Progression or Apply Saved Progression is clicked, with the button, to show what it does beside it. */
    std::function<void (juce::Component& button)> onSaveProgression, onApplyProgression;

    /** Says how many progressions are saved, so Apply Saved Progression can say there are none. */
    void setNumSavedProgressions (int);

    /** The parts Copy Chords offers to copy to, in its menu when there's more than one. */
    juce::PopupMenu getCopyTargetsMenu() const;

    int getIdealHeight() const;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void update();

    Score& score;
    int part = 0;

    juce::Label keyHeading, signatureLabel, timeSignatureHeading, alternateHeading, progressionHeading;
    juce::ComboBox keyBox, timeSignatureBox, alternateBox;
    juce::TextButton majorButton { "Major" }, minorButton { "Minor" };
    juce::TextButton copyChordsButton;
    juce::TextButton saveProgressionButton { "Save Progression" + controls::fromUTF8 ("\xe2\x80\xa6") };
    juce::TextButton applyProgressionButton { "Apply Saved Progression" + controls::fromUTF8 ("\xe2\x80\xa6") };
    int numSavedProgressions = 0;
    juce::TextButton generateNotesButton { "Generate Notes from Key" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScorePanel)
};
