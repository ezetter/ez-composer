#include "NoteLengthPicker.h"

#include "Controls.h"
#include "Score.h"

namespace
{
    constexpr int gap = 3;
}

NoteLengthPicker::NoteLengthPicker()
    : glyphs (glyphStaffSpace)
{
    // Whole, dotted half, half, dotted quarter, quarter, dotted eighth, eighth, 16th, 32nd and 64th, longest first
    for (auto [index, beats, name] : { std::tuple { 0, 4.0, "Whole" }, { 1, 3.0, "Dotted half" }, { 2, 2.0, "Half" },
                                       { 3, 1.5, "Dotted quarter" }, { 4, 1.0, "Quarter" }, { 5, 0.75, "Dotted eighth" },
                                       { 6, 0.5, "Eighth" }, { 7, 0.25, "16th" }, { 8, 0.125, "32nd" }, { 9, 0.0625, "64th" } })
    {
        auto& button = buttons[(size_t) index];
        button = std::make_unique<NoteButton> (glyphs, beats);
        button->setTooltip (juce::String (name) + " notes: click the staff to add " + juce::String (name).toLowerCase() + " notes");
        button->setClickingTogglesState (true);
        button->setRadioGroupId (1);
        button->setWantsKeyboardFocus (false);
        button->onClick = [this, chosen = beats]
        {
            length = chosen;
            tuplet = false;
            setChoiceShown (true);

            if (onChange != nullptr)
                onChange (chosen);
        };
        addAndMakeVisible (*button);
    }

    // Staccato goes with whichever length's chosen, and stays on until it's clicked again.
    staccatoButton.setTooltip ("Staccato: the notes you add are played short and detached, with a dot over or under them. Click again to turn it off.");
    staccatoButton.setClickingTogglesState (true);
    staccatoButton.setWantsKeyboardFocus (false);
    staccatoButton.onClick = [this]
    {
        if (onStaccatoChange != nullptr)
            onStaccatoChange (staccatoButton.getToggleState());
    };
    addAndMakeVisible (staccatoButton);

    // Tuplets go in place of a length.
    tupletButton.setTooltip ("Tuplets: from 2 to 10 notes evenly in the time of others, e.g. 3 in the time of 2. Click to choose how many and their length,\nthen click the staff for each note.");
    tupletButton.setWantsKeyboardFocus (false);
    tupletButton.onClick = [this]
    {
        tuplet = true;
        setChoiceShown (true);

        if (onTupletClicked != nullptr)
            onTupletClicked();
    };
    addAndMakeVisible (tupletButton);

    setLength (1.0);
    setSize (getIdealBounds().getWidth(), getIdealBounds().getHeight());
}

void NoteLengthPicker::setStaccato (bool shouldBeStaccato)
{
    staccatoButton.setToggleState (shouldBeStaccato, juce::dontSendNotification);
}

void NoteLengthPicker::setLength (double beats)
{
    length = beats;
    tuplet = false;
    setChoiceShown (true);
}

void NoteLengthPicker::setTuplet (double noteBeats, int numNotes)
{
    tupletLength = noteBeats;
    tupletNotes = juce::jlimit (Tuplet::minNotes, Tuplet::maxNotes, numNotes);
    tuplet = true;
    setChoiceShown (true);
}

void NoteLengthPicker::setChoiceShown (bool shown)
{
    choiceShown = shown;

    for (auto& button : buttons)
        button->setToggleState (shown && ! tuplet && juce::exactlyEqual (button->beats, length), juce::dontSendNotification);

    tupletButton.setToggleState (shown && tuplet, juce::dontSendNotification);
    tupletButton.repaint();
}

juce::Rectangle<int> NoteLengthPicker::getIdealBounds() const
{
    return { (int) buttons.size() * buttonSize + ((int) buttons.size() - 1) * gap + 2 * (groupGap + buttonSize), buttonSize };
}

void NoteLengthPicker::resized()
{
    for (size_t i = 0; i < buttons.size(); ++i)
        buttons[i]->setBounds ((int) i * (buttonSize + gap), 0, buttonSize, buttonSize);

    staccatoButton.setBounds (buttons.back()->getRight() + groupGap, 0, buttonSize, buttonSize);
    tupletButton.setBounds (staccatoButton.getRight() + groupGap, 0, buttonSize, buttonSize);
}

//==============================================================================
NoteLengthPicker::NoteButton::NoteButton (const MusicGlyphs& glyphsToUse, double beatsToShow)
    : juce::Button ({}),
      glyphs (glyphsToUse),
      beats (beatsToShow)
{
}

void NoteLengthPicker::NoteButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    g.setColour (controls::drawSymbolButtonTile (g, *this, highlighted, down));
    drawNote (g, glyphs, beats, getLocalBounds().toFloat().reduced (0.5f));
}

