#pragma once

#include "SavedLibrary.h"
#include "Score.h"

#include <array>
#include <optional>
#include <vector>

/** A rhythm saved to use again, in this score or another: when the notes written on each staff
    of some measures start, how long each is, whether it's staccato, and whether it's tied to the
    next, whatever their pitches. The rests are the gaps between them. A rhythm that had notes on
    both staves keeps them on their own staves, in step with each other.
*/
struct SavedRhythm
{
    /** A note, or notes starting together, in 32nds from the rhythm's start */
    struct Hit
    {
        int onset = 0;
        int length = Score::slotsPerBeat;
        bool staccato = false;
        bool tiedToNext = false;

        bool operator== (const Hit&) const = default;
    };

    static constexpr int maxNameLength = 80;
    static constexpr const char* settingsKey = "savedRhythms";

    juce::String name;
    int beatsPerMeasure = 4;        // of the measures it was saved from
    int numMeasures = 1;
    std::array<std::vector<Hit>, 2> staves;     // the treble staff's and the bass staff's, in order

    /** How long it is, in 32nds: as long as its measures */
    int getLength() const noexcept { return numMeasures * beatsPerMeasure * Score::slotsPerBeat; }

    bool hasNotes (Staff staff) const noexcept { return ! staves[(size_t) staff].empty(); }
    bool hasNotes() const noexcept { return hasNotes (Staff::treble) || hasNotes (Staff::bass); }

    /** e.g. "2 measures of 4/4: 6 notes on the treble staff, 4 on the bass" */
    juce::String describe() const;

    juce::var toJSON() const;

    /** A rhythm from JSON, or nothing if it isn't one, or has no notes. */
    static std::optional<SavedRhythm> fromJSON (const juce::var&);

    bool operator== (const SavedRhythm&) const = default;
};

/** The rhythm of what's written on a part's staves in some measures, from one: its notes, its
    chords' notes, and its alternate staff's. A note tied over from the measure before starts it.
*/
SavedRhythm getRhythm (const Score&, int part, int firstMeasure, int numMeasures);

/** How many measures a part's rhythm can be saved from, from one: up to the end of the page,
    but not to a barline a tie goes over, and only as many as have some notes in them.
*/
std::vector<int> getRhythmLengthChoices (const Score&, int part, int firstMeasure);

/** The notes a rhythm, played so many times over, gives from a note: each where its hit starts,
    and as long, cut into notes tied over the barlines, and into notes of the lengths there are,
    tied together, where it won't go in one. A rhythm with notes on both staves has them on both,
    the other staff's an octave from the note, above it or below; one on one staff has them all
    on the note's staff. All of them are at the note's pitch, and are in order on each staff.
*/
struct RhythmPlan
{
    std::vector<PlacedNote> notes;

    /** The stretch of each staff the rhythm takes, in 32nds from the start of the score, rests and all */
    std::array<std::optional<juce::Range<int>>, 2> spans;
};

RhythmPlan planRhythm (const SavedRhythm&, int times, const Note& start, int beatsPerMeasure);

/** The rhythms saved, for every score, in the app's settings. */
using RhythmLibrary = SavedLibrary<SavedRhythm>;
