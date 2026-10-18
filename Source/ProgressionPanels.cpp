#include "ProgressionPanels.h"

#include "ChordEditor.h"

namespace
{
    constexpr int padding = 12, rowHeight = 26, gap = 6, labelWidth = 84, buttonWidth = 80;
    constexpr int panelWidth = 360, listRowHeight = 38, listRows = 5;

    void makeLabel (juce::Label& label, const juce::String& text)
    {
        label.setText (text, juce::dontSendNotification);
        label.setFont (juce::FontOptions (13.0f));
        label.setBorderSize ({});
    }

    juce::String describeChords (const SavedProgression& progression)
    {
        const auto count = (int) progression.chords.size();
        return juce::String (count) + (count == 1 ? " chord" : " chords");
    }
}

//==============================================================================
SaveProgressionPanel::SaveProgressionPanel (const SavedProgression& progression, int part)
    : numerals (progression.getNumerals())
{
    controls::makeHeading (heading, "Save Progression");
    makeLabel (summaryLabel, describeChords (progression) + " from instrument " + juce::String (part + 1) + ": " + numerals);
    summaryLabel.setColour (juce::Label::textColourId, controls::secondaryText);
    summaryLabel.setJustificationType (juce::Justification::topLeft);
    summaryLabel.setMinimumHorizontalScale (1.0f);
    makeLabel (nameLabel, "Name");

    nameEditor.setText (progression.name, false);
    nameEditor.setInputRestrictions (SavedProgression::maxNameLength);
    nameEditor.setTooltip ("What to call the progression. It starts as its chords' numerals.");
    nameEditor.onReturnKey = [this] { save(); };
    nameEditor.onEscapeKey = [this] { if (onCancel != nullptr) onCancel(); };

    saveButton.setTooltip ("Keep the progression's chords, to apply them again, in this score or another");
    cancelButton.setTooltip ("Close without saving");
    saveButton.onClick = [this] { save(); };
    cancelButton.onClick = [this] { if (onCancel != nullptr) onCancel(); };

    for (auto* component : std::initializer_list<juce::Component*> { &heading, &summaryLabel, &nameLabel, &nameEditor, &saveButton, &cancelButton })
        addAndMakeVisible (component);

    for (auto* button : { &saveButton, &cancelButton })
        button->setWantsKeyboardFocus (false);

    setSize (panelWidth, padding * 2 + 16 + gap + 34 + gap + rowHeight + gap * 2 + rowHeight);
}

void SaveProgressionPanel::parentHierarchyChanged()
{
    // Ready to type a name over the numerals, once it's on screen
    if (nameEditor.isShowing())
    {
        nameEditor.grabKeyboardFocus();
        nameEditor.selectAll();
    }
}

void SaveProgressionPanel::save()
{
    const auto name = SavedProgression::tidyName (nameEditor.getText());

    if (onSave != nullptr)
        onSave (name.isNotEmpty() ? name : numerals);
}

void SaveProgressionPanel::resized()
{
    auto bounds = getLocalBounds().reduced (padding);
    heading.setBounds (bounds.removeFromTop (16));
    bounds.removeFromTop (gap);
    summaryLabel.setBounds (bounds.removeFromTop (34));
    bounds.removeFromTop (gap);

    auto nameRow = bounds.removeFromTop (rowHeight);
    nameLabel.setBounds (nameRow.removeFromLeft (labelWidth / 2 + 8));
    nameEditor.setBounds (nameRow);
    bounds.removeFromTop (gap * 2);

    auto buttons = bounds.removeFromTop (rowHeight);
    saveButton.setBounds (buttons.removeFromRight (buttonWidth));
    buttons.removeFromRight (gap);
    cancelButton.setBounds (buttons.removeFromRight (buttonWidth));
}

