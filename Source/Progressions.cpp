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

        if (chord.spec.suspension != music::Suspension::none)
            chordObject->setProperty ("suspension", (int) chord.spec.suspension);
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
        chord.spec.suspension = (music::Suspension) readInt (c.getProperty ("suspension", {}), 0, 2, 0);
        music::tidySuspension (chord.spec);
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

//==============================================================================
namespace
{
    juce::String utf8 (const char* text) { return juce::String (juce::CharPointer_UTF8 (text)); }
}

bool ChordCriteria::isEmpty() const noexcept
{
    return ! (minor.has_value() || degree.has_value() || accidental.has_value() || addedNote.has_value()
              || suspension.has_value() || alteration.has_value());
}

bool ChordCriteria::matches (const music::ChordSpec& spec) const
{
    if (isEmpty())
        return true;

    if (spec.isEmpty())
        return false;

    const auto chordAccidental = spec.flat ? -1 : spec.sharp ? 1 : 0;
    const auto chordAlteration = ! spec.altered ? 0 : spec.minor ? -1 : 1;

    return (! minor.has_value() || *minor == spec.minor)
        && (! degree.has_value() || *degree == spec.degree)
        && (! accidental.has_value() || *accidental == chordAccidental)
        && (! addedNote.has_value() || *addedNote == spec.addedNote)
        && (! suspension.has_value() || *suspension == spec.suspension)
        && (! alteration.has_value() || *alteration == chordAlteration);
}

juce::String ChordCriteria::describe() const
{
    static const juce::StringArray numerals { "I", "II", "III", "IV", "V", "VI", "VII" };
    juce::StringArray parts;

    if (minor.has_value())
        parts.add (*minor ? "minor" : "major");

    if (degree.has_value() || accidental.has_value())
    {
        const auto sign = ! accidental.has_value() ? juce::String()
                        : *accidental < 0 ? utf8 ("\xe2\x99\xad")
                        : *accidental > 0 ? utf8 ("\xe2\x99\xaf")
                                          : juce::String();
        const auto numeral = degree.has_value() ? numerals[juce::jlimit (0, 6, *degree)] : juce::String();

        if (accidental.has_value() && *accidental == 0)
            parts.add (numeral.isNotEmpty() ? "natural " + numeral : juce::String ("natural"));
        else if (numeral.isNotEmpty())
            parts.add (sign + numeral);
        else
            parts.add (*accidental < 0 ? "flat" : "sharp");
    }

    if (addedNote.has_value())
        parts.add (*addedNote == music::AddedNote::none ? juce::String ("no added note")
                                                        : juce::String (music::getInfo (*addedNote).name).toLowerCase());

    if (suspension.has_value())
        parts.add (*suspension == music::Suspension::none ? juce::String ("not sus") : music::getSuspensionText (*suspension));

    if (alteration.has_value())
        parts.add (*alteration > 0 ? "augmented" : *alteration < 0 ? "diminished" : "not augmented or diminished");

    return parts.joinIntoString (", ");
}

bool ProgressionSearch::isEmpty() const noexcept
{
    return nameStart.trim().isEmpty() && ! numChords.has_value()
        && std::all_of (chords.begin(), chords.end(), [] (const ChordCriteria& c) { return c.isEmpty(); });
}

bool ProgressionSearch::matches (const SavedProgression& progression) const
{
    if (const auto start = nameStart.trim(); start.isNotEmpty() && ! progression.name.startsWithIgnoreCase (start))
        return false;

    if (numChords.has_value() && *numChords != (int) progression.chords.size())
        return false;

    // A chord looked for that isn't there doesn't match.
    for (size_t i = 0; i < chords.size(); ++i)
        if (! chords[i].isEmpty() && (i >= progression.chords.size() || ! chords[i].matches (progression.chords[i].spec)))
            return false;

    return true;
}

std::vector<SavedProgression> ProgressionSearch::filter (const std::vector<SavedProgression>& progressions) const
{
    std::vector<SavedProgression> found;

    for (const auto& progression : progressions)
        if (matches (progression))
            found.push_back (progression);

    return found;
}

juce::String ProgressionSearch::describe() const
{
    juce::StringArray parts;

    if (const auto start = nameStart.trim(); start.isNotEmpty())
        parts.add ("name starting " + utf8 ("\xe2\x80\x9c") + start + utf8 ("\xe2\x80\x9d"));

    if (numChords.has_value())
        parts.add (juce::String (*numChords) + (*numChords == 1 ? " chord" : " chords"));

    for (size_t i = 0; i < chords.size(); ++i)
        if (! chords[i].isEmpty())
            parts.add ("chord " + juce::String ((int) i + 1) + ": " + chords[i].describe());

    return parts.joinIntoString ("; ");
}