void NoteLengthPicker::drawNote (juce::Graphics& g, const MusicGlyphs& glyphs, double beats, juce::Rectangle<float> bounds)
{
    // The note: an open notehead for a whole or half note, a filled one for a dotted quarter or
    // shorter, a stem up for all but the whole note, flags on the stems of an eighth, 16th, 32nd or 64th,
    // and a dot after a dotted half, quarter or eighth.
    const auto glyph = beats >= 4.0 ? Smufl::noteheadWhole : beats >= 2.0 ? Smufl::noteheadHalf : Smufl::noteheadBlack;
    const auto dotted = juce::exactlyEqual (beats, 3.0) || juce::exactlyEqual (beats, 1.5) || juce::exactlyEqual (beats, 0.75);
    const auto flagged = beats < 1.0;
    const auto headBounds = glyphs.getPath (glyph).getBounds();
    const auto hasStem = beats < 4.0;
    const auto flagGlyph = beats >= 0.5 ? Smufl::flag8thUp : beats >= 0.25 ? Smufl::flag16thUp : beats >= 0.125 ? Smufl::flag32ndUp : Smufl::flag64thUp;
    const auto flagAnchor = beats >= 0.5 ? -0.04f : beats >= 0.25 ? -0.088f : beats >= 0.125 ? 0.376f : 1.168f;     // from Bravura's metadata
    const auto stemLength = (beats < 0.125 ? 4.0f : beats < 0.25 ? 3.4f : 2.8f) * glyphStaffSpace;
    const auto noteHeight = headBounds.getHeight() + (hasStem ? stemLength - headBounds.getHeight() / 2.0f : 0.0f);

    // A dotted or flagged note is moved left a little, to leave room for its dot or flag, and one
    // with both more, as its dot goes after the flag.
    const auto centre = bounds.getCentre().translated ((dotted && flagged ? -0.9f : dotted || flagged ? -0.35f : 0.0f) * glyphStaffSpace, 0.0f);
    const auto origin = juce::Point<float> (centre.x - headBounds.getCentreX(),
                                            centre.y + noteHeight / 2.0f - headBounds.getBottom());

    glyphs.draw (g, glyph, origin);

    if (dotted)
    {
        const auto afterFlag = flagged ? glyphs.getPath (flagGlyph).getBounds().getRight() + 0.1f * glyphStaffSpace : 0.3f * glyphStaffSpace;
        glyphs.draw (g, Smufl::augmentationDot, { origin.x + headBounds.getRight() + afterFlag, origin.y });
    }

    if (hasStem)
    {
        const auto stemThickness = 0.13f * glyphStaffSpace * 1.6f;
        const auto right = origin.x + headBounds.getRight();
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (right - stemThickness, origin.y - stemLength,
                                                                right, origin.y - 0.168f * glyphStaffSpace));

        // The flag's anchor meets the top of the stem.
        if (flagged)
            glyphs.draw (g, flagGlyph, { right - stemThickness, origin.y - stemLength + flagAnchor * glyphStaffSpace });
    }
}

void NoteLengthPicker::drawStaccato (juce::Graphics& g, juce::Rectangle<float> bounds)
{
    // Just the dot, big enough to see at a glance
    constexpr float diameter = 5.5f;
    g.fillEllipse (juce::Rectangle<float> (diameter, diameter).withCentre (bounds.getCentre()));
}

//==============================================================================
NoteLengthPicker::StaccatoButton::StaccatoButton()
    : juce::Button ("Staccato")
{
}

void NoteLengthPicker::StaccatoButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    g.setColour (controls::drawSymbolButtonTile (g, *this, highlighted, down));
    drawStaccato (g, getLocalBounds().toFloat().reduced (0.5f));
}

void NoteLengthPicker::drawTuplet (juce::Graphics& g, const MusicGlyphs& glyphs, double beats, int numNotes, juce::Rectangle<float> bounds)
{
    // The note a little lower and smaller-looking, under the number
    drawNote (g, glyphs, beats, bounds.withTrimmedTop (bounds.getHeight() * 0.22f).withTrimmedLeft (bounds.getWidth() * 0.12f));
    g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    g.drawText (juce::String (numNotes), bounds.withHeight (bounds.getHeight() * 0.4f).withTrimmedRight (bounds.getWidth() * 0.45f).translated (2.0f, 1.0f),
                juce::Justification::centred, false);
}

//==============================================================================
NoteLengthPicker::TupletButton::TupletButton (const MusicGlyphs& glyphsToUse, const double& beatsToShow, const int& notesToShow)
    : juce::Button ("Tuplets"),
      glyphs (glyphsToUse),
      beats (beatsToShow),
      numNotes (notesToShow)
{
}

void NoteLengthPicker::TupletButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    g.setColour (controls::drawSymbolButtonTile (g, *this, highlighted, down));
    drawTuplet (g, glyphs, beats, numNotes, getLocalBounds().toFloat().reduced (0.5f));
}