//==============================================================================
ApplyProgressionPanel::ApplyProgressionPanel (const std::vector<SavedProgression>& saved, const ChordStyle& style, int part, int measure,
                                              int beatsPerMeasure, const juce::String& chosenName, std::vector<Rhythm> savedRhythms)
    : rhythms (std::move (savedRhythms))
{
    controls::makeHeading (heading, "Apply Saved Progression");
    makeLabel (targetLabel, "Into instrument " + juce::String (part + 1) + ", from measure " + juce::String (measure + 1)
                                + ", where it plays from, its chords one after another");
    targetLabel.setColour (juce::Label::textColourId, controls::secondaryText);
    targetLabel.setJustificationType (juce::Justification::topLeft);
    makeLabel (typeLabel, "Chord type");
    makeLabel (clefLabel, "Clef");
    makeLabel (susLabel, "Suspend");

    // Each turns on, in place of the other, or off again, neither on to start with.
    controls::makeSegmented ({ &susButtons[0], &susButtons[1] }, 0);
    susButtons[0].setButtonText ("sus2");
    susButtons[1].setButtonText ("sus4");
    susButtons[0].setTooltip ("Write every chord with its 2nd in place of its 3rd, e.g. C D G");
    susButtons[1].setTooltip ("Write every chord with its 4th in place of its 3rd, e.g. C F G");

    for (size_t i = 0; i < susButtons.size(); ++i)
    {
        susButtons[i].setClickingTogglesState (false);
        susButtons[i].onClick = [this, i]
        {
            const auto turnOn = ! susButtons[i].getToggleState();

            for (auto& button : susButtons)
                button.setToggleState (false, juce::dontSendNotification);

            susButtons[i].setToggleState (turnOn, juce::dontSendNotification);
        };
    }
    makeLabel (lengthLabel, "Note length");
    makeLabel (rhythmLabel, "Rhythm");
    makeLabel (measuresLabel, "Measures per chord");

    // Its tip first, so its buttons and box get it too, as its style makes them
    measuresSlider.setTooltip ("How many measures each chord goes in, one after another, at least as many as its rhythm takes");
    measuresSlider.setSliderStyle (juce::Slider::IncDecButtons);
    measuresSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 44, rowHeight);
    measuresSlider.setRange (1, maxMeasuresPerChord, 1);
    measuresSlider.setValue (1, juce::dontSendNotification);

    list.setRowHeight (listRowHeight);
    list.setColour (juce::ListBox::outlineColourId, juce::Colours::black.withAlpha (0.15f));
    list.setOutlineThickness (1);
    list.setTooltip ("The progressions saved. Double-click one to apply it.");

    typeBox.addItemList (getChordTypeNames(), 1);
    typeBox.setSelectedId ((int) style.type + 1, juce::dontSendNotification);
    typeBox.setTooltip ("How the chords are written and played, as in the chord window");

    controls::makeSegmented ({ &trebleButton, &bassButton }, 3001);
    trebleButton.setToggleState (style.staff == Staff::treble, juce::dontSendNotification);
    bassButton.setToggleState (style.staff == Staff::bass, juce::dontSendNotification);
    trebleButton.setTooltip ("Write the chords on the treble staff");
    bassButton.setTooltip ("Write the chords on the bass staff");

    // Automatic, as the chord type has them, or one of the lengths that fit in a measure
    const auto& noteLengths = getChordNoteLengths();
    lengthBox.addItem ("Automatic", 1);

    for (size_t i = 0; i < noteLengths.size(); ++i)
    {
        lengthBox.addItem (noteLengths[i].second, (int) i + 2);
        lengthBox.setItemEnabled ((int) i + 2, i == 0 || noteLengths[i].first <= beatsPerMeasure * Score::slotsPerBeat);

        if (style.noteLength == noteLengths[i].first)
            lengthBox.setSelectedId ((int) i + 2, juce::dontSendNotification);
    }

    if (lengthBox.getSelectedId() == 0)
        lengthBox.setSelectedId (1, juce::dontSendNotification);

    lengthBox.setTooltip ("How long the chords' notes are. Automatic leaves it to the chord type, as the chord window does.");

    // Or a saved rhythm, in place of the note length, the one applied last if it's still there
    rhythmBox.addItem ("No rhythm", 1);

    for (size_t i = 0; i < rhythms.size(); ++i)
        rhythmBox.addItem (rhythms[i].name, (int) i + 2);

    const auto chosenRhythm = style.rhythm.has_value()
                                ? std::find_if (rhythms.begin(), rhythms.end(), [&] (const Rhythm& r) { return r.name == style.rhythm->name; })
                                : rhythms.end();
    rhythmBox.setSelectedId (chosenRhythm != rhythms.end() ? (int) std::distance (rhythms.begin(), chosenRhythm) + 2 : 1, juce::dontSendNotification);
    rhythmBox.setTooltip ("A saved rhythm for each chord's notes to go in, in place of the note length, in as many measures as it takes");
    rhythmBox.onChange = [this] { updateLength(); };
    updateLength();

    deleteButton.setTooltip ("Delete the chosen progression from the ones saved");
    cancelButton.setTooltip ("Close without applying a progression");
    applyButton.setTooltip ("Write the chosen progression's chords in, adding measures at the end if they don't all fit");
    deleteButton.onClick = [this]
    {
        if (const auto* chosen = getChosen(); chosen != nullptr && onDelete != nullptr)
        {
            const auto name = chosen->name;
            onDelete (name);
        }
    };
    cancelButton.onClick = [this] { if (onCancel != nullptr) onCancel(); };
    applyButton.onClick = [this] { apply(); };

    for (auto* component : std::initializer_list<juce::Component*> { &heading, &targetLabel, &list, &typeLabel, &typeBox, &clefLabel, &trebleButton, &bassButton,
                                                                     &susLabel, &susButtons[0], &susButtons[1],
                                                                     &lengthLabel, &lengthBox, &rhythmLabel, &rhythmBox, &measuresLabel, &measuresSlider,
                                                                     &deleteButton, &cancelButton, &applyButton })
        addAndMakeVisible (component);

    for (auto* button : { &deleteButton, &cancelButton, &applyButton })
        button->setWantsKeyboardFocus (false);

    progressions = saved;
    list.updateContent();

    const auto chosen = std::find_if (progressions.begin(), progressions.end(), [&] (const SavedProgression& p) { return p.name == chosenName; });
    list.selectRow (chosen != progressions.end() ? (int) std::distance (progressions.begin(), chosen) : 0);
    updateButtons();

    setSize (panelWidth, padding * 2 + 16 + gap + 34 + gap + listRows * listRowHeight + 2 + gap * 2 + 6 * (rowHeight + gap) + gap + rowHeight);
}

