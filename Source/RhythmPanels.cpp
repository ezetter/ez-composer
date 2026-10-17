#include "RhythmPanels.h"

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

    void makeNote (juce::Label& label)
    {
        label.setColour (juce::Label::textColourId, controls::secondaryText);
        label.setJustificationType (juce::Justification::topLeft);
    }
}

//==============================================================================
SaveRhythmPanel::SaveRhythmPanel (int part, int firstMeasure, const std::vector<int>& lengthChoices,
                                  std::function<SavedRhythm (int)> rhythmFor, const juce::String& name)
    : getRhythmFor (std::move (rhythmFor))
{
    controls::makeHeading (heading, "Save Rhythm");
    makeLabel (fromLabel, "From measure " + juce::String (firstMeasure + 1) + " of instrument " + juce::String (part + 1) + ", where it plays from");
    makeNote (fromLabel);
    makeLabel (measuresLabel, "Measures");
    makeLabel (summaryLabel, {});
    makeNote (summaryLabel);
    makeLabel (nameLabel, "Name");

    for (auto count : lengthChoices)
        measuresBox.addItem (juce::String (count), count);

    measuresBox.setSelectedItemIndex (0, juce::dontSendNotification);
    measuresBox.setTooltip ("How many measures to save the rhythm of: no further than the page goes, or a tie would be cut");
    measuresBox.onChange = [this] { update(); };

    nameEditor.setText (name, false);
    nameEditor.setInputRestrictions (SavedRhythm::maxNameLength);
    nameEditor.setTooltip ("What to call the rhythm");
    nameEditor.onReturnKey = [this] { save(); };
    nameEditor.onEscapeKey = [this] { if (onCancel != nullptr) onCancel(); };
    nameEditor.onTextChange = [this] { update(); };

    saveButton.setTooltip ("Keep the rhythm, to apply it again, in this score or another");
    cancelButton.setTooltip ("Close without saving");
    saveButton.onClick = [this] { save(); };
    cancelButton.onClick = [this] { if (onCancel != nullptr) onCancel(); };

    for (auto* component : std::initializer_list<juce::Component*> { &heading, &fromLabel, &measuresLabel, &measuresBox, &summaryLabel,
                                                                     &nameLabel, &nameEditor, &saveButton, &cancelButton })
        addAndMakeVisible (component);

    for (auto* button : { &saveButton, &cancelButton })
        button->setWantsKeyboardFocus (false);

    update();
    setSize (panelWidth, padding * 2 + 16 + gap + 18 + gap + rowHeight + gap + 34 + gap + rowHeight + gap * 2 + rowHeight);
}

int SaveRhythmPanel::getNumMeasures() const
{
    return measuresBox.getSelectedId();
}

void SaveRhythmPanel::update()
{
    // What the measures chosen have in them, or that there's nothing to save
    const auto count = getNumMeasures();
    summaryLabel.setText (count > 0 ? getRhythmFor (count).describe() : juce::String ("There are no notes to save, from here to the end of the page."),
                          juce::dontSendNotification);
    measuresBox.setEnabled (count > 0);
    saveButton.setEnabled (count > 0 && tidySavedName (nameEditor.getText(), SavedRhythm::maxNameLength).isNotEmpty());
}

void SaveRhythmPanel::parentHierarchyChanged()
{
    // Ready to type a name over the one it starts with, once it's on screen
    if (nameEditor.isShowing())
    {
        nameEditor.grabKeyboardFocus();
        nameEditor.selectAll();
    }
}

void SaveRhythmPanel::save()
{
    const auto name = tidySavedName (nameEditor.getText(), SavedRhythm::maxNameLength);

    if (name.isNotEmpty() && getNumMeasures() > 0 && onSave != nullptr)
        onSave (name, getNumMeasures());
}

void SaveRhythmPanel::resized()
{
    auto bounds = getLocalBounds().reduced (padding);
    heading.setBounds (bounds.removeFromTop (16));
    bounds.removeFromTop (gap);
    fromLabel.setBounds (bounds.removeFromTop (18));
    bounds.removeFromTop (gap);

    auto measuresRow = bounds.removeFromTop (rowHeight);
    measuresLabel.setBounds (measuresRow.removeFromLeft (labelWidth));
    measuresBox.setBounds (measuresRow.removeFromLeft (90));
    bounds.removeFromTop (gap);
    summaryLabel.setBounds (bounds.removeFromTop (34));
    bounds.removeFromTop (gap);

    auto nameRow = bounds.removeFromTop (rowHeight);
    nameLabel.setBounds (nameRow.removeFromLeft (labelWidth));
    nameEditor.setBounds (nameRow);
    bounds.removeFromTop (gap * 2);

    auto buttons = bounds.removeFromTop (rowHeight);
    saveButton.setBounds (buttons.removeFromRight (buttonWidth));
    buttons.removeFromRight (gap);
    cancelButton.setBounds (buttons.removeFromRight (buttonWidth));
}

