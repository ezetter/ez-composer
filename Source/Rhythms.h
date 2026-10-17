#pragma once

#include "SavedLibrary.h"
#include "Score.h"

#include <array>
#include <optional>
#include <vector>

/** A rhythm saved to use again, for every score */
using SavedRhythm = Rhythm;

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