void ApplyProgressionPanel::setProgressions (const std::vector<SavedProgression>& saved)
{
    const auto chosenName = getChosen() != nullptr ? getChosen()->name : juce::String();
    const auto chosenRow = list.getSelectedRow();
    progressions = saved;
    list.updateContent();

    // The same one, or else the one in its place, or the last, if there are any
    const auto chosen = std::find_if (progressions.begin(), progressions.end(), [&] (const SavedProgression& p) { return p.name == chosenName; });

    if (progressions.empty())
        list.deselectAllRows();
    else
        list.selectRow (chosen != progressions.end() ? (int) std::distance (progressions.begin(), chosen)
                                                     : juce::jlimit (0, (int) progressions.size() - 1, chosenRow));

    updateButtons();
    list.repaint();
}

const SavedProgression* ApplyProgressionPanel::getChosen() const
{
    const auto row = list.getSelectedRow();
    return juce::isPositiveAndBelow (row, (int) progressions.size()) ? &progressions[(size_t) row] : nullptr;
}

ChordStyle ApplyProgressionPanel::getStyle() const
{
    ChordStyle style;
    style.type = (music::ChordType) juce::jlimit (0, 4, typeBox.getSelectedId() - 1);
    style.staff = bassButton.getToggleState() ? Staff::bass : Staff::treble;

    if (const auto index = lengthBox.getSelectedId() - 2; juce::isPositiveAndBelow (index, (int) getChordNoteLengths().size()))
        style.noteLength = getChordNoteLengths()[(size_t) index].first;

    if (const auto index = rhythmBox.getSelectedId() - 2; juce::isPositiveAndBelow (index, (int) rhythms.size()))
        style.rhythm = rhythms[(size_t) index];

    return style;
}

music::Suspension ApplyProgressionPanel::getSuspension() const
{
    return susButtons[0].getToggleState() ? music::Suspension::sus2
         : susButtons[1].getToggleState() ? music::Suspension::sus4
                                          : music::Suspension::none;
}

int ApplyProgressionPanel::getMeasuresPerChord() const
{
    return juce::jlimit (1, maxMeasuresPerChord, juce::roundToInt (measuresSlider.getValue()));
}

void ApplyProgressionPanel::updateLength()
{
    // A rhythm says how long the notes are instead.
    lengthBox.setEnabled (rhythmBox.getSelectedId() <= 1);
}

