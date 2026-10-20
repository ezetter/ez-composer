#pragma once

#include "Controls.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

/** The instruments loaded lately, for every part, kept in the app's settings, the latest first. */
namespace recentInstruments
{
    constexpr int maxCount = 12;

    /** The ones kept, the latest first, leaving out any whose plugin file isn't there any more. */
    std::vector<juce::PluginDescription> get (juce::PropertiesFile&);

    /** Puts one first, or, unless it's latest, last if it isn't already among them, as for one
        reloaded when the app starts, so it's there to start with without coming before the ones
        chosen since. Only so many are kept.
    */
    void remember (juce::PropertiesFile&, const juce::PluginDescription&, bool latest = true);

    void forget (juce::PropertiesFile&, const juce::PluginDescription&);
}

/** What Load Instrument shows: the instruments loaded lately, to load one again straight away,
    with Browse… to find another plugin, Remove to take one off the list, Cancel and Load.
*/
class LoadInstrumentPanel final : public juce::Component,
                                  private juce::ListBoxModel
{
public:
    /** For a part, with the instruments loaded lately, and the name of the one it has, if any. */
    LoadInstrumentPanel (int part, const std::vector<juce::PluginDescription>& recent, const juce::String& loadedName);

    /** Shows the instruments again, e.g. once one's been removed, keeping the one chosen if it's still there. */
    void setInstruments (const std::vector<juce::PluginDescription>&);

    const juce::PluginDescription* getChosen() const;

    /** Called when Load is clicked, Return pressed, or an instrument double-clicked. */
    std::function<void (const juce::PluginDescription&)> onLoad;

    /** Called when Remove is clicked, with the chosen instrument. */
    std::function<void (const juce::PluginDescription&)> onRemove;
    std::function<void()> onBrowse, onCancel;

    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void selectedRowsChanged (int lastRowSelected) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
    void returnKeyPressed (int lastRowSelected) override;
    juce::String getTooltipForRow (int row) override;
    void parentHierarchyChanged() override;

    void load();
    void updateButtons();

    std::vector<juce::PluginDescription> instruments;
    juce::Label heading, noteLabel;
    juce::ListBox list { "Recent instruments", this };
    juce::TextButton removeButton { "Remove" }, browseButton, cancelButton { "Cancel" }, loadButton { "Load" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoadInstrumentPanel)
};
