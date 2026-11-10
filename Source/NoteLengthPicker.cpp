#include "NoteLengthPicker.h"

#include "Controls.h"

namespace
{
    constexpr int gap = 3;
}

NoteLengthPicker::NoteLengthPicker()
    : glyphs (glyphStaffSpace)
{
    // Whole, dotted half, half, dotted quarter, quarter, dotted eighth, eighth, 16th and 32nd, longest first
    for (auto [index, beats, name] : { std::tuple { 0, 4.0, "Whole" }, { 1, 3.0, "Dotted half" }, { 2, 2.0, "Half" },
                                       { 3, 1.5, "Dotted quarter" }, { 4, 1.0, "Quarter" }, { 5, 0.75, "Dotted eighth" },
                                       { 6, 0.5, "Eighth" }, { 7, 0.25, "16th" }, { 8, 0.125, "32nd" } })
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
            triplet = false;
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

    // Triplets go in place of a length.
    tripletButton.setTooltip ("Triplets: three notes in the time of two. Click to choose their notes' length, then click the staff three times for the notes.");
    tripletButton.setWantsKeyboardFocus (false);
    tripletButton.onClick = [this]
    {
        triplet = true;
        setChoiceShown (true);

        if (onTripletClicked != nullptr)
            onTripletClicked();
    };
    addAndMakeVisible (tripletButton);

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
    triplet = false;
    setChoiceShown (true);
}

void NoteLengthPicker::setTriplet (double noteBeats)
{
    tripletLength = noteBeats;
    triplet = true;
    setChoiceShown (true);
}

void NoteLengthPicker::setChoiceShown (bool shown)
{
    choiceShown = shown;

    for (auto& button : buttons)
        button->setToggleState (shown && ! triplet && juce::exactlyEqual (button->beats, length), juce::dontSendNotification);

    tripletButton.setToggleState (shown && triplet, juce::dontSendNotification);
    tripletButton.repaint();
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
    tripletButton.setBounds (staccatoButton.getRight() + groupGap, 0, buttonSize, buttonSize);
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
    // shorter, a stem up for all but the whole note, flags on the stems of an eighth, 16th or 32nd,
    // and a dot after a dotted half, quarter or eighth.
    const auto glyph = beats >= 4.0 ? Smufl::noteheadWhole : beats >= 2.0 ? Smufl::noteheadHalf : Smufl::noteheadBlack;
    const auto dotted = juce::exactlyEqual (beats, 3.0) || juce::exactlyEqual (beats, 1.5) || juce::exactlyEqual (beats, 0.75);
    const auto flagged = beats < 1.0;
    const auto headBounds = glyphs.getPath (glyph).getBounds();
    const auto hasStem = beats < 4.0;
    const auto flagGlyph = beats >= 0.5 ? Smufl::flag8thUp : beats >= 0.25 ? Smufl::flag16thUp : Smufl::flag32ndUp;
    const auto flagAnchor = beats >= 0.5 ? -0.04f : beats >= 0.25 ? -0.088f : 0.376f;     // from Bravura's metadata
    const auto stemLength = (beats < 0.25 ? 3.4f : 2.8f) * glyphStaffSpace;
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

void NoteLengthPicker::drawTriplet (juce::Graphics& g, const MusicGlyphs& glyphs, double beats, juce::Rectangle<float> bounds)
{
    // The note a little lower and smaller-looking, under a 3
    drawNote (g, glyphs, beats, bounds.withTrimmedTop (bounds.getHeight() * 0.22f).withTrimmedLeft (bounds.getWidth() * 0.12f));
    g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    g.drawText ("3", bounds.withHeight (bounds.getHeight() * 0.4f).withTrimmedRight (bounds.getWidth() * 0.45f).translated (2.0f, 1.0f),
                juce::Justification::centred, false);
}

//==============================================================================
NoteLengthPicker::TripletButton::TripletButton (const MusicGlyphs& glyphsToUse, const double& beatsToShow)
    : juce::Button ("Triplets"),
      glyphs (glyphsToUse),
      beats (beatsToShow)
{
}

void NoteLengthPicker::TripletButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    g.setColour (controls::drawSymbolButtonTile (g, *this, highlighted, down));
    drawTriplet (g, glyphs, beats, getLocalBounds().toFloat().reduced (0.5f));
}

//==============================================================================
TripletLengthPanel::TripletLengthPanel (double chosenBeats)
    : glyphs (NoteLengthPicker::glyphStaffSpace)
{
    heading.setText ("Triplet notes", juce::dontSendNotification);
    heading.setFont (juce::FontOptions (13.0f));
    heading.setBorderSize ({});
    heading.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (heading);

    for (auto [beats, name] : { std::pair { 2.0, "Half" }, { 1.0, "Quarter" }, { 0.5, "Eighth" }, { 0.25, "16th" } })
    {
        auto button = std::make_unique<LengthButton> (glyphs, beats);
        button->setTooltip (juce::String (name) + "-note triplets: three " + juce::String (name).toLowerCase()
                            + " notes in the time of two");
        button->setClickingTogglesState (false);
        button->setToggleState (juce::exactlyEqual (beats, chosenBeats), juce::dontSendNotification);
        button->setWantsKeyboardFocus (false);
        button->onClick = [this, chosen = beats]
        {
            if (onChoose != nullptr)
                onChoose (chosen);
        };
        addAndMakeVisible (*button);
        buttons.push_back (std::move (button));
    }

    constexpr int size = NoteLengthPicker::buttonSize + 6;
    setSize (12 + (int) buttons.size() * (size + 4) - 4 + 12, 12 + 20 + size + 12);
}

void TripletLengthPanel::resized()
{
    constexpr int size = NoteLengthPicker::buttonSize + 6;
    auto area = getLocalBounds().reduced (12);
    heading.setBounds (area.removeFromTop (20));

    for (auto& button : buttons)
    {
        button->setBounds (area.removeFromLeft (size).withHeight (size));
        area.removeFromLeft (4);
    }
}

TripletLengthPanel::LengthButton::LengthButton (const MusicGlyphs& glyphsToUse, double beatsToShow)
    : juce::Button ({}),
      glyphs (glyphsToUse),
      beats (beatsToShow)
{
}

void TripletLengthPanel::LengthButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    g.setColour (controls::drawSymbolButtonTile (g, *this, highlighted, down));
    NoteLengthPicker::drawTriplet (g, glyphs, beats, getLocalBounds().toFloat().reduced (3.5f));
}
