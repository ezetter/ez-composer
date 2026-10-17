#include "Progressions.h"

#include <algorithm>

namespace
{
    /** A number from the settings, or the fallback if it's missing or out of range. */
    int readInt (const juce::var& value, int lowest, int highest, int fallback)
    {
        if (! (value.isInt() || value.isInt64() || value.isDouble()))
            return fallback;

        const auto number = (int) value;
        return number >= lowest && number <= highest ? number : fallback;
    }

    juce::var spellingToJSON (music::Spelling spelling)
    {
        return juce::Array<juce::var> { spelling.letter, spelling.alter };
    }

    std::optional<music::Spelling> readSpelling (const juce::var& value)
    {
        const music::Spelling spelling { readInt (getListItem (value, 0), 0, 6, -1), readInt (getListItem (value, 1), -2, 2, 99) };

        if (spelling.letter < 0 || spelling.alter == 99)
            return {};

        return spelling;
    }

    music::Chord createChord (music::Key key, const SavedProgression::Chord& chord)
    {
        return chord.keyboardNotes.has_value() ? music::createChord (key, *chord.keyboardNotes, chord.spec, chord.staff)
                                               : music::createChord (key, chord.spec, chord.staff);
    }
}

//==============================================================================
juce::String SavedProgression::getNumerals() const
{
    juce::StringArray labels;

    for (const auto& chord : chords)
        labels.add (createChord (key, chord).label);

    auto numerals = labels.joinIntoString (juce::String (juce::CharPointer_UTF8 (" \xe2\x80\x93 ")));

    if (numerals.length() > maxNameLength)
        numerals = numerals.substring (0, maxNameLength - 1).trimEnd() + juce::String (juce::CharPointer_UTF8 ("\xe2\x80\xa6"));

    return numerals;
}

std::vector<MeasureChord> SavedProgression::createChords (music::Key newKey, const ChordStyle& style) const
{
    std::vector<MeasureChord> created;

    for (const auto& chord : chords)
    {
        MeasureChord measureChord;
        measureChord.style = style;

        if (chord.keyboardNotes.has_value())
        {
            // Described again, as the notes are, in the new key and on the new staff
            const auto moved = music::transposeNotes (*chord.keyboardNotes, key, chord.staff, newKey, style.staff);

            if (! moved.empty())
            {
                const auto description = music::describeNotes (newKey, style.staff, moved);
                measureChord.spec = description.spec;
                measureChord.keyboardNotes = description.keyboardNotes;
            }
        }
        else
        {
            measureChord.spec = chord.spec;
        }

        created.push_back (measureChord);
    }

    return created;
}

juce::String SavedProgression::tidyName (const juce::String& name)
{
    return tidySavedName (name, maxNameLength);
}

juce::var SavedProgression::toJSON() const
{
    auto* object = new juce::DynamicObject();
    object->setProperty ("name", name);
    object->setProperty ("tonic", spellingToJSON (key.tonic));
    object->setProperty ("minor", key.minor);

    juce::Array<juce::var> chordList;

    for (const auto& chord : chords)
    {
        auto* chordObject = new juce::DynamicObject();
        chordObject->setProperty ("degree", chord.spec.degree);
        chordObject->setProperty ("flat", chord.spec.flat);
        chordObject->setProperty ("sharp", chord.spec.sharp);
        chordObject->setProperty ("minor", chord.spec.minor);
        chordObject->setProperty ("altered", chord.spec.altered);
        chordObject->setProperty ("addedNote", (int) chord.spec.addedNote);
        chordObject->setProperty ("inversion", chord.spec.inversion);
        chordObject->setProperty ("octave", chord.spec.octave);
        chordObject->setProperty ("staff", chord.staff == Staff::treble ? "treble" : "bass");

        if (chord.keyboardNotes.has_value())
        {
            juce::Array<juce::var> notes;

            for (const auto& note : *chord.keyboardNotes)
                notes.add (juce::Array<juce::var> { note.midi, note.spelling.letter, note.spelling.alter });

            chordObject->setProperty ("keyboardNotes", notes);
        }

        chordList.add (juce::var (chordObject));
    }

    object->setProperty ("chords", chordList);
    return juce::var (object);
}

