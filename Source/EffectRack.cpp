#include "EffectRack.h"

#include "LoadPluginPanel.h"

namespace
{
    juce::MemoryBlock getState (juce::AudioPluginInstance& plugin)
    {
        juce::MemoryBlock state;
        plugin.getStateInformation (state);
        return state;
    }
}

EffectRack::EffectRack (InstrumentHost& hostToUse, juce::PropertiesFile& settingsToUse)
    : host (hostToUse),
      settings (settingsToUse)
{
    juce::addDefaultFormatsToManager (formatManager);
}

EffectRack::~EffectRack()
{
    // The editors go before their effects do.
    editors.clear();
}

void EffectRack::changed (bool byUser)
{
    if (onChanged != nullptr)
        onChanged (byUser);
}

//==============================================================================
int EffectRack::addBus()
{
    const auto id = host.addBus();
    changed (true);
    return id;
}

void EffectRack::removeBus (int busId)
{
    for (int i = 0; i < host.getNumEffects (busId); ++i)
        closeEditor (host.getEffect (busId, i));

    host.removeBus (busId);
    changed (true);
}

void EffectRack::chooseEffect (int busId, juce::Component& besideThis, const juce::String& busName)
{
    // The ones loaded lately, to add again, or Browse for another
    auto panel = std::make_unique<LoadPluginPanel> (PluginKind::effect, "Add Effect to " + busName,
                                                    "It goes at the end of " + busName + "'s chain.",
                                                    recentPlugins::get (settings, PluginKind::effect));
    auto* panelPointer = panel.get();
    const auto close = [panelPointer]
    {
        if (auto* box = panelPointer->findParentComponentOfClass<juce::CallOutBox>())
            box->dismiss();
    };

    panel->onCancel = close;
    panel->onRemove = [this, panelPointer] (const juce::PluginDescription& description)
    {
        recentPlugins::forget (settings, PluginKind::effect, description);
        panelPointer->setPlugins (recentPlugins::get (settings, PluginKind::effect));
    };
    panel->onBrowse = [this, close, busId, safeButton = juce::Component::SafePointer (&besideThis)]
    {
        close();

        if (safeButton != nullptr)
            browseForEffect (busId, *safeButton);
    };
    panel->onLoad = [this, close, busId] (const juce::PluginDescription& description)
    {
        close();

        if (const auto error = addEffect (busId, description); error.isNotEmpty())
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't load " + description.name, error);
    };

    auto* parent = besideThis.getTopLevelComponent();
    juce::CallOutBox::launchAsynchronously (std::move (panel), parent->getLocalArea (&besideThis, besideThis.getLocalBounds()), parent);
}

void EffectRack::browseForEffect (int busId, juce::Component& besideThis)
{
    fileChooser = std::make_unique<juce::FileChooser> ("Choose a VST3 effect", pluginFolder, "*.vst3");

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this, busId, safeButton = juce::Component::SafePointer (&besideThis)] (const juce::FileChooser& chooser)
                              {
                                  const auto file = chooser.getResult();

                                  if (file == juce::File())
                                      return;

                                  pluginFolder = file.getParentDirectory();

                                  juce::OwnedArray<juce::PluginDescription> descriptions;
                                  juce::MouseCursor::showWaitCursor();

                                  for (auto* format : formatManager.getFormats())
                                      if (format->fileMightContainThisPluginType (file.getFullPathName()))
                                          format->findAllTypesForFile (descriptions, file.getFullPathName());

                                  juce::MouseCursor::hideWaitCursor();

                                  juce::Array<juce::PluginDescription> effects;

                                  for (const auto* description : descriptions)
                                      if (! description->isInstrument)
                                          effects.add (*description);

                                  if (effects.isEmpty())
                                  {
                                      juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't load an effect",
                                                                              file.getFileNameWithoutExtension() + " isn't a VST3 effect.");
                                      return;
                                  }

                                  const auto load = [this, busId] (const juce::PluginDescription& description)
                                  {
                                      if (const auto error = addEffect (busId, description); error.isNotEmpty())
                                          juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't load " + description.name, error);
                                  };

                                  if (effects.size() == 1 || safeButton == nullptr)
                                  {
                                      load (effects.getReference (0));
                                      return;
                                  }

                                  // Some plugins have several effects in them, so the user picks one.
                                  juce::PopupMenu menu;

                                  for (int i = 0; i < effects.size(); ++i)
                                      menu.addItem (i + 1, effects.getReference (i).name);

                                  menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (safeButton.getComponent()),
                                                      [effects, load] (int chosen)
                                                      {
                                                          if (chosen > 0)
                                                              load (effects.getReference (chosen - 1));
                                                      });
                              });
}

juce::String EffectRack::addEffect (int busId, const juce::PluginDescription& description, const juce::MemoryBlock& state, bool bypassed)
{
    juce::String error;
    auto effect = formatManager.createPluginInstance (description, host.getSampleRate(), host.getBlockSize(), error);

    if (effect == nullptr)
        return error.isNotEmpty() ? error : description.name + " couldn't be loaded.";

    if (! state.isEmpty())
        effect->setStateInformation (state.getData(), (int) state.getSize());

    lastStates[effect.get()] = getState (*effect);
    const auto index = host.getNumEffects (busId);
    host.insertEffect (busId, index, std::move (effect));
    host.setEffectBypassed (busId, index, bypassed);
    recentPlugins::remember (settings, PluginKind::effect, description);
    changed (true);
    return {};
}

void EffectRack::removeEffect (int busId, int index)
{
    closeEditor (host.getEffect (busId, index));
    lastStates.erase (host.getEffect (busId, index));
    host.removeEffect (busId, index);
    changed (true);
}

