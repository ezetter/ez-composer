#pragma once

#include "MusicGlyphs.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>

/** Small buttons, side by side, showing a whole, a dotted half, a half, a dotted quarter, a
    quarter, a dotted eighth, an eighth, a 16th and a 32nd note, for choosing how long the notes
    that clicks add are. One is chosen at a time. After them, a little apart, a staccato dot turns
    staccato on or off for the notes clicks add, whatever their length. After that, a little apart
    again, a 3 chooses triplets in place of a length, three notes in the time of two, showing the
    length of their notes.
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

    /** Whether triplets are chosen, in place of a length, and their notes' length, in beats */
    bool isTriplet() const noexcept { return triplet; }
    double getTripletLength() const noexcept { return tripletLength; }

    /** Chooses triplets, with their notes' length in beats, without calling onTripletClicked. Choosing a length goes back to single notes. */
    void setTriplet (double noteBeats);

    /** Called when the triplet button's clicked, e.g. to offer the lengths its notes can be. */
    std::function<void()> onTripletClicked;

    /** The triplet button, to show the lengths beside */
    juce::Component& getTripletButton() noexcept { return tripletButton; }

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

    /** Draws a triplet's note of a length, in beats, with a 3 over it, in an area the size of a button. */
    static void drawTriplet (juce::Graphics&, const MusicGlyphs&, double beats, juce::Rectangle<float> area);

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

    /** The button showing a 3, over a note of the triplets' notes' length. */
    struct TripletButton final : public juce::Button
    {
        TripletButton (const MusicGlyphs&, const double& beats);

        void paintButton (juce::Graphics&, bool highlighted, bool down) override;

        const MusicGlyphs& glyphs;
        const double& beats;
    };

    MusicGlyphs glyphs;
    std::array<std::unique_ptr<NoteButton>, 9> buttons;
    StaccatoButton staccatoButton;
    double length = 1.0;
    bool triplet = false;
    double tripletLength = 0.5;
    bool choiceShown = true;
    TripletButton tripletButton { glyphs, tripletLength };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NoteLengthPicker)
};

/** The lengths a triplet's notes can be, as buttons, one of them chosen: half notes, quarters,
    eighths and 16ths, each with a 3 over it.
*/
class TripletLengthPanel final : public juce::Component
{
public:
    explicit TripletLengthPanel (double chosenBeats);

    /** Called when one's clicked, with its notes' length in beats. */
    std::function<void (double beats)> onChoose;

    void resized() override;

private:
    struct LengthButton final : public juce::Button
    {
        LengthButton (const MusicGlyphs&, double beats);

        void paintButton (juce::Graphics&, bool highlighted, bool down) override;

        const MusicGlyphs& glyphs;
        const double beats;
    };

    MusicGlyphs glyphs;
    juce::Label heading;
    std::vector<std::unique_ptr<LengthButton>> buttons;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TripletLengthPanel)
};
