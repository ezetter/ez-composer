#pragma once

#include <juce_core/juce_core.h>

/** Copies the settings file the app kept under its old name, Anthropocene Music, to where it
    keeps them now, if there isn't one there yet, so it starts with the same instrument, effects,
    recent scores, and saved progressions and rhythms. The old file stays where it was. Says
    whether it copied it.
*/
inline bool copyOldSettings (const juce::File& oldFile, const juce::File& newFile)
{
    return oldFile.existsAsFile() && ! newFile.exists() && newFile.getParentDirectory().createDirectory() && oldFile.copyFileTo (newFile);
}
