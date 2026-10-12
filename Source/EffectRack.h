#pragma once

#include "InstrumentHost.h"
#include "PluginWindow.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <functional>
#include <map>
#include <memory>

/** Loads effect plugins into the instrument host's buses, opens their editors, and saves the
    buses, their effects and where everything goes, as JSON, to keep in the settings and with the
    score, and loads them back.
*/
class EffectRack final
{
public:
    explicit EffectRack (InstrumentHost&);
    ~EffectRack();

    InstrumentHost& getHost() noexcept { return host; }

    /** Called when the buses, their effects or where things go change: by the user, or because
        they've been loaded from the settings or a score.
    */
    std::function<void (bool byUser)> onChanged;

    //==============================================================================
    /** Adds a bus, going to the main output, and returns its id. */
    int addBus();
    void removeBus (int busId);

    /** Lets the user choose a VST3 effect to add to the end of a bus's chain, from a file, and if
        the file has several, from a menu by a component.
    */
    void chooseEffect (int busId, juce::Component& besideThis);

    /** Adds an effect to the end of a bus's chain, with the settings given. Returns an error, or
        nothing if it loaded.
    */
    juce::String addEffect (int busId, const juce::PluginDescription&, const juce::MemoryBlock& state = {}, bool bypassed = false);

    void removeEffect (int busId, int index);
    void moveEffect (int busId, int from, int to);
    void setEffectBypassed (int busId, int index, bool);
    void setBusOutput (int busId, int output);
    void setPartOutput (int part, int output);

    /** Opens an effect's editor, or brings it to the front if it's open. */
    void showEditor (int busId, int index);

    //==============================================================================
    /** The buses, their effects and settings, and where each bus and part goes, by their places. */
    juce::var toJSON() const;

    /** Replaces the buses with saved ones. Returns the names of any effects that couldn't be
        loaded, which are left out.
    */
    juce::StringArray loadJSON (const juce::var&);

    /** Whether an effect's settings have changed since this was last asked, e.g. in its editor. */
    bool checkForSoundChanges();

private:
    void closeEditor (juce::AudioPluginInstance*);
    void changed (bool byUser);

    InstrumentHost& host;
    juce::AudioPluginFormatManager formatManager;
    std::unique_ptr<juce::FileChooser> fileChooser;
    juce::File pluginFolder { "/Library/Audio/Plug-Ins/VST3" };
    std::map<juce::AudioPluginInstance*, std::unique_ptr<PluginWindow>> editors;
    std::map<juce::AudioPluginInstance*, juce::MemoryBlock> lastStates;     // for noticing changes made in the editors

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectRack)
};
