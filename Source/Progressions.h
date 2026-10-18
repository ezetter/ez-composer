#pragma once

#include "SavedLibrary.h"
#include "Score.h"

#include <optional>
#include <vector>

/** A chord progression saved to use again, in this score or another: the notes of a part's
    chords, one for each measure that had one, in order, without how they were written (their
    chord type, clef or note length).
*/
struct SavedProgression
{
    struct Chord
    {
        music::ChordSpec spec;

        /** Notes set on the piano, in the key and on the staff the chord was saved from, when
            they're not simply the spec's own voicing.
        */
        std::optional<std::vector<music::KeyboardNote>> keyboardNotes;
        Staff staff = Staff::treble;

        bool operator== (const Chord&) const = default;
    };

    static constexpr int maxNameLength = 80;
    static constexpr const char* settingsKey = "savedProgressions";

    juce::String name;
    music::Key key { music::getMajorKeys()[0] };       // the key it was saved in
    std::vector<Chord> chords;

    /** The chords' numerals, one after another, e.g. "I – IV – V7 – I", or for notes that don't
        make a chord with a numeral, their names. Shortened, with "…", to the longest a name goes.
    */
    juce::String getNumerals() const;

    /** The chords, in a key, written in a style. A spec's chord keeps its numeral; notes set on
        the piano move with the key's tonic, and go to the staff, as far into its range.
    */
    std::vector<MeasureChord> createChords (music::Key, const ChordStyle&) const;

    /** The progression with every chord a numeral describes suspended, sus2 or sus4, with its 2nd
        or 4th in place of its 3rd, its numeral's own notes in place of any set on the piano, as the
        chord window does. Notes that don't make a chord with a numeral stay as they are.
    */
    SavedProgression suspended (music::Suspension) const;

    /** A name as it's kept: on one line, without spaces at either end, and no longer than the longest. */
    static juce::String tidyName (const juce::String&);

    juce::var toJSON() const;

    /** A progression from JSON, or nothing if it isn't one, or has no chords. */
    static std::optional<SavedProgression> fromJSON (const juce::var&);

    bool operator== (const SavedProgression&) const = default;
};

/** A part's progression: the chords of the measures that have one, in order, named for their numerals. */
SavedProgression getProgression (const Score&, int part);

/** The progressions saved, for every score, in the app's settings. */
using ProgressionLibrary = SavedLibrary<SavedProgression>;
