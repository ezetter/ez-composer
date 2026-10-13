#pragma once

#include "MusicGlyphs.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>

/** Small buttons, side by side, showing a whole, a dotted half, a half, a dotted quarter, a
    quarter, a dotted eighth, an eighth, a 16th and a 32nd note, for choosing how long the notes
    that clicks add are. One is chosen at a time. After them, a little apart, a staccato dot turns
    staccato on or off for the notes clicks add, whatever their length.
*/
class NoteLengthPicker final : public juce::Component
{
public:
    NoteLengthPicker();

    /** The length chosen, in beats: 4, 3, 2, 1.5, 1, 0.75, 0.5, 0.25 or 0.125. */
    double getLength() const noexcept { return length; }

    /** Chooses a length, in beats, without calling onChange. */
    void setLength (double beats);

    /** Shows which length is chosen, or none while clicks are doing something else, such as
        marking dynamics. Clicking a length shows it again, and calls onChange.
    */
    void setChoiceShown (bool);

    /** Called when a button's clicked, with the length it chooses. */
    std::function<void (double beats)> onChange;

    /** Whether the notes clicks add are staccato. It stays on until it's clicked again. */
    bool isStaccato() const noexcept { return staccatoButton.getToggleState(); }

    /** Turns staccato on or off, without calling onStaccatoChange. */
    void setStaccato (bool);

    /** Called when the staccato button's clicked, with whether it's on now. */
    std::function<void (bool staccato)> onStaccatoChange;

    /** The size the buttons need, side by side. */
    juce::Rectangle<int> getIdealBounds() const;

    void resized() override;

    static constexpr int buttonSize = 28;

    /** The notes are drawn as if on a staff with spaces this size. */
    static constexpr float glyphStaffSpace = 6.5f;

    /** Draws a note of a length, in beats, in the current colour, centred in an area the size of
        a button, with glyphs sized for glyphStaffSpace.
    */
    static void drawNote (juce::Graphics&, const MusicGlyphs&, double beats, juce::Rectangle<float> area);

    /** Draws a staccato dot in the current colour, centred in an area the size of a button. */
    static void drawStaccato (juce::Graphics&, juce::Rectangle<float> area);

    static constexpr int groupGap = 8;      // more space between the lengths and the staccato dot

private:
    /** A button showing a note. */
    struct NoteButton final : public juce::Button
    {
        NoteButton (const MusicGlyphs&, double beats);

        void paintButton (juce::Graphics&, bool highlighted, bool down) override;

        const MusicGlyphs& glyphs;
        const double beats;
    };

    /** The button showing a staccato dot, which goes on and off. */
    struct StaccatoButton final : public juce::Button
    {
        StaccatoButton();

        void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    };

    MusicGlyphs glyphs;
    std::array<std::unique_ptr<NoteButton>, 9> buttons;
    StaccatoButton staccatoButton;
    double length = 1.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NoteLengthPicker)
};