//==============================================================================
namespace
{
    constexpr int panelMargin = 12, headingHeight = 20, countSize = 26, countGap = 3, lengthSize = NoteLengthPicker::buttonSize + 6, lengthGap = 4;
}

bool TupletLengthPanel::fits (double beats, int numNotes)
{
    return Tuplet::getNormalCount (numNotes) * beats <= (double) Score::maxBeatsPerMeasure + 1.0e-9;
}

TupletLengthPanel::TupletLengthPanel (double chosenBeats, int chosenNotes)
    : glyphs (NoteLengthPicker::glyphStaffSpace),
      beats (chosenBeats),
      numNotes (juce::jlimit (Tuplet::minNotes, Tuplet::maxNotes, chosenNotes))
{
    for (auto [label, text] : { std::pair { &notesHeading, "Notes" }, { &lengthHeading, "Note length" } })
    {
        label->setText (text, juce::dontSendNotification);
        label->setFont (juce::FontOptions (13.0f));
        label->setBorderSize ({});
        label->setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (label);
    }

    // How many notes: choosing a number keeps the box open, for the length, which it may have to
    // shorten for the tuplet to fit.
    for (int count = Tuplet::minNotes; count <= Tuplet::maxNotes; ++count)
    {
        auto button = std::make_unique<controls::TileTextButton> (juce::String (count));
        button->setTooltip (juce::String (count) + " notes in the time of " + juce::String (Tuplet::getNormalCount (count)));
        button->onClick = [this, count]
        {
            numNotes = count;

            if (! fits (beats, numNotes))
                for (auto length : Score::tupletNoteLengths)
                    if (fits (length, numNotes))
                    {
                        beats = length;
                        break;
                    }

            update();

            if (onChoose != nullptr)
                onChoose (beats, numNotes, false);
        };
        addAndMakeVisible (*button);
        countButtons.push_back (std::move (button));
    }

    for (auto [length, name] : { std::pair { 2.0, "Half" }, { 1.0, "Quarter" }, { 0.5, "Eighth" }, { 0.25, "16th" }, { 0.125, "32nd" } })
    {
        auto button = std::make_unique<LengthButton> (glyphs, length, numNotes);
        button->setTooltip (juce::String (name) + "-note tuplets");
        button->setWantsKeyboardFocus (false);
        button->onClick = [this, chosen = length]
        {
            beats = chosen;
            update();

            if (onChoose != nullptr)
                onChoose (beats, numNotes, true);
        };
        addAndMakeVisible (*button);
        buttons.push_back (std::move (button));
    }

    update();

    const auto countsWidth = (int) countButtons.size() * (countSize + countGap) - countGap;
    const auto lengthsWidth = (int) buttons.size() * (lengthSize + lengthGap) - lengthGap;
    setSize (2 * panelMargin + juce::jmax (countsWidth, lengthsWidth),
             2 * panelMargin + headingHeight + countSize + 10 + headingHeight + lengthSize);
}

void TupletLengthPanel::update()
{
    for (size_t i = 0; i < countButtons.size(); ++i)
        countButtons[i]->setToggleState ((int) i + Tuplet::minNotes == numNotes, juce::dontSendNotification);

    for (auto& button : buttons)
    {
        button->setToggleState (juce::exactlyEqual (button->beats, beats), juce::dontSendNotification);
        button->setEnabled (fits (button->beats, numNotes));
        button->repaint();
    }
}

void TupletLengthPanel::resized()
{
    auto area = getLocalBounds().reduced (panelMargin);
    notesHeading.setBounds (area.removeFromTop (headingHeight));
    auto countRow = area.removeFromTop (countSize);

    for (auto& button : countButtons)
    {
        button->setBounds (countRow.removeFromLeft (countSize));
        countRow.removeFromLeft (countGap);
    }

    area.removeFromTop (10);
    lengthHeading.setBounds (area.removeFromTop (headingHeight));
    auto lengthRow = area.removeFromTop (lengthSize);

    for (auto& button : buttons)
    {
        button->setBounds (lengthRow.removeFromLeft (lengthSize));
        lengthRow.removeFromLeft (lengthGap);
    }
}

TupletLengthPanel::LengthButton::LengthButton (const MusicGlyphs& glyphsToUse, double beatsToShow, const int& notesToShow)
    : juce::Button ({}),
      glyphs (glyphsToUse),
      beats (beatsToShow),
      numNotes (notesToShow)
{
}

void TupletLengthPanel::LengthButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    g.setColour (controls::drawSymbolButtonTile (g, *this, highlighted, down));

    if (! isEnabled())
        g.setColour (juce::Colours::black.withAlpha (0.2f));

    NoteLengthPicker::drawTuplet (g, glyphs, beats, numNotes, getLocalBounds().toFloat().reduced (3.5f));
}