std::optional<SavedProgression> SavedProgression::fromJSON (const juce::var& json)
{
    if (! json.isObject())
        return {};

    const auto tonic = readSpelling (json.getProperty ("tonic", {}));
    const auto* chordList = json.getProperty ("chords", {}).getArray();

    if (! tonic.has_value() || chordList == nullptr)
        return {};

    SavedProgression progression;
    progression.key = music::Key (*tonic, (bool) json.getProperty ("minor", false));

    for (const auto& c : *chordList)
    {
        Chord chord;
        chord.spec.degree = readInt (c.getProperty ("degree", {}), -1, 6, -1);
        chord.spec.flat = (bool) c.getProperty ("flat", false);
        chord.spec.sharp = (bool) c.getProperty ("sharp", false) && ! chord.spec.flat;
        chord.spec.minor = (bool) c.getProperty ("minor", false);
        chord.spec.altered = (bool) c.getProperty ("altered", false);
        chord.spec.addedNote = (music::AddedNote) readInt (c.getProperty ("addedNote", {}), 0, 6, 0);
        chord.spec.inversion = readInt (c.getProperty ("inversion", {}), 0, 3, 0);
        chord.spec.octave = readInt (c.getProperty ("octave", {}), -1, 1, 0);
        chord.staff = c.getProperty ("staff", {}).toString() == "bass" ? Staff::bass : Staff::treble;

        // The combinations the chord panel allows: an added note suits the chord's quality and
        // can't go with + / °, and only 7th chords have a 3rd inversion.
        const auto choices = music::getAddedNoteChoices (chord.spec.minor);

        if (chord.spec.addedNote != music::AddedNote::none
            && (chord.spec.altered || std::find (choices.begin(), choices.end(), chord.spec.addedNote) == choices.end()))
            chord.spec.addedNote = music::AddedNote::none;

        if (chord.spec.inversion == 3 && chord.spec.addedNote == music::AddedNote::none)
            chord.spec.inversion = 0;

        if (const auto* notes = c.getProperty ("keyboardNotes", {}).getArray())
        {
            std::vector<music::KeyboardNote> keyboardNotes;

            for (const auto& item : *notes)
            {
                const music::KeyboardNote note { readInt (getListItem (item, 0), 0, 127, -1),
                                                 { readInt (getListItem (item, 1), 0, 6, -1), readInt (getListItem (item, 2), -2, 2, 99) } };

                // Only real notes, spelled as they sound
                if (note.midi >= 0 && note.spelling.letter >= 0 && note.spelling.alter != 99
                    && note.spelling.getPitchClass() == music::mod (note.midi, 12))
                    keyboardNotes.push_back (note);
            }

            if (! keyboardNotes.empty())
                chord.keyboardNotes = std::move (keyboardNotes);
        }

        if (! chord.spec.isEmpty() || chord.keyboardNotes.has_value())
            progression.chords.push_back (chord);
    }

    if (progression.chords.empty())
        return {};

    progression.name = tidyName (json.getProperty ("name", {}).toString());

    if (progression.name.isEmpty())
        progression.name = progression.getNumerals();

    return progression;
}

//==============================================================================
SavedProgression getProgression (const Score& score, int part)
{
    SavedProgression progression;
    progression.key = score.getKey();

    for (int measure = 0; measure < score.getNumMeasures(); ++measure)
        if (const auto* chord = score.getChord (part, measure); chord != nullptr && chord->hasNotes())
            progression.chords.push_back ({ chord->spec, chord->keyboardNotes, chord->style.staff });

    progression.name = progression.getNumerals();
    return progression;
}
