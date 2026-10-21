#pragma once

#include "SavedLibrary.h"
#include "Score.h"

#include <array>
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

    /** A name as it's kept: on one line, without spaces at either end, and no longer than the longest. */
    static juce::String tidyName (const juce::String&);

    juce::var toJSON() const;

    /** A progression from JSON, or nothing if it isn't one, or has no chords. */
    static std::optional<SavedProgression> fromJSON (const juce::var&);

    bool operator== (const SavedProgression&) const = default;
};

/** What to look for in a saved progression's chord, for Search. Each part is left out, so any chord
    will do for it, while it's empty. A chord whose notes don't make one with a numeral has none of
    them, so only a chord with nothing to look for will do for it.
*/
struct ChordCriteria
{
    std::optional<bool> minor;                      // major or minor
    std::optional<int> degree;                      // the scale step, from 0 for I to 6 for VII
    std::optional<int> accidental;                  // -1 for flat, 0 for neither, 1 for sharp
    std::optional<music::AddedNote> addedNote;      // AddedNote::none for none
    std::optional<music::Suspension> suspension;    // Suspension::none for not suspended
    std::optional<int> alteration;                  // 0 for neither, 1 for augmented, -1 for diminished

    bool isEmpty() const noexcept;
    bool matches (const music::ChordSpec&) const;

    /** What it looks for, e.g. "minor, ♭VI, sus4", or nothing if it's empty */
    juce::String describe() const;

    bool operator== (const ChordCriteria&) const = default;
};

/** What Search looks for in the saved progressions: a name starting with some letters, a
    number of chords, and what the first four chords are like. Each part is left out while it's
    empty, so an empty search finds them all.
*/
struct ProgressionSearch
{
    static constexpr int numChordsSearched = 4;

    juce::String nameStart;                         // the first letters of the name, whatever their case
    std::optional<int> numChords;
    std::array<ChordCriteria, numChordsSearched> chords;

    bool isEmpty() const noexcept;
    bool matches (const SavedProgression&) const;

    /** The ones that match, in their order */
    std::vector<SavedProgression> filter (const std::vector<SavedProgression>&) const;

    /** What it looks for, e.g. "Name starting “Bl”, 4 chords, chord 1: I", or nothing if it's empty */
    juce::String describe() const;

    bool operator== (const ProgressionSearch&) const = default;
};

/** A part's progression: the chords of the measures that have one, in order, named for their numerals. */
SavedProgression getProgression (const Score&, int part);

/** The progressions saved, for every score, in the app's settings. */
using ProgressionLibrary = SavedLibrary<SavedProgression>;