void EffectRack::moveEffect (int busId, int from, int to)
{
    host.moveEffect (busId, from, to);
    changed (true);
}

void EffectRack::setEffectBypassed (int busId, int index, bool bypassed)
{
    host.setEffectBypassed (busId, index, bypassed);
    changed (true);
}

void EffectRack::setBusOutput (int busId, int output)
{
    host.setBusOutput (busId, output);
    changed (true);
}

void EffectRack::setPartOutput (int part, int output)
{
    host.setPartOutput (part, output);
    changed (true);
}

//==============================================================================
void EffectRack::showEditor (int busId, int index)
{
    auto* effect = host.getEffect (busId, index);

    if (effect == nullptr)
        return;

    if (auto& window = editors[effect]; window != nullptr)
    {
        window->toFront (true);
        return;
    }

    // Closing the editor keeps whatever was changed in it.
    editors[effect] = std::make_unique<PluginWindow> (*effect, [this, effect]
    {
        closeEditor (effect);

        if (checkForSoundChanges())
            changed (true);
    });
}

void EffectRack::closeEditor (juce::AudioPluginInstance* effect)
{
    if (effect == nullptr)
        return;

    // Moved out first, as deleting the window can call back in here.
    if (const auto found = editors.find (effect); found != editors.end())
    {
        auto window = std::move (found->second);
        editors.erase (found);
    }
}

bool EffectRack::checkForSoundChanges()
{
    auto anyChanged = false;

    for (auto busId : host.getBuses())
    {
        for (int i = 0; i < host.getNumEffects (busId); ++i)
        {
            auto* effect = host.getEffect (busId, i);

            if (auto state = getState (*effect); state != lastStates[effect])
            {
                lastStates[effect] = std::move (state);
                anyChanged = true;
            }
        }
    }

    return anyChanged;
}

//==============================================================================
juce::var EffectRack::toJSON() const
{
    const auto buses = host.getBuses();
    const auto placeOf = [&buses] (int id)
    {
        const auto found = std::find (buses.begin(), buses.end(), id);
        return found == buses.end() ? -1 : (int) std::distance (buses.begin(), found);
    };

    juce::Array<juce::var> busList;

    for (auto busId : buses)
    {
        juce::Array<juce::var> effects;

        for (int i = 0; i < host.getNumEffects (busId); ++i)
        {
            auto* effect = host.getEffect (busId, i);
            auto* json = new juce::DynamicObject();
            json->setProperty ("name", effect->getName());
            json->setProperty ("plugin", effect->getPluginDescription().createXml()->toString (juce::XmlElement::TextFormat().singleLine().withoutHeader()));
            json->setProperty ("state", getState (*effect).toBase64Encoding());
            json->setProperty ("bypassed", host.isEffectBypassed (busId, i));
            effects.add (json);
        }

        auto* bus = new juce::DynamicObject();
        bus->setProperty ("effects", effects);
        bus->setProperty ("output", placeOf (host.getBusOutput (busId)));
        busList.add (bus);
    }

    // Each part's output: the bus's place, or -1 for the main output
    juce::Array<juce::var> parts;

    for (int part = 0; part < host.getNumParts(); ++part)
        parts.add (placeOf (host.getPartOutput (part)));

    auto* root = new juce::DynamicObject();
    root->setProperty ("buses", busList);
    root->setProperty ("parts", parts);
    return root;
}

juce::StringArray EffectRack::loadJSON (const juce::var& json)
{
    editors.clear();
    lastStates.clear();
    host.removeAllBuses();

    juce::StringArray failed;
    std::vector<int> ids;
    const auto* busList = json.getProperty ("buses", {}).getArray();

    if (busList != nullptr)
    {
        for (const auto& busJSON : *busList)
        {
            const auto busId = host.addBus();
            ids.push_back (busId);

            if (const auto* effects = busJSON.getProperty ("effects", {}).getArray())
            {
                for (const auto& effectJSON : *effects)
                {
                    juce::PluginDescription description;
                    const auto xml = juce::parseXML (effectJSON.getProperty ("plugin", {}).toString());

                    if (xml == nullptr || ! description.loadFromXml (*xml))
                    {
                        failed.add (effectJSON.getProperty ("name", "An effect").toString());
                        continue;
                    }

                    juce::MemoryBlock state;
                    state.fromBase64Encoding (effectJSON.getProperty ("state", {}).toString());
                    juce::String error;

                    if (auto effect = formatManager.createPluginInstance (description, host.getSampleRate(), host.getBlockSize(), error))
                    {
                        if (! state.isEmpty())
                            effect->setStateInformation (state.getData(), (int) state.getSize());

                        lastStates[effect.get()] = getState (*effect);
                        const auto index = host.getNumEffects (busId);
                        host.insertEffect (busId, index, std::move (effect));
                        host.setEffectBypassed (busId, index, (bool) effectJSON.getProperty ("bypassed", false));

                        // On the list to start with, after the ones chosen
                        recentPlugins::remember (settings, PluginKind::effect, description, false);
                    }
                    else
                    {
                        failed.add (description.name);
                    }
                }
            }
        }

        // Where each bus goes, now they're all there
        for (int i = 0; i < busList->size() && i < (int) ids.size(); ++i)
            if (const auto output = (int) busList->getReference (i).getProperty ("output", -1); juce::isPositiveAndBelow (output, (int) ids.size()))
                host.setBusOutput (ids[(size_t) i], ids[(size_t) output]);
    }

    if (const auto* parts = json.getProperty ("parts", {}).getArray())
        for (int part = 0; part < host.getNumParts() && part < parts->size(); ++part)
            if (const auto output = (int) parts->getReference (part); juce::isPositiveAndBelow (output, (int) ids.size()))
                host.setPartOutput (part, ids[(size_t) output]);

    changed (false);
    return failed;
}
