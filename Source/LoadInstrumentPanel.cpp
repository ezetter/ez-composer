#include "LoadInstrumentPanel.h"

#include <optional>

namespace
{
    constexpr int padding = 12, rowHeight = 26, gap = 6, buttonWidth = 80;
    constexpr int panelWidth = 380, listRowHeight = 38, listRows = 6;
    const juce::String settingsKey = "recentInstruments";

    juce::String withEllipsis (const juce::String& text)
    {
        return text + juce::String (juce::CharPointer_UTF8 ("\xe2\x80\xa6"));
    }

    /** All that are kept, whether or not their files are still there */
    std::vector<juce::PluginDescription> getKept (juce::PropertiesFile& settings)
    {
        std::vector<juce::PluginDescription> kept;

        if (const auto xml = settings.getXmlValue (settingsKey))
        {
            for (const auto* child : xml->getChildIterator())
            {
                juce::PluginDescription description;

                if (description.loadFromXml (*child)
                    && std::none_of (kept.begin(), kept.end(), [&] (const juce::PluginDescription& d) { return d.isDuplicateOf (description); }))
                    kept.push_back (description);
            }
        }

        return kept;
    }

    void keep (juce::PropertiesFile& settings, const std::vector<juce::PluginDescription>& kept)
    {
        juce::XmlElement xml ("RECENTINSTRUMENTS");

        for (size_t i = 0; i < kept.size() && i < (size_t) recentInstruments::maxCount; ++i)
            xml.addChildElement (kept[i].createXml().release());

        settings.setValue (settingsKey, &xml);
        settings.saveIfNeeded();
    }

    /** Where the plugin's file is, for showing, as the home folder's ~ */
    juce::String getPlace (const juce::PluginDescription& description)
    {
        const auto& path = description.fileOrIdentifier;

        if (! juce::File::isAbsolutePath (path))
            return path;

        const auto home = juce::File::getSpecialLocation (juce::File::userHomeDirectory).getFullPathName();
        return path.startsWith (home + "/") ? "~" + path.substring (home.length()) : path;
    }
}

//==============================================================================
std::vector<juce::PluginDescription> recentInstruments::get (juce::PropertiesFile& settings)
{
    auto kept = getKept (settings);

    std::erase_if (kept, [] (const juce::PluginDescription& description)
    {
        return juce::File::isAbsolutePath (description.fileOrIdentifier) && ! juce::File (description.fileOrIdentifier).exists();
    });

    return kept;
}

void recentInstruments::remember (juce::PropertiesFile& settings, const juce::PluginDescription& description, bool latest)
{
    auto kept = getKept (settings);
    const auto isIt = [&] (const juce::PluginDescription& d) { return d.isDuplicateOf (description); };

    if (latest)
    {
        std::erase_if (kept, isIt);
        kept.insert (kept.begin(), description);
    }
    else if (std::none_of (kept.begin(), kept.end(), isIt))
    {
        // Only if there's room, so it doesn't push out one chosen since
        if ((int) kept.size() >= maxCount)
            return;

        kept.push_back (description);
    }
    else
    {
        return;
    }

    keep (settings, kept);
}

void recentInstruments::forget (juce::PropertiesFile& settings, const juce::PluginDescription& description)
{
    auto kept = getKept (settings);

    if (std::erase_if (kept, [&] (const juce::PluginDescription& d) { return d.isDuplicateOf (description); }) > 0)
        keep (settings, kept);
}

//==============================================================================
LoadInstrumentPanel::LoadInstrumentPanel (int part, const std::vector<juce::PluginDescription>& recent, const juce::String& loadedName)
    : browseButton (withEllipsis ("Browse"))
{
    controls::makeHeading (heading, "Load Instrument " + juce::String (part + 1));

    noteLabel.setFont (juce::FontOptions (13.0f));
    noteLabel.setBorderSize ({});
    noteLabel.setColour (juce::Label::textColourId, controls::secondaryText);
    noteLabel.setJustificationType (juce::Justification::topLeft);
    noteLabel.setText ((loadedName.isNotEmpty() ? "It plays " + loadedName + " now. " : juce::String ("It has no instrument now. "))
                           + (recent.empty() ? "No instruments have been loaded yet: Browse to find one."
                                             : "Choose one loaded lately, or Browse to find another."),
                       juce::dontSendNotification);

    list.setRowHeight (listRowHeight);
    list.setColour (juce::ListBox::outlineColourId, juce::Colours::black.withAlpha (0.15f));
    list.setOutlineThickness (1);
    list.setTooltip ("The instruments loaded lately, the latest first. Double-click one to load it.");

    removeButton.setTooltip ("Take the chosen instrument off the list. It stays installed.");
    browseButton.setTooltip ("Find a VST3 instrument plugin to load, in the plugin folders or anywhere else");
    cancelButton.setTooltip ("Close, keeping the instrument as it is");
    loadButton.setTooltip ("Load the chosen instrument, with its own sound to start with");

    removeButton.onClick = [this]
    {
        if (const auto* chosen = getChosen(); chosen != nullptr && onRemove != nullptr)
        {
            const auto description = *chosen;
            onRemove (description);
        }
    };
    browseButton.onClick = [this] { if (onBrowse != nullptr) onBrowse(); };
    cancelButton.onClick = [this] { if (onCancel != nullptr) onCancel(); };
    loadButton.onClick = [this] { load(); };

    for (auto* component : std::initializer_list<juce::Component*> { &heading, &noteLabel, &list, &removeButton, &browseButton,
                                                                     &cancelButton, &loadButton })
        addAndMakeVisible (component);

    for (auto* button : { &removeButton, &browseButton, &cancelButton, &loadButton })
        button->setWantsKeyboardFocus (false);

    instruments = recent;
    list.updateContent();

    if (! instruments.empty())
        list.selectRow (0);

    updateButtons();
    setSize (panelWidth, padding * 2 + 16 + gap + 34 + gap + listRows * listRowHeight + 2 + gap * 2 + rowHeight);
}

