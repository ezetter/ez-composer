#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include <functional>
#include <map>
#include <optional>

/** The on-screen piano. It reports clicked keys, and can mark a chord's notes on its keys. */
class PianoKeyboard final : public juce::MidiKeyboardComponent,
                            public juce::SettableTooltipClient
{
public:
    explicit PianoKeyboard (juce::MidiKeyboardState&);

    /** Called when a key is clicked with the mouse. */
    std::function<void (int midiNote)> onKeyClicked;

    /** Tints these notes' keys and labels them with the notes' names. */
    void setHeldNotes (std::map<int, juce::String>);

    /** Colours one note's key blue, for the note selected on the staves, or none. */
    void setSelectedNote (std::optional<int> midiNote);

private:
    bool mouseDownOnKey (int midiNoteNumber, const juce::MouseEvent&) override;
    juce::String getWhiteNoteText (int midiNoteNumber) override;
    void drawWhiteNote (int midiNoteNumber, juce::Graphics&, juce::Rectangle<float> area,
                        bool isDown, bool isOver, juce::Colour lineColour, juce::Colour textColour) override;
    void drawBlackNote (int midiNoteNumber, juce::Graphics&, juce::Rectangle<float> area,
                        bool isDown, bool isOver, juce::Colour noteFillColour) override;

    std::map<int, juce::String> heldNotes;
    std::optional<int> selectedNote;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PianoKeyboard)
};
