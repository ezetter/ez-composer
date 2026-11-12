#pragma once

#include "Controls.h"
#include "MusicGlyphs.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>

/** Small buttons, side by side, showing a whole, a dotted half, a half, a dotted quarter, a
    quarter, a dotted eighth, an eighth, a 16th, a 32nd and a 64th note, for choosing how long the notes
    that clicks add are. One is chosen at a time. After them, a little apart, a staccato dot turns
    staccato on or off for the notes clicks add, whatever their length. After that, a little apart
    again, a tuplet button chooses tuplets in place of a length, from 2 to 10 notes, showing how
    many and the length of their notes.
*/
class NoteLengthPicker final : public juce::Component
{
public:
    NoteLengthPicker();

    /** The length chosen, in beats: 4, 3, 2, 1.5, 1, 0.75, 0.5, 0.25, 0.125 or 0.0625. */
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

    /** Whether tuplets are chosen, in place of a length, their notes' length, in beats, and how many notes they have */
    bool isTuplet() const noexcept { return tuplet; }
    double getTupletLength() const noexcept { return tupletLength; }
    int getTupletNotes() const noexcept { return tupletNotes; }

    /** Chooses tuplets, with their notes' length in beats and how many there are, without calling
        onTupletClicked. Choosing a length goes back to single notes.
    */
    void setTuplet (double noteBeats, int numNotes);

    /** Called when the tuplet button's clicked, e.g. to offer how many notes and their length. */
    std::function<void()> onTupletClicked;

    /** The tuplet button, to show the lengths beside */
    juce::Component& getTupletButton() noexcept { return tupletButton; }

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

    /** Draws a tuplet's note of a length, in beats, with how many notes it has over it, in an area the size of a button. */
    static void drawTuplet (juce::Graphics&, const MusicGlyphs&, double beats, int numNotes, juce::Rectangle<float> area);

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

    /** The button showing how many notes the tuplets have, over a note of their notes' length. */
    struct TupletButton final : public juce::Button
    {
        TupletButton (const MusicGlyphs&, const double& beats, const int& numNotes);

        void paintButton (juce::Graphics&, bool highlighted, bool down) override;

        const MusicGlyphs& glyphs;
        const double& beats;
        const int& numNotes;
    };

    MusicGlyphs glyphs;
    std::array<std::unique_ptr<NoteButton>, 10> buttons;
    StaccatoButton staccatoButton;
    double length = 1.0;
    bool tuplet = false;
    double tupletLength = 0.5;
    int tupletNotes = 3;
    bool choiceShown = true;
    TupletButton tupletButton { glyphs, tupletLength, tupletNotes };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NoteLengthPicker)
};

/** How many notes tuplets have, from 2 to 10, as a row of numbered buttons, and the lengths their
    notes can be, as buttons under them: half notes, quarters, eighths, 16ths and 32nds, each with the
    number over it. One of each is chosen. Lengths whose tuplets would be longer than a 4/4
    measure can't be chosen.
*/
class TupletLengthPanel final : public juce::Component
{
public:
    TupletLengthPanel (double chosenBeats, int chosenNotes);

    /** Called when a number or a length is clicked, with the notes' length in beats, how many
        there are, and whether it was a length, which finishes choosing.
    */
    std::function<void (double beats, int numNotes, bool finished)> onChoose;

    void resized() override;

    /** Whether tuplets of so many notes of a length fit in a 4/4 measure */
    static bool fits (double beats, int numNotes);

private:
    struct LengthButton final : public juce::Button
    {
        LengthButton (const MusicGlyphs&, double beats, const int& numNotes);

        void paintButton (juce::Graphics&, bool highlighted, bool down) override;

        const MusicGlyphs& glyphs;
        const double beats;
        const int& numNotes;
    };

    /** Shows what's chosen, and which lengths fit. */
    void update();

    MusicGlyphs glyphs;
    juce::Label notesHeading, lengthHeading;
    std::vector<std::unique_ptr<controls::TileTextButton>> countButtons;    // 2 to 10
    std::vector<std::unique_ptr<LengthButton>> buttons;
    double beats;
    int numNotes;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TupletLengthPanel)
};
