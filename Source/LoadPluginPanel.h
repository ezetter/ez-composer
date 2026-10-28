#pragma once

#include "Controls.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

/** Instrument plugins, played by the parts, or effect plugins, in the buses */
enum class PluginKind { instrument, effect };

/** The plugins of a kind loaded lately, the instruments for every part, or the effects for every
    bus, kept in the app's settings, the latest first.
*/
namespace recentPlugins
{
    constexpr int maxCount = 12;

    /** Where they're kept in the settings: recentInstruments or recentEffects */
    juce::String getSettingsKey (PluginKind);

    /** The ones kept, the latest first, leaving out any whose plugin file isn't there any more. */
    std::vector<juce::PluginDescription> get (juce::PropertiesFile&, PluginKind);

    /** Puts one first, or, unless it's latest, last if it isn't already among them, as for one
        reloaded when the app starts, so it's there to start with without coming before the ones
        chosen since. Only so many are kept.
    */
    void remember (juce::PropertiesFile&, PluginKind, const juce::PluginDescription&, bool latest = true);

    void forget (juce::PropertiesFile&, PluginKind, const juce::PluginDescription&);
}

/** What Load Instrument and Add Effect show: the plugins of that kind loaded lately, to load one
    again straight away, with Browse… to find another plugin, Remove to take one off the list,
    Cancel and Load.
*/
class LoadPluginPanel final : public juce::Component,
                                  private juce::ListBoxModel
{
public:
    /** With a heading, e.g. "Load Instrument 2", what it's for, e.g. "It plays Pigments now.", and
        the plugins of the kind loaded lately.
    */
    LoadPluginPanel (PluginKind, const juce::String& heading, const juce::String& situation, const std::vector<juce::PluginDescription>& recent);

    /** Shows the plugins again, e.g. once one's been removed, keeping the one chosen if it's still there. */
    void setPlugins (const std::vector<juce::PluginDescription>&);

    const juce::PluginDescription* getChosen() const;

    /** Called when Load is clicked, Return pressed, or an instrument double-clicked. */
    std::function<void (const juce::PluginDescription&)> onLoad;

    /** Called when Remove is clicked, with the chosen plugin. */
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

    std::vector<juce::PluginDescription> plugins;
    juce::Label heading, noteLabel;
    juce::ListBox list { "Recent plugins", this };
    juce::TextButton removeButton { "Remove" }, browseButton, cancelButton { "Cancel" }, loadButton { "Load" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoadPluginPanel)
};
