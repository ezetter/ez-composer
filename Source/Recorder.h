#pragma once

#include "Piece.h"

#include <optional>
#include <vector>

/** Writes the keys played while recording into the score, each as a note as long as the key was
    held, on the treble staff from middle C up and the bass staff below it, in the part it played.

    Keys are given by the page of the piece they're played on, whichever page is being edited,
    and where the page has got to as it's played, its repeats written out, and their notes go in
    that page's measures as written: a key played the second time through a repeat goes in the
    repeated measure, and one held as the music goes back for a repeat, or on to another page,
    ends there.

    Notes start and end on the nearest 16th note. A note within a measure is written as the length
    nearest to how long it was held; one held over a barline is tied across it. A note still held
    when another starts on its staff ends there, as a staff has one line of notes at a time, and
    notes starting together make a chord.
*/
class Recorder
{
public:
    explicit Recorder (Piece&);

    /** A key going down, on a page, at a point in the page, in beats from its start as it's
        played, playing a part.
    */
    void keyDown (int midiNote, int page, double beat, int partId);

    /** A key coming up, which writes its note. On another page than it went down on, or at a
        beat before it went down, as when a loop's gone back to the start while it was held, the
        note ends at the end of its page.
    */
    void keyUp (int midiNote, int page, double beat);

    /** Writes the notes of the keys still down, ending them at a point on a page, or at the end
        of their pages.
    */
    void finish (std::optional<std::pair<int, double>> pageAndBeat);

    /** Forgets the keys still down, without writing them. */
    void clear() noexcept { held.clear(); }

    /** What notes start and end on, in 32nds: a 16th note. */
    static constexpr int grid = 2;

private:
    struct HeldKey
    {
        int midiNote;
        int partId;
        Staff staff;
        int page;
        double beat;                    // where it went down, from the start of its page, as it's played
        int start;                      // where its note starts, in 32nds from the start of its page, as it's played
        std::optional<int> cut;         // where another note on its staff starts, if one has since
    };

    /** Writes a key's note, ending at a beat of its page, or a negative one for the end of the page. */
    void write (const HeldKey&, double endBeat);

    /** How long a page is, as it's played, its repeats and all, in 32nds. */
    static int getScoreSlots (const Score&);

    Piece& piece;
    std::vector<HeldKey> held;
};
