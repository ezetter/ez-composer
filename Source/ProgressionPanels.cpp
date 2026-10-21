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

    juce::String withEllipsis (const juce::String& text)
    {
        return text + juce::String (juce::CharPointer_UTF8 ("\xe2\x80\xa6"));
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
namespace
{
    constexpr int searchRowLabelWidth = 92, searchColumnWidth = 124, anyId = 1;

    juce::String utf8 (const char* text) { return juce::String (juce::CharPointer_UTF8 (text)); }

    /** An optional value from a box: nothing for Any, its first item, or else the value at the
        item chosen, with the items after Any in the order the values are in.
    */
    template <typename Value>
    std::optional<Value> getChoice (const juce::ComboBox& box, const std::vector<Value>& values)
    {
        const auto index = box.getSelectedId() - anyId - 1;
        return juce::isPositiveAndBelow (index, (int) values.size()) ? std::optional (values[(size_t) index]) : std::nullopt;
    }

    template <typename Value>
    void setChoice (juce::ComboBox& box, const std::optional<Value>& value, const std::vector<Value>& values)
    {
        const auto found = value.has_value() ? std::find (values.begin(), values.end(), *value) : values.end();
        box.setSelectedId (found != values.end() ? (int) std::distance (values.begin(), found) + anyId + 1 : anyId, juce::dontSendNotification);
    }

    const std::vector<bool> qualityValues { false, true };
    const std::vector<int> degreeValues { 0, 1, 2, 3, 4, 5, 6 };
    const std::vector<int> accidentalValues { 0, -1, 1 };
    const std::vector<music::AddedNote> addedNoteValues { music::AddedNote::none, music::AddedNote::dominant7th, music::AddedNote::major7th,
                                                          music::AddedNote::minor7th, music::AddedNote::dominant9th, music::AddedNote::major9th,
                                                          music::AddedNote::minor9th };
    const std::vector<music::Suspension> suspensionValues { music::Suspension::none, music::Suspension::sus2, music::Suspension::sus4 };
    const std::vector<int> alterationValues { 1, -1, 0 };
}

SearchProgressionsPanel::SearchProgressionsPanel (const ProgressionSearch& start, int maxChords,
                                                  std::function<juce::String (const ProgressionSearch&)> describe)
    : describeFound (std::move (describe))
{
    controls::makeHeading (heading, "Search Progressions");
    makeLabel (nameLabel, "Name starts");
    makeLabel (countLabel, "Chords");
    makeLabel (foundLabel, {});
    foundLabel.setColour (juce::Label::textColourId, controls::secondaryText);

    nameEditor.setInputRestrictions (SavedProgression::maxNameLength);
    nameEditor.setTextToShowWhenEmpty ("Any name", controls::secondaryText);
    nameEditor.setTooltip ("The first letters of the progression's name, in capitals or not. Leave it empty for any name.");
    nameEditor.onTextChange = [this] { changed(); };
    nameEditor.onReturnKey = [this] { if (onDone != nullptr) onDone(); };
    nameEditor.onEscapeKey = [this] { if (onDone != nullptr) onDone(); };

    countBox.addItem ("Any", anyId);

    for (int count = 1; count <= juce::jmax (ProgressionSearch::numChordsSearched, maxChords); ++count)
        countBox.addItem (juce::String (count), count + anyId);

    countBox.setTooltip ("How many chords the progression has. Any leaves it out.");
    countBox.onChange = [this] { changed(); };

    const auto flat = utf8 ("\xe2\x99\xad"), sharp = utf8 ("\xe2\x99\xaf");
    const std::array<std::pair<const char*, juce::StringArray>, numRows> rows {{
        { "Major/minor", { "Major", "Minor" } },
        { "Number", { "I", "II", "III", "IV", "V", "VI", "VII" } },
        { "Flat/sharp", { "Neither", flat + " Flat", sharp + " Sharp" } },
        { "Added note", { "None" } },
        { "Sus", { "Not sus", "sus2", "sus4" } },
        { "Augmented", { "Augmented +", utf8 ("Diminished \xc2\xb0"), "Neither" } },
    }};
    const std::array<const char*, numRows> tips {
        "Whether the chord is major or minor",
        "The chord's number: the scale step it's on, from I for the key's own chord to VII",
        "Whether the chord's root is lowered a half step, flat, or raised, sharp, or neither",
        "The 7th or 9th added to the chord, or none",
        "Whether the chord is suspended, with its 2nd or 4th in place of its 3rd",
        "Whether the chord is augmented, a major chord with its 5th raised, or diminished, a minor one with its 5th lowered",
    };

    for (size_t row = 0; row < (size_t) numRows; ++row)
    {
        makeLabel (rowLabels[row], rows[row].first);
        addAndMakeVisible (rowLabels[row]);

        for (size_t column = 0; column < boxes[row].size(); ++column)
        {
            auto& box = boxes[row][column];
            box.addItem ("Any", anyId);
            box.addItemList (rows[row].second, anyId + 1);

            if (row == (size_t) addedNote)
                for (size_t i = 1; i < addedNoteValues.size(); ++i)
                    box.addItem (music::getInfo (addedNoteValues[i]).name, anyId + 1 + (int) i);

            box.setTooltip (juce::String (tips[row]) + " (chord " + juce::String ((int) column + 1) + "). Any leaves it out.");
            box.onChange = [this] { changed(); };
            addAndMakeVisible (box);
        }
    }

    for (size_t column = 0; column < columnLabels.size(); ++column)
    {
        makeLabel (columnLabels[column], "Chord " + juce::String ((int) column + 1));
        columnLabels[column].setFont (juce::FontOptions (13.0f, juce::Font::bold));
        addAndMakeVisible (columnLabels[column]);
    }

    clearButton.setTooltip ("Set everything back to Any, to show all the progressions");
    doneButton.setTooltip ("Close, with the list showing the progressions found");
    clearButton.onClick = [this]
    {
        setSearch ({});
        changed();
    };
    doneButton.onClick = [this] { if (onDone != nullptr) onDone(); };

    for (auto* component : std::initializer_list<juce::Component*> { &heading, &nameLabel, &nameEditor, &countLabel, &countBox,
                                                                     &foundLabel, &clearButton, &doneButton })
        addAndMakeVisible (component);

    for (auto* button : { &clearButton, &doneButton })
        button->setWantsKeyboardFocus (false);

    setSearch (start);
    foundLabel.setText (describeFound != nullptr ? describeFound (getSearch()) : juce::String(), juce::dontSendNotification);
    setSize (padding * 2 + searchRowLabelWidth + ProgressionSearch::numChordsSearched * (searchColumnWidth + gap) - gap,
             padding * 2 + 16 + gap + rowHeight + gap * 2 + 18 + gap + numRows * (rowHeight + gap) + gap + rowHeight);
}

ProgressionSearch SearchProgressionsPanel::getSearch() const
{
    ProgressionSearch search;
    search.nameStart = nameEditor.getText().trim();

    if (countBox.getSelectedId() > anyId)
        search.numChords = countBox.getSelectedId() - anyId;

    for (size_t i = 0; i < search.chords.size(); ++i)
    {
        auto& chord = search.chords[i];
        chord.minor = getChoice (boxes[quality][i], qualityValues);
        chord.degree = getChoice (boxes[number][i], degreeValues);
        chord.accidental = getChoice (boxes[accidental][i], accidentalValues);
        chord.addedNote = getChoice (boxes[addedNote][i], addedNoteValues);
        chord.suspension = getChoice (boxes[suspension][i], suspensionValues);
        chord.alteration = getChoice (boxes[alteration][i], alterationValues);
    }

    return search;
}

void SearchProgressionsPanel::setSearch (const ProgressionSearch& search)
{
    nameEditor.setText (search.nameStart, false);
    countBox.setSelectedId (search.numChords.has_value() && countBox.indexOfItemId (*search.numChords + anyId) >= 0 ? *search.numChords + anyId : anyId,
                            juce::dontSendNotification);

    for (size_t i = 0; i < search.chords.size(); ++i)
    {
        const auto& chord = search.chords[i];
        setChoice (boxes[quality][i], chord.minor, qualityValues);
        setChoice (boxes[number][i], chord.degree, degreeValues);
        setChoice (boxes[accidental][i], chord.accidental, accidentalValues);
        setChoice (boxes[addedNote][i], chord.addedNote, addedNoteValues);
        setChoice (boxes[suspension][i], chord.suspension, suspensionValues);
        setChoice (boxes[alteration][i], chord.alteration, alterationValues);
    }
}

void SearchProgressionsPanel::changed()
{
    const auto search = getSearch();
    foundLabel.setText (describeFound != nullptr ? describeFound (search) : juce::String(), juce::dontSendNotification);

    if (onChange != nullptr)
        onChange (search);
}

void SearchProgressionsPanel::parentHierarchyChanged()
{
    // Ready to type the name, once it's on screen
    if (nameEditor.isShowing())
        nameEditor.grabKeyboardFocus();
}

void SearchProgressionsPanel::resized()
{
    auto bounds = getLocalBounds().reduced (padding);
    heading.setBounds (bounds.removeFromTop (16));
    bounds.removeFromTop (gap);

    auto nameRow = bounds.removeFromTop (rowHeight);
    nameLabel.setBounds (nameRow.removeFromLeft (searchRowLabelWidth));
    nameEditor.setBounds (nameRow.removeFromLeft (2 * searchColumnWidth + gap));
    nameRow.removeFromLeft (gap * 3);
    countLabel.setBounds (nameRow.removeFromLeft (56));
    countBox.setBounds (nameRow.removeFromLeft (90));
    bounds.removeFromTop (gap * 2);

    auto headings = bounds.removeFromTop (18);
    headings.removeFromLeft (searchRowLabelWidth);

    for (auto& label : columnLabels)
    {
        label.setBounds (headings.removeFromLeft (searchColumnWidth));
        headings.removeFromLeft (gap);
    }

    bounds.removeFromTop (gap);

    for (size_t row = 0; row < (size_t) numRows; ++row)
    {
        auto line = bounds.removeFromTop (rowHeight);
        rowLabels[row].setBounds (line.removeFromLeft (searchRowLabelWidth));

        for (auto& box : boxes[row])
        {
            box.setBounds (line.removeFromLeft (searchColumnWidth));
            line.removeFromLeft (gap);
        }

        bounds.removeFromTop (gap);
    }

    bounds.removeFromTop (gap);
    auto buttons = bounds.removeFromTop (rowHeight);
    doneButton.setBounds (buttons.removeFromRight (buttonWidth));
    buttons.removeFromRight (gap);
    clearButton.setBounds (buttons.removeFromRight (buttonWidth));
    buttons.removeFromRight (gap);
    foundLabel.setBounds (buttons);
}

//==============================================================================
ApplyProgressionPanel::ApplyProgressionPanel (const std::vector<SavedProgression>& saved, const ChordStyle& style, int part, int measure,
                                              int beatsPerMeasure, const juce::String& chosenName, std::vector<Rhythm> savedRhythms)
    : rhythms (std::move (savedRhythms)),
      searchButton (withEllipsis ("Search"))
{
    controls::makeHeading (heading, "Apply Saved Progression");
    makeLabel (targetLabel, "Into instrument " + juce::String (part + 1) + ", from measure " + juce::String (measure + 1)
                                + ", where it plays from, its chords one after another");
    targetLabel.setColour (juce::Label::textColourId, controls::secondaryText);
    targetLabel.setJustificationType (juce::Justification::topLeft);
    makeLabel (typeLabel, "Chord type");
    makeLabel (clefLabel, "Clef");
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
    typeBox.onChange = [this] { updateLength(); };

    controls::makeSegmented ({ &trebleButton, &bassButton }, 3001);
    trebleButton.setToggleState (style.staff == Staff::treble, juce::dontSendNotification);
    bassButton.setToggleState (style.staff == Staff::bass, juce::dontSendNotification);
    trebleButton.setTooltip ("Write the chords on the treble staff");
    bassButton.setTooltip ("Write the chords on the bass staff");

    // Automatic, as the chord type has them, or one of the lengths that fit in a measure
    const auto& noteLengths = getChordNoteLengths();
    lengthBox.addItem ("Automatic", 1);
    lengthBox.addItem ("Two measures", twoMeasuresItemId);

    for (size_t i = 0; i < noteLengths.size(); ++i)
    {
        lengthBox.addItem (noteLengths[i].second, (int) i + 2);
        lengthBox.setItemEnabled ((int) i + 2, i == 0 || noteLengths[i].first <= beatsPerMeasure * Score::slotsPerBeat);

        if (style.noteLength == noteLengths[i].first)
            lengthBox.setSelectedId ((int) i + 2, juce::dontSendNotification);
    }

    if (style.measures > 1)
        lengthBox.setSelectedId (twoMeasuresItemId, juce::dontSendNotification);

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

    makeLabel (searchLabel, {});
    searchLabel.setColour (juce::Label::textColourId, controls::secondaryText);
    searchLabel.setMinimumHorizontalScale (1.0f);
    searchButton.setTooltip ("Look for progressions by name, number of chords, and what their first four chords are like");
    clearSearchButton.setTooltip ("Show all the progressions again");
    searchButton.onClick = [this] { showSearch(); };
    clearSearchButton.onClick = [this] { setSearch ({}); };

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

    for (auto* component : std::initializer_list<juce::Component*> { &heading, &targetLabel, &list, &searchButton, &searchLabel, &clearSearchButton, &typeLabel, &typeBox, &clefLabel, &trebleButton, &bassButton,
                                                                     &lengthLabel, &lengthBox, &rhythmLabel, &rhythmBox, &measuresLabel, &measuresSlider,
                                                                     &deleteButton, &cancelButton, &applyButton })
        addAndMakeVisible (component);

    for (auto* button : { &searchButton, &clearSearchButton, &deleteButton, &cancelButton, &applyButton })
        button->setWantsKeyboardFocus (false);

    allProgressions = saved;
    showProgressions (saved, chosenName, 0);

    setSize (panelWidth, padding * 2 + 16 + gap + 34 + gap + listRows * listRowHeight + 2 + gap + rowHeight + gap * 2
                             + 5 * (rowHeight + gap) + gap + rowHeight);
}

void ApplyProgressionPanel::setProgressions (const std::vector<SavedProgression>& saved)
{
    allProgressions = saved;
    showProgressions (search.filter (saved), getChosen() != nullptr ? getChosen()->name : juce::String(), list.getSelectedRow());
}

void ApplyProgressionPanel::setSearch (const ProgressionSearch& newSearch)
{
    search = newSearch;
    showProgressions (search.filter (allProgressions), getChosen() != nullptr ? getChosen()->name : juce::String(), 0);
}

void ApplyProgressionPanel::showProgressions (const std::vector<SavedProgression>& shown, const juce::String& chosenName, int chosenRow)
{
    progressions = shown;
    list.updateContent();

    // The same one, or else the one in its place, or the last, if there are any
    const auto chosen = std::find_if (progressions.begin(), progressions.end(), [&] (const SavedProgression& p) { return p.name == chosenName; });

    if (progressions.empty())
        list.deselectAllRows();
    else
        list.selectRow (chosen != progressions.end() ? (int) std::distance (progressions.begin(), chosen)
                                                     : juce::jlimit (0, (int) progressions.size() - 1, chosenRow));

    // What the search finds, or that it's showing them all
    const auto total = (int) allProgressions.size();
    const auto description = search.describe();

    if (search.isEmpty())
        searchLabel.setText ("All " + juce::String (total) + (total == 1 ? " progression" : " progressions"), juce::dontSendNotification);
    else
        searchLabel.setText (juce::String ((int) progressions.size()) + " of " + juce::String (total) + " found: " + description, juce::dontSendNotification);

    searchLabel.setTooltip (search.isEmpty() ? juce::String() : "Searching for " + description);
    searchLabel.setColour (juce::Label::textColourId, ! search.isEmpty() ? controls::accent : controls::secondaryText);
    clearSearchButton.setVisible (! search.isEmpty());
    updateButtons();
    list.repaint();
    resized();
}

void ApplyProgressionPanel::showSearch()
{
    auto maxChords = 0;

    for (const auto& progression : allProgressions)
        maxChords = juce::jmax (maxChords, (int) progression.chords.size());

    auto panel = std::make_unique<SearchProgressionsPanel> (search, maxChords, [safeThis = juce::Component::SafePointer (this)] (const ProgressionSearch& s)
    {
        if (safeThis == nullptr)
            return juce::String();

        const auto& all = safeThis->allProgressions;
        return juce::String ((int) s.filter (all).size()) + " of " + juce::String ((int) all.size()) + " found";
    });

    auto* panelPointer = panel.get();
    panel->onChange = [safeThis = juce::Component::SafePointer (this)] (const ProgressionSearch& s)
    {
        if (safeThis != nullptr)
            safeThis->setSearch (s);
    };
    panel->onDone = [panelPointer]
    {
        if (auto* box = panelPointer->findParentComponentOfClass<juce::CallOutBox>())
            box->dismiss();
    };

    auto* parent = getTopLevelComponent();
    juce::CallOutBox::launchAsynchronously (std::move (panel), parent->getLocalArea (&searchButton, searchButton.getLocalBounds()), parent);
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

    if (lengthBox.getSelectedId() == twoMeasuresItemId)
        style.measures = 2;
    else if (const auto index = lengthBox.getSelectedId() - 2; juce::isPositiveAndBelow (index, (int) getChordNoteLengths().size()))
        style.noteLength = getChordNoteLengths()[(size_t) index].first;

    if (const auto index = rhythmBox.getSelectedId() - 2; juce::isPositiveAndBelow (index, (int) rhythms.size()))
        style.rhythm = rhythms[(size_t) index];

    return style;
}

int ApplyProgressionPanel::getMeasuresPerChord() const
{
    return juce::jlimit (1, maxMeasuresPerChord, juce::roundToInt (measuresSlider.getValue()));
}

void ApplyProgressionPanel::updateLength()
{
    // A rhythm says how long the notes are instead.
    lengthBox.setEnabled (rhythmBox.getSelectedId() <= 1);

    // Two measures only for a block or rolled chord, which is held over them
    const auto melodic = music::isMelodic ((music::ChordType) juce::jlimit (0, 4, typeBox.getSelectedId() - 1));
    lengthBox.setItemEnabled (twoMeasuresItemId, ! melodic);

    if (melodic && lengthBox.getSelectedId() == twoMeasuresItemId)
        lengthBox.setSelectedId (1, juce::dontSendNotification);
}

void ApplyProgressionPanel::apply()
{
    // Copies, as the panel may close
    if (const auto* chosen = getChosen(); chosen != nullptr && onApply != nullptr)
    {
        const auto progression = *chosen;
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
    bounds.removeFromTop (gap);

    auto searchRow = bounds.removeFromTop (rowHeight);
    searchButton.setBounds (searchRow.removeFromLeft (buttonWidth));
    searchRow.removeFromLeft (gap * 2);

    if (clearSearchButton.isVisible())
    {
        clearSearchButton.setBounds (searchRow.removeFromRight (60));
        searchRow.removeFromRight (gap);
    }

    searchLabel.setBounds (searchRow);
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