void LoadInstrumentPanel::setInstruments (const std::vector<juce::PluginDescription>& recent)
{
    const auto chosen = getChosen() != nullptr ? std::optional (*getChosen()) : std::nullopt;
    const auto chosenRow = list.getSelectedRow();
    instruments = recent;
    list.updateContent();

    // The same one, or else the one in its place, or the last, if there are any
    const auto same = std::find_if (instruments.begin(), instruments.end(),
                                    [&] (const juce::PluginDescription& d) { return chosen.has_value() && d.isDuplicateOf (*chosen); });

    if (instruments.empty())
        list.deselectAllRows();
    else
        list.selectRow (same != instruments.end() ? (int) std::distance (instruments.begin(), same)
                                                  : juce::jlimit (0, (int) instruments.size() - 1, chosenRow));

    updateButtons();
    list.repaint();
}

const juce::PluginDescription* LoadInstrumentPanel::getChosen() const
{
    const auto row = list.getSelectedRow();
    return juce::isPositiveAndBelow (row, (int) instruments.size()) ? &instruments[(size_t) row] : nullptr;
}

void LoadInstrumentPanel::load()
{
    // A copy, as the panel may close
    if (const auto* chosen = getChosen(); chosen != nullptr && onLoad != nullptr)
    {
        const auto description = *chosen;
        onLoad (description);
    }
}

void LoadInstrumentPanel::updateButtons()
{
    loadButton.setEnabled (getChosen() != nullptr);
    removeButton.setEnabled (getChosen() != nullptr);
}

bool LoadInstrumentPanel::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::returnKey)
    {
        load();
        return true;
    }

    if (key == juce::KeyPress::escapeKey)
    {
        if (onCancel != nullptr)
            onCancel();

        return true;
    }

    return false;
}

void LoadInstrumentPanel::parentHierarchyChanged()
{
    // Ready for the arrow keys and Return, once it's on screen
    if (list.isShowing())
        list.grabKeyboardFocus();
}

int LoadInstrumentPanel::getNumRows()
{
    return (int) instruments.size();
}

void LoadInstrumentPanel::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (! juce::isPositiveAndBelow (row, (int) instruments.size()))
        return;

    // Its name, and under it, who makes it and where its plugin is
    const auto& description = instruments[(size_t) row];

    if (selected)
        g.fillAll (controls::accentLight);

    const auto area = juce::Rectangle<int> (width, height).reduced (8, 3);
    g.setColour (juce::Colour (0xff1b1b1b));
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText (description.name, area.withHeight (area.getHeight() / 2), juce::Justification::centredLeft, true);
    g.setColour (controls::secondaryText);
    g.setFont (juce::FontOptions (12.0f));
    const auto maker = description.manufacturerName.trim();
    g.drawText ((maker.isNotEmpty() ? maker + juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 ")) : juce::String())
                    + juce::File (description.fileOrIdentifier).getFileName(),
                area.withTrimmedTop (area.getHeight() / 2), juce::Justification::centredLeft, true);
}

void LoadInstrumentPanel::selectedRowsChanged (int)
{
    updateButtons();
}

void LoadInstrumentPanel::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
{
    if (juce::isPositiveAndBelow (row, (int) instruments.size()))
        load();
}

void LoadInstrumentPanel::returnKeyPressed (int)
{
    load();
}

juce::String LoadInstrumentPanel::getTooltipForRow (int row)
{
    return juce::isPositiveAndBelow (row, (int) instruments.size()) ? getPlace (instruments[(size_t) row]) : juce::String();
}

void LoadInstrumentPanel::resized()
{
    auto bounds = getLocalBounds().reduced (padding);
    heading.setBounds (bounds.removeFromTop (16));
    bounds.removeFromTop (gap);
    noteLabel.setBounds (bounds.removeFromTop (34));
    bounds.removeFromTop (gap);
    list.setBounds (bounds.removeFromTop (listRows * listRowHeight + 2));
    bounds.removeFromTop (gap * 2);

    auto buttons = bounds.removeFromTop (rowHeight);
    removeButton.setBounds (buttons.removeFromLeft (buttonWidth));
    buttons.removeFromLeft (gap);
    browseButton.setBounds (buttons.removeFromLeft (buttonWidth));
    loadButton.setBounds (buttons.removeFromRight (buttonWidth));
    buttons.removeFromRight (gap);
    cancelButton.setBounds (buttons.removeFromRight (buttonWidth));
}