//==============================================================================
ApplyRhythmPanel::ApplyRhythmPanel (const std::vector<SavedRhythm>& saved, int times, const juce::String& chosenName)
{
    controls::makeHeading (heading, "Apply Saved Rhythm");
    makeLabel (howLabel, "Then click on a staff: the notes go from there, on the line or space clicked, and an octave away on the other staff");
    makeNote (howLabel);
    makeLabel (timesLabel, "Times");

    list.setRowHeight (listRowHeight);
    list.setColour (juce::ListBox::outlineColourId, juce::Colours::black.withAlpha (0.15f));
    list.setOutlineThickness (1);
    list.setTooltip ("The rhythms saved. Double-click one to place it.");

    // Its tip first, so its buttons and box get it too, as its style makes them
    timesSlider.setTooltip ("How many times over to write the rhythm, one after another, from " + juce::String (1) + " to " + juce::String (maxTimes));
    timesSlider.setSliderStyle (juce::Slider::IncDecButtons);
    timesSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 44, rowHeight);
    timesSlider.setRange (1, maxTimes, 1);
    timesSlider.setValue (juce::jlimit (1, maxTimes, times), juce::dontSendNotification);

    deleteButton.setTooltip ("Delete the chosen rhythm from the ones saved");
    cancelButton.setTooltip ("Close without placing a rhythm");
    placeButton.setTooltip ("Then click on a staff, where the rhythm's notes are to start, on the line or space they're to be");
    deleteButton.onClick = [this]
    {
        if (const auto* chosen = getChosen(); chosen != nullptr && onDelete != nullptr)
        {
            const auto name = chosen->name;
            onDelete (name);
        }
    };
    cancelButton.onClick = [this] { if (onCancel != nullptr) onCancel(); };
    placeButton.onClick = [this] { place(); };

    for (auto* component : std::initializer_list<juce::Component*> { &heading, &howLabel, &list, &timesLabel, &timesSlider,
                                                                     &deleteButton, &cancelButton, &placeButton })
        addAndMakeVisible (component);

    for (auto* button : { &deleteButton, &cancelButton, &placeButton })
        button->setWantsKeyboardFocus (false);

    rhythms = saved;
    list.updateContent();

    const auto chosen = std::find_if (rhythms.begin(), rhythms.end(), [&] (const SavedRhythm& r) { return r.name == chosenName; });
    list.selectRow (chosen != rhythms.end() ? (int) std::distance (rhythms.begin(), chosen) : 0);
    updateButtons();

    setSize (panelWidth, padding * 2 + 16 + gap + 34 + gap + listRows * listRowHeight + 2 + gap * 2 + rowHeight + gap * 2 + rowHeight);
}

void ApplyRhythmPanel::setRhythms (const std::vector<SavedRhythm>& saved)
{
    const auto chosenName = getChosen() != nullptr ? getChosen()->name : juce::String();
    const auto chosenRow = list.getSelectedRow();
    rhythms = saved;
    list.updateContent();

    const auto chosen = std::find_if (rhythms.begin(), rhythms.end(), [&] (const SavedRhythm& r) { return r.name == chosenName; });
    list.selectRow (chosen != rhythms.end() ? (int) std::distance (rhythms.begin(), chosen) : juce::jmin (chosenRow, (int) rhythms.size() - 1));
    updateButtons();
    list.repaint();
}

const SavedRhythm* ApplyRhythmPanel::getChosen() const
{
    const auto row = list.getSelectedRow();
    return juce::isPositiveAndBelow (row, (int) rhythms.size()) ? &rhythms[(size_t) row] : nullptr;
}

int ApplyRhythmPanel::getTimes() const
{
    return juce::jlimit (1, maxTimes, juce::roundToInt (timesSlider.getValue()));
}

void ApplyRhythmPanel::place()
{
    // Copies, as the panel may close
    if (const auto* chosen = getChosen(); chosen != nullptr && onPlace != nullptr)
    {
        const auto rhythm = *chosen;
        onPlace (rhythm, getTimes());
    }
}

void ApplyRhythmPanel::updateButtons()
{
    placeButton.setEnabled (getChosen() != nullptr);
    deleteButton.setEnabled (getChosen() != nullptr);
}

int ApplyRhythmPanel::getNumRows()
{
    return (int) rhythms.size();
}

void ApplyRhythmPanel::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (! juce::isPositiveAndBelow (row, (int) rhythms.size()))
        return;

    // Its name, and under it, what it has in it
    const auto& rhythm = rhythms[(size_t) row];

    if (selected)
        g.fillAll (controls::accentLight);

    const auto area = juce::Rectangle<int> (width, height).reduced (8, 3);
    g.setColour (juce::Colour (0xff1b1b1b));
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText (rhythm.name, area.withHeight (area.getHeight() / 2), juce::Justification::centredLeft, true);
    g.setColour (controls::secondaryText);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText (rhythm.describe(), area.withTrimmedTop (area.getHeight() / 2), juce::Justification::centredLeft, true);
}

void ApplyRhythmPanel::selectedRowsChanged (int)
{
    updateButtons();
}

void ApplyRhythmPanel::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
{
    if (juce::isPositiveAndBelow (row, (int) rhythms.size()))
        place();
}

juce::String ApplyRhythmPanel::getTooltipForRow (int row)
{
    return juce::isPositiveAndBelow (row, (int) rhythms.size()) ? rhythms[(size_t) row].describe() : juce::String();
}

void ApplyRhythmPanel::resized()
{
    auto bounds = getLocalBounds().reduced (padding);
    heading.setBounds (bounds.removeFromTop (16));
    bounds.removeFromTop (gap);
    howLabel.setBounds (bounds.removeFromTop (34));
    bounds.removeFromTop (gap);
    list.setBounds (bounds.removeFromTop (listRows * listRowHeight + 2));
    bounds.removeFromTop (gap * 2);

    auto timesRow = bounds.removeFromTop (rowHeight);
    timesLabel.setBounds (timesRow.removeFromLeft (labelWidth));
    timesSlider.setBounds (timesRow.removeFromLeft (110));
    bounds.removeFromTop (gap * 2);

    auto buttons = bounds.removeFromTop (rowHeight);
    deleteButton.setBounds (buttons.removeFromLeft (buttonWidth));
    placeButton.setBounds (buttons.removeFromRight (buttonWidth));
    buttons.removeFromRight (gap);
    cancelButton.setBounds (buttons.removeFromRight (buttonWidth));
}