void ApplyProgressionPanel::apply()
{
    // Copies, as the panel may close
    if (const auto* chosen = getChosen(); chosen != nullptr && onApply != nullptr)
    {
        const auto progression = chosen->suspended (getSuspension());
        const auto style = getStyle();
        onApply (progression, style, getMeasuresPerChord());
    }
}

void ApplyProgressionPanel::updateButtons()
{
    applyButton.setEnabled (getChosen() != nullptr);
    deleteButton.setEnabled (getChosen() != nullptr);
}

int ApplyProgressionPanel::getNumRows()
{
    return (int) progressions.size();
}

void ApplyProgressionPanel::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (! juce::isPositiveAndBelow (row, (int) progressions.size()))
        return;

    // Its name, and under it, how many chords it has, and their numerals
    const auto& progression = progressions[(size_t) row];

    if (selected)
        g.fillAll (controls::accentLight);

    const auto area = juce::Rectangle<int> (width, height).reduced (8, 3);
    g.setColour (juce::Colour (0xff1b1b1b));
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText (progression.name, area.withHeight (area.getHeight() / 2), juce::Justification::centredLeft, true);
    g.setColour (controls::secondaryText);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText (describeChords (progression) + ": " + progression.getNumerals(), area.withTrimmedTop (area.getHeight() / 2),
                juce::Justification::centredLeft, true);
}

void ApplyProgressionPanel::selectedRowsChanged (int)
{
    updateButtons();
}

void ApplyProgressionPanel::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
{
    if (juce::isPositiveAndBelow (row, (int) progressions.size()))
        apply();
}

juce::String ApplyProgressionPanel::getTooltipForRow (int row)
{
    if (! juce::isPositiveAndBelow (row, (int) progressions.size()))
        return {};

    return progressions[(size_t) row].getNumerals();
}

void ApplyProgressionPanel::resized()
{
    auto bounds = getLocalBounds().reduced (padding);
    heading.setBounds (bounds.removeFromTop (16));
    bounds.removeFromTop (gap);
    targetLabel.setBounds (bounds.removeFromTop (34));
    bounds.removeFromTop (gap);
    list.setBounds (bounds.removeFromTop (listRows * listRowHeight + 2));
    bounds.removeFromTop (gap * 2);

    auto typeRow = bounds.removeFromTop (rowHeight);
    typeLabel.setBounds (typeRow.removeFromLeft (labelWidth));
    typeBox.setBounds (typeRow);
    bounds.removeFromTop (gap);

    auto clefRow = bounds.removeFromTop (rowHeight);
    clefLabel.setBounds (clefRow.removeFromLeft (labelWidth));
    trebleButton.setBounds (clefRow.removeFromLeft (clefRow.getWidth() / 2));
    bassButton.setBounds (clefRow);
    bounds.removeFromTop (gap);

    auto susRow = bounds.removeFromTop (rowHeight);
    susLabel.setBounds (susRow.removeFromLeft (labelWidth));
    susButtons[0].setBounds (susRow.removeFromLeft (susRow.getWidth() / 2));
    susButtons[1].setBounds (susRow);
    bounds.removeFromTop (gap);

    auto lengthRow = bounds.removeFromTop (rowHeight);
    lengthLabel.setBounds (lengthRow.removeFromLeft (labelWidth));
    lengthBox.setBounds (lengthRow);
    bounds.removeFromTop (gap);

    auto rhythmRow = bounds.removeFromTop (rowHeight);
    rhythmLabel.setBounds (rhythmRow.removeFromLeft (labelWidth));
    rhythmBox.setBounds (rhythmRow);
    bounds.removeFromTop (gap);

    auto measuresRow = bounds.removeFromTop (rowHeight);
    measuresLabel.setBounds (measuresRow.removeFromLeft (130));
    measuresSlider.setBounds (measuresRow.removeFromLeft (110));
    bounds.removeFromTop (gap * 2);

    auto buttons = bounds.removeFromTop (rowHeight);
    deleteButton.setBounds (buttons.removeFromLeft (buttonWidth));
    applyButton.setBounds (buttons.removeFromRight (buttonWidth));
    buttons.removeFromRight (gap);
    cancelButton.setBounds (buttons.removeFromRight (buttonWidth));
}
