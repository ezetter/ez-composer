#pragma once

#include <juce_data_structures/juce_data_structures.h>

#include <algorithm>
#include <optional>
#include <vector>

/** An item of a JSON list, or nothing if it isn't a list, or it's past the end. */
inline juce::var getListItem (const juce::var& list, int index)
{
    if (const auto* items = list.getArray(); items != nullptr && juce::isPositiveAndBelow (index, items->size()))
        return items->getReference (index);

    return {};
}

/** A name as saved things keep it: on one line, without spaces at either end, and no longer than the longest. */
inline juce::String tidySavedName (const juce::String& name, int maxLength)
{
    return name.replaceCharacters ("\r\n\t", "   ").trim().substring (0, maxLength).trimEnd();
}

/** Things saved for every score, such as chord progressions or rhythms, kept in the app's
    settings, under their type's settingsKey, as JSON, in order of name. Names are told apart
    without regard to case. A type has a name, toJSON(), and fromJSON(), which gives nothing for
    JSON that isn't one.
*/
template <typename Item>
class SavedLibrary
{
public:
    explicit SavedLibrary (juce::PropertiesFile& settingsToUse)
        : settings (settingsToUse)
    {
        const auto saved = juce::JSON::parse (settings.getValue (Item::settingsKey));

        if (const auto* list = saved.getArray())
            for (const auto& entry : *list)
                if (auto item = Item::fromJSON (entry); item.has_value() && find (item->name) == nullptr)
                    items.push_back (std::move (*item));

        std::sort (items.begin(), items.end(), [] (const Item& a, const Item& b) { return a.name.compareNatural (b.name) < 0; });
    }

    /** In order of name */
    const std::vector<Item>& getAll() const noexcept { return items; }

    const Item* find (const juce::String& name) const
    {
        const auto found = std::find_if (items.begin(), items.end(), [&] (const Item& item) { return item.name.compareIgnoreCase (name) == 0; });
        return found != items.end() ? &*found : nullptr;
    }

    /** Keeps one, in place of one with the same name. */
    void save (const Item& item)
    {
        erase (item.name);
        items.insert (std::find_if (items.begin(), items.end(), [&] (const Item& other) { return item.name.compareNatural (other.name) < 0; }), item);
        write();
    }

    void remove (const juce::String& name)
    {
        if (erase (name))
            write();
    }

private:
    /** Takes out the one with a name, if there is one, saying whether there was. */
    bool erase (const juce::String& name)
    {
        const auto before = items.size();
        std::erase_if (items, [&] (const Item& item) { return item.name.compareIgnoreCase (name) == 0; });
        return items.size() != before;
    }

    void write()
    {
        juce::Array<juce::var> list;

        for (const auto& item : items)
            list.add (item.toJSON());

        settings.setValue (Item::settingsKey, juce::JSON::toString (juce::var (list), true));
        settings.saveIfNeeded();
    }

    juce::PropertiesFile& settings;
    std::vector<Item> items;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SavedLibrary)
};
