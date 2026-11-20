#include "MeasureContent.h"

#include <algorithm>
#include <functional>

namespace
{
    music::Tone makeTone (const music::Pitch& pitch, const std::array<int, 7>& keyAlterations)
    {
        return { pitch, pitch.getMidiNoteNumber(), pitch.alter == keyAlterations[(size_t) pitch.getLetter()] };
    }

    /** The chord's notes in the order its chord type writes them. */
    std::vector<music::Tone> getSequence (const music::Chord& chord, const MeasureChord& measureChord)
    {
        auto tones = chord.tones;

        switch (measureChord.style.type)
        {
            case music::ChordType::arpeggioDown:
                std::reverse (tones.begin(), tones.end());
                return tones;

            case music::ChordType::random:
            {
                std::vector<music::Tone> sequence;

                for (auto midi : measureChord.randomOrder)
                    if (const auto tone = std::find_if (tones.begin(), tones.end(), [midi] (const music::Tone& t) { return t.midi == midi; });
                        tone != tones.end())
                        sequence.push_back (*tone);

                return sequence.empty() ? tones : sequence;
            }

            case music::ChordType::arpeggioUp:
            case music::ChordType::block:
            case music::ChordType::rolled:
                break;
        }

        return tones;
    }

    /** Rests filling an empty stretch of a measure, from one 64th note to another. A whole beat
        gets a quarter rest, and two that make up the first half of 4/4 or 3/4, or the second half
        of 4/4, share a half rest. Part of a beat gets the longest of an eighth, 16th, 32nd or 64th rest
        that starts where it does, in step with the beat, and fits.
    */
    void addRests (std::vector<StaffEvent>& events, int from, int to, int beatsPerMeasure)
    {
        constexpr auto perBeat = Score::slotsPerBeat;

        for (auto slot = from; slot < to;)
        {
            const auto onset = (double) slot / perBeat;
            const auto beat = slot / perBeat;
            const auto beatEmpty = slot % perBeat == 0 && slot + perBeat <= to;

            if (beatEmpty && beat % 2 == 0 && beat + 1 < beatsPerMeasure && slot + 2 * perBeat <= to)
            {
                events.push_back ({ onset, Duration::half, {}, false, false });
                slot += 2 * perBeat;
                continue;
            }

            if (beatEmpty)
            {
                events.push_back ({ onset, Duration::quarter, {}, false, false });
                slot += perBeat;
                continue;
            }

            auto restLength = perBeat / 2;

            while (restLength > 1 && (slot % restLength != 0 || slot + restLength > to))
                restLength /= 2;

            events.push_back ({ onset, getDurationForBeats ((double) restLength / perBeat), {}, false, false });
            slot += restLength;
        }
    }

    /** How many notes a beat has room for: as many as the shortest note or rest needs. */
    void setNotesPerBeat (StaffContent& content)
    {
        auto shortest = 1.0;

        for (const auto& event : content.events)
            shortest = juce::jmin (shortest, event.getPlayedBeats());

        content.notesPerBeat = juce::jlimit (1, Score::slotsPerBeat, juce::roundToInt (1.0 / shortest));
    }

    music::Tone shiftedByOctave (music::Tone tone, int octaves)
    {
        tone.pitch.step += 7 * octaves;
        tone.midi += 12 * octaves;
        return tone;
    }

    /** One time round an arpeggio, which goes back and forth between the chord's lowest note and
        its octave, if the chord doesn't reach that far, or else its highest note. Going up, it
        starts on the lowest note and goes up first: C E G C' G E. Going down, it starts on the
        chord's highest note and goes down first, so it carries on from an arpeggio going up in
        the measure before: G E C E G C'. Played over and over, it fills a measure.
    */
    std::vector<music::Tone> getArpeggioCycle (std::vector<music::Tone> tones, bool upwards)
    {
        std::sort (tones.begin(), tones.end(), [] (const music::Tone& a, const music::Tone& b) { return a.midi < b.midi; });

        if (tones.empty())
            return tones;

        // Up through the notes to the turn at the top, and back down to the second note
        const auto numTones = (int) tones.size();
        const auto turn = shiftedByOctave (tones.front(), 1);
        const auto reachesTurn = tones.back().midi >= turn.midi;
        auto cycle = tones;

        if (! reachesTurn)
            cycle.push_back (turn);

        for (auto i = numTones - (reachesTurn ? 2 : 1); i >= 1; --i)
            cycle.push_back (tones[(size_t) i]);

        if (upwards)
            return cycle;

        // Going down, from the highest of the chord's own notes: on its way down, after the
        // octave, or at the top if there isn't one
        const auto start = (size_t) ((reachesTurn ? numTones - 1 : numTones + 1) % (int) cycle.size());
        std::rotate (cycle.begin(), cycle.begin() + (std::ptrdiff_t) start, cycle.end());
        return cycle;
    }

    /** The notes a melodic chord's notes can be moved to: its own, in every octave there is, in
        order of pitch.
    */
    std::vector<music::Tone> getNoteChoices (const music::Chord& chord)
    {
        std::vector<music::Tone> choices;

        for (const auto& tone : chord.tones)
        {
            for (int octaves = -10; octaves <= 10; ++octaves)
            {
                const auto shifted = shiftedByOctave (tone, octaves);

                if (shifted.pitch.step >= 0 && juce::isPositiveAndBelow (shifted.midi, 128)
                    && std::none_of (choices.begin(), choices.end(), [&] (const music::Tone& t) { return t.midi == shifted.midi; }))
                    choices.push_back (shifted);
            }
        }

        std::sort (choices.begin(), choices.end(), [] (const music::Tone& a, const music::Tone& b) { return a.midi < b.midi; });
        return choices;
    }

    /** The notes as they've been moved by hand, if they still fit: as many of them, each one of the chord's. */
    std::optional<std::vector<music::Tone>> getMovedNotes (const music::Chord& chord, const MeasureChord& measureChord, size_t numNotes)
    {
        if (measureChord.movedNotes.empty() || measureChord.movedNotes.size() != numNotes)
            return {};

        const auto choices = getNoteChoices (chord);
        std::vector<music::Tone> moved;

        for (auto midi : measureChord.movedNotes)
        {
            const auto choice = std::find_if (choices.begin(), choices.end(), [midi] (const music::Tone& t) { return t.midi == midi; });

            if (choice == choices.end())
                return {};

            moved.push_back (*choice);
        }

        return moved;
    }

    /** A chord's notes in its rhythm, in one of the measures it takes: all its notes together for
        each of the rhythm's notes, for a block or rolled chord, or the next of the notes it plays
        in turn for each that starts afresh, the one before held on for each tied on, with rests
        between them. A measure the rhythm has no notes in has a whole rest.
    */
    StaffContent getRhythmContent (const Score& score, const std::vector<RhythmNote>& notes, StaffContent::Source source,
                                   const std::function<std::vector<music::Tone> (int attack)>& tonesFor, bool rolled)
    {
        StaffContent content;
        content.source = source;
        const auto beats = score.getBeatsPerMeasure();
        const auto measureLength = beats * Score::slotsPerBeat;

        if (notes.empty())
        {
            content.events.push_back ({ 0.0, Duration::whole, {}, false, true });
            return content;
        }

        auto slot = 0;
        std::optional<int> lastAttack;

        for (const auto& note : notes)
        {
            // A tuplet's note, evenly through it, on from the one before
            if (note.isTuplet())
            {
                if (note.tupletIndex == 0)
                    addRests (content.events, slot, note.onset, beats);

                const auto start = (double) note.onset / Score::slotsPerBeat;
                StaffEvent event { start + (double) note.tupletIndex * note.length / juce::jmax (1, note.tupletNotes) / Score::slotsPerBeat,
                                   getDurationForBeats ((double) note.tupletNoteLength / Score::slotsPerBeat), tonesFor (note.attack),
                                   rolled, false, note.staccato };
                event.tupletStart = start;
                event.tupletNotes = note.tupletNotes;
                content.events.push_back (event);
                lastAttack = note.attack;
                slot = note.onset + note.length;
                continue;
            }

            addRests (content.events, slot, note.onset, beats);

            // Rolled as it starts, not as it's held on
            const auto onset = (double) note.onset / Score::slotsPerBeat;
            auto tones = tonesFor (note.attack);
            content.events.push_back ({ onset, getDurationForBeats ((double) note.length / Score::slotsPerBeat), tones,
                                        rolled && lastAttack != note.attack, false, note.staccato });
            lastAttack = note.attack;

            if (note.tiedToNext)
                for (const auto& tone : tones)
                    content.tiedNotes.push_back ({ onset, tone.pitch });

            slot = note.onset + note.length;
        }

        addRests (content.events, slot, measureLength, beats);
        setNotesPerBeat (content);
        return content;
    }

    /** A chord's notes, from one 64th of the measure for so many: the whole measure, or half of it. */
    StaffContent getChordContent (const Score& score, const music::Chord& chord, const MeasureChord& measureChord, int from, int span)
    {
        StaffContent content;
        content.source = StaffContent::Source::chord;

        const auto beats = score.getBeatsPerMeasure();
        const auto measureLength = beats * Score::slotsPerBeat;
        const auto end = from + span;
        const auto length = score.getChordNoteLength (measureChord);
        const auto type = measureChord.style.type;
        const auto add = [&] (int slot, std::vector<music::Tone> tones, int slots, bool rolled)
        {
            content.events.push_back ({ (double) slot / Score::slotsPerBeat, getDurationForBeats ((double) slots / Score::slotsPerBeat),
                                        std::move (tones), rolled, false });
        };

        // In a rhythm, its notes go as the rhythm's do, in place of the note length.
        if (measureChord.style.rhythm.has_value())
        {
            const auto& hits = measureChord.style.rhythm->getHitsFor (measureChord.style.staff);
            const auto notes = layOutRhythm (hits, measureLength, measureChord.rhythmMeasure);

            if (! music::isMelodic (type))
                return getRhythmContent (score, notes, StaffContent::Source::chord, [&] (int) { return chord.tones; }, type == music::ChordType::rolled);

            // One note at a time, going round as the chord type has them, the next for each of the
            // rhythm's notes that starts afresh: an arpeggio up and down, or the random order
            auto sequence = type == music::ChordType::random ? getSequence (chord, measureChord)
                                                              : getArpeggioCycle (chord.tones, type == music::ChordType::arpeggioUp);

            if (sequence.empty())
                sequence = chord.tones;

            auto inRhythm = getRhythmContent (score, notes, StaffContent::Source::chord,
                                              [&] (int attack) { return std::vector<music::Tone> { sequence[(size_t) (attack % (int) sequence.size())] }; }, false);

            // Unless its notes have been moved by hand to others of the chord's
            std::vector<StaffEvent*> written;

            for (auto& event : inRhythm.events)
                if (! event.isRest())
                    written.push_back (&event);

            if (const auto moved = getMovedNotes (chord, measureChord, written.size()))
            {
                inRhythm.tiedNotes.clear();

                for (size_t i = 0; i < written.size(); ++i)
                    written[i]->tones = { (*moved)[i] };

                // Ties hold on only notes that stay the same, and the last on into the next measure.
                for (size_t i = 0; i < written.size(); ++i)
                    if (notes[i].tiedToNext && (i + 1 == written.size() || (*moved)[i].midi == (*moved)[i + 1].midi))
                        inRhythm.tiedNotes.push_back ({ written[i]->onset, (*moved)[i].pitch });
            }

            return inRhythm;
        }

        if (! music::isMelodic (type))
        {
            // As long as its note length, from the start of the measure, or of its half, so it
            // lines up with the first beat of anything on the other staff, and again until it's
            // full: the whole measure, or half, unless a shorter length's been chosen.
            auto slot = from;

            for (; slot + length <= end; slot += length)
                add (slot, chord.tones, length, type == music::ChordType::rolled);

            addRests (content.events, slot, end, beats);
            setNotesPerBeat (content);
            return content;
        }

        // Single notes of the note length, one after another until it's full
        const auto room = span / length;
        std::vector<music::Tone> sequence;

        if (type == music::ChordType::random)
        {
            sequence = getSequence (chord, measureChord);
            sequence.resize ((size_t) juce::jmin ((int) sequence.size(), room));
        }
        else
        {
            // An arpeggio goes up and down, or down and up, again and again, every note the note
            // length, until the measure's full.
            const auto cycle = getArpeggioCycle (chord.tones, type == music::ChordType::arpeggioUp);
            const auto period = (int) cycle.size();

            for (int i = 0; i < room && period > 0; ++i)
                sequence.push_back (cycle[(size_t) (i % period)]);
        }

        // Unless its notes have been moved by hand to others of the chord's
        if (auto moved = getMovedNotes (chord, measureChord, sequence.size()))
            sequence = std::move (*moved);

        auto slot = from;

        for (const auto& tone : sequence)
        {
            add (slot, { tone }, length, false);
            slot += length;
        }

        addRests (content.events, slot, end, beats);
        setNotesPerBeat (content);
        return content;
    }

    StaffContent getNotesContent (const Score& score, int part, Staff staff, int measure, const std::array<int, 7>& keyAlterations)
    {
        // Notes start on 64th notes: on a beat, or a number of 64ths through one.
        const auto beats = score.getBeatsPerMeasure();
        const auto slots = beats * Score::slotsPerBeat;
        const auto beatOf = [] (int slot) { return (double) slot / Score::slotsPerBeat; };
        StaffContent content;
        content.tuplets = score.getTuplets (part, staff, measure);

        // A tuplet's where it starts, and nothing else starts while it plays.
        std::array<const Tuplet*, Score::maxSlotsPerMeasure> tupletAt {};

        for (const auto& tuplet : content.tuplets)
            tupletAt[(size_t) std::lround (tuplet.beat * Score::slotsPerBeat)] = &tuplet;

        const auto restAt = [&] (int slot) { return score.getRestLength (part, staff, measure, beatOf (slot)); };

        const auto isEmpty = [&] (int slot)
        {
            return score.getNotes (part, staff, measure, beatOf (slot)).empty() && tupletAt[(size_t) slot] == nullptr && ! restAt (slot).has_value();
        };

        bool allEmpty = true;

        for (int slot = 0; slot < slots; ++slot)
            allEmpty = allEmpty && isEmpty (slot);

        // An empty measure gets a whole rest in any time signature.
        if (allEmpty)
        {
            content.events.push_back ({ 0.0, Duration::whole, {}, false, true });
            return content;
        }

        for (int slot = 0; slot < slots;)
        {
            const auto onset = beatOf (slot);

            // A tuplet's notes, each with its notes' written length, played as long as its share of the tuplet
            if (const auto* tuplet = tupletAt[(size_t) slot])
            {
                for (int i = 0; i < tuplet->getNumNotes(); ++i)
                {
                    StaffEvent event { tuplet->getNoteOnset (i), getDurationForBeats (tuplet->noteLength), {}, false, false, tuplet->staccato };
                    event.tones.push_back (makeTone (tuplet->pitches[(size_t) i], keyAlterations));
                    event.tupletStart = tuplet->beat;
                    event.tupletNotes = tuplet->getNumNotes();
                    content.events.push_back (event);
                }

                slot += (int) std::lround (tuplet->getLength() * Score::slotsPerBeat);
                continue;
            }

            // A rest written in, as long as it was, or as fits: a whole rest in the middle of the measure, if it fills it
            if (const auto rest = restAt (slot); rest.has_value() && score.getNotes (part, staff, measure, onset).empty())
            {
                const auto length = Score::fitNoteLength ((int) std::lround (*rest * Score::slotsPerBeat), slots - slot);
                const auto fillsMeasure = slot == 0 && length == slots;
                StaffEvent event { onset, fillsMeasure ? Duration::whole : getDurationForBeats (beatOf (length)), {}, false, fillsMeasure };
                event.writtenRest = true;
                content.events.push_back (event);
                slot += length;
                continue;
            }

            if (! isEmpty (slot))
            {
                // As long as they were added, or as long as fits in what's left of the measure. Any
                // notes they cover, which only a damaged file could have, aren't shown.
                const auto added = (int) std::lround (score.getNoteLength (part, staff, measure, onset) * Score::slotsPerBeat);
                const auto length = Score::fitNoteLength (added, slots - slot);
                StaffEvent event { onset, getDurationForBeats (beatOf (length)), {}, false, false,
                                   score.isStaccato (part, staff, measure, onset) };

                for (const auto& pitch : score.getNotes (part, staff, measure, onset))
                {
                    event.tones.push_back (makeTone (pitch, keyAlterations));

                    if (score.isTied (part, staff, measure, onset, pitch))
                        content.tiedNotes.push_back ({ onset, pitch });
                }

                content.events.push_back (event);
                slot += length;
                continue;
            }

            // Rests up to the next notes, or the end of the measure
            auto end = slot + 1;

            while (end < slots && isEmpty (end))
                ++end;

            addRests (content.events, slot, end, beats);
            slot = end;
        }

        // Eighths and shorter notes need room, and are beamed a beat at a time.
        setNotesPerBeat (content);
        return content;
    }
}

Duration getDurationForBeats (double beats)
{
    return beats >= 4.0 ? Duration::whole
         : beats >= 3.0 ? Duration::dottedHalf
         : beats >= 2.0 ? Duration::half
         : beats >= 1.5 ? Duration::dottedQuarter
         : beats >= 1.0 ? Duration::quarter
         : beats >= 0.75 ? Duration::dottedEighth
         : beats >= 0.5 ? Duration::eighth
         : beats >= 0.25 ? Duration::sixteenth
         : beats >= 0.125 ? Duration::thirtySecond
                          : Duration::sixtyFourth;
}

double StaffEvent::getPlayedBeats() const noexcept
{
    return tupletStart.has_value() && tupletNotes > 0 ? getBeats (duration) * Tuplet::getNormalCount (tupletNotes) / tupletNotes
                                                      : getBeats (duration);
}

double getBeats (Duration duration)
{
    switch (duration)
    {
        case Duration::whole:       return 4.0;
        case Duration::dottedHalf:  return 3.0;
        case Duration::half:        return 2.0;
        case Duration::dottedQuarter: return 1.5;
        case Duration::quarter:     return 1.0;
        case Duration::dottedEighth: return 0.75;
        case Duration::eighth:      return 0.5;
        case Duration::sixteenth:   return 0.25;
        case Duration::thirtySecond: return 0.125;
        case Duration::sixtyFourth: return 0.0625;
    }

    return 1.0;
}

bool isDotted (Duration duration)
{
    return duration == Duration::dottedHalf || duration == Duration::dottedQuarter || duration == Duration::dottedEighth;
}

Duration getFullMeasureDuration (int beatsPerMeasure)
{
    return beatsPerMeasure == 4 ? Duration::whole
         : beatsPerMeasure == 3 ? Duration::dottedHalf
                                : Duration::half;
}

namespace
{
    /** Whether a chord's alternate staff notes are held for its measure, rather than going as the
        other staff's notes of a rhythm with notes on both
    */
    bool holdsAlternate (const MeasureChord& chord)
    {
        return ! (chord.style.rhythm.has_value() && chord.style.rhythm->hasNotes (Staff::treble) && chord.style.rhythm->hasNotes (Staff::bass));
    }

    std::vector<int> getMidiNotes (const std::vector<music::Tone>& tones)
    {
        std::vector<int> midiNotes;

        for (const auto& tone : tones)
            midiNotes.push_back (tone.midi);

        return midiNotes;
    }

    /** Whether a measure's root or octave on the alternate staff is held on into the next: the next
        has the same chord, the same numeral, however it's written, or the same notes set on the
        piano, for one without a numeral, with the same notes on the alternate staff, both held
        for their chords. For a measure with two chords, it's its second half's that's held on.
    */
    bool isAlternateTiedOver (const Score& score, int part, int measure, int index, const MeasureChord& chord)
    {
        const auto alternate = score.getAlternateStaff (part);

        if ((alternate != music::AlternateStaff::root && alternate != music::AlternateStaff::octave) || measure + 1 >= score.getNumMeasures())
            return false;

        const auto* next = score.getChord (part, measure + 1);

        // On the same staff, so the notes are on the same alternate staff
        if (next == nullptr || ! next->hasNotes() || next->style.staff != chord.style.staff || ! holdsAlternate (chord) || ! holdsAlternate (*next))
            return false;

        const auto sameChord = ! chord.spec.isEmpty() ? next->spec == chord.spec : next->keyboardNotes == chord.keyboardNotes;
        const auto tones = getMidiNotes (score.getAlternateTones (part, measure, index));

        return sameChord && ! tones.empty() && tones == getMidiNotes (score.getAlternateTones (part, measure + 1));
    }

    /** Ties the notes of a staff's last note or chord, if it lasts to the end of the measure */
    void tieOver (StaffContent& content, int beatsPerMeasure)
    {
        if (content.events.empty() || content.events.back().isRest())
            return;

        const auto& last = content.events.back();

        if (last.onset + last.getPlayedBeats() < (double) beatsPerMeasure - 1.0e-9)
            return;

        for (const auto& tone : last.tones)
            content.tiedNotes.push_back ({ last.onset, tone.pitch });
    }
}

const MeasureContent::ChordPlayed* MeasureContent::getChordAt (double beat) const
{
    for (const auto& played : chords)
        if (beat >= played.onset - 1.0e-9 && beat < played.onset + played.beats - 1.0e-9)
            return &played;

    return nullptr;
}

MeasureContent getMeasureContent (const Score& score, int part, int measure)
{
    MeasureContent content;
    std::array<bool, 2> filled { false, false };
    const auto* firstChord = score.getChord (part, measure);
    const auto numChords = score.hasTwoChords (part, measure) ? 2 : 1;
    const auto measureLength = score.getBeatsPerMeasure() * Score::slotsPerBeat;
    const auto beatsPerMeasure = score.getBeatsPerMeasure();

    // The measure's chord, or its two halves', each half's on its own, each on its own staff,
    // with rests in a half without notes
    std::array<std::optional<music::Chord>, 2> notes;

    for (int index = 0; index < numChords; ++index)
        notes[(size_t) index] = score.getChordNotes (part, measure, index);

    if (firstChord != nullptr && (notes[0].has_value() || notes[1].has_value()))
    {
        for (auto staff : { Staff::treble, Staff::bass })
            filled[(size_t) staff] = score.chordUsesStaff (part, measure, staff);

        // Each half's chord is on its staff; the other has the alternate staff's notes, or rests.
        // A half without notes counts as the other half's for which staff it has.
        const auto staffOf = [&] (int index)
        {
            const auto own = notes[(size_t) index].has_value() ? index : 1 - index;
            return score.getChord (part, measure, own)->style.staff;
        };

        for (auto staff : { Staff::treble, Staff::bass })
        {
            const auto sourceIn = [&] (int index) { return staffOf (index) == staff ? StaffContent::Source::chord : StaffContent::Source::alternate; };
            auto& staffContent = content.staves[(size_t) staff];
            staffContent.source = sourceIn (0);

            if (numChords == 2 && sourceIn (1) != staffContent.source)
                staffContent.laterSource = std::pair ((double) score.getChordStart (1) / Score::slotsPerBeat, sourceIn (1));
        }

        for (int index = 0; index < numChords; ++index)
        {
            const auto& measureChord = *score.getChord (part, measure, index);
            const auto from = score.getChordStart (index);
            const auto span = numChords == 2 ? measureLength / 2 : measureLength;
            const auto onset = (double) from / Score::slotsPerBeat;
            const auto beats = (double) span / Score::slotsPerBeat;

            if (! notes[(size_t) index].has_value())
            {
                // A half without a chord is rests, on both staves.
                for (auto staff : { Staff::treble, Staff::bass })
                    if (filled[(size_t) staff])
                        addRests (content.staves[(size_t) staff].events, from, from + span, beatsPerMeasure);

                continue;
            }

            const auto chordStaff = measureChord.style.staff;
            const auto otherStaff = music::getOtherStaff (chordStaff);
            auto& written = content.staves[(size_t) chordStaff];
            auto& alternate = content.staves[(size_t) otherStaff];
            const auto& chord = *notes[(size_t) index];
            content.chords.push_back ({ chord, measureChord.style, onset, beats, index });

            const auto chordContent = getChordContent (score, chord, measureChord, from, span);
            written.events.insert (written.events.end(), chordContent.events.begin(), chordContent.events.end());
            written.tiedNotes.insert (written.tiedNotes.end(), chordContent.tiedNotes.begin(), chordContent.tiedNotes.end());

            if (! filled[(size_t) otherStaff])
                continue;

            // Held for the whole chord, from its start. With all its notes taken out by hand, it's
            // a rest, rather than the quarter notes it hides: a whole-measure rest for a chord
            // lasting the measure. Without an alternate staff, it's rests, where the other half's
            // chord has that staff.
            // In a rhythm with notes on both staves, the other staff's go as its notes do.
            const auto& rhythm = measureChord.style.rhythm;
            const auto rolled = score.getAlternateStaff (part) == music::AlternateStaff::rolledChord;
            auto tones = score.getAlternateTones (part, measure, index);

            if (! tones.empty() && rhythm.has_value() && rhythm->hasNotes (otherStaff) && rhythm->hasNotes (chordStaff))
            {
                alternate = getRhythmContent (score, layOutRhythm (rhythm->staves[(size_t) otherStaff], measureLength, measureChord.rhythmMeasure),
                                              StaffContent::Source::alternate, [&] (int) { return tones; }, rolled);
            }
            else if (! tones.empty())
            {
                alternate.events.push_back ({ onset, numChords == 2 ? getDurationForBeats (beats) : getFullMeasureDuration (beatsPerMeasure),
                                              std::move (tones), rolled, false });
            }
            else if (numChords == 2)
            {
                addRests (alternate.events, from, from + span, beatsPerMeasure);
            }
            else
            {
                alternate.events.push_back ({ 0.0, Duration::whole, {}, false, true });
            }
        }

        // The chord's own content says how close its notes are; two halves need room for both.
        for (auto staff : { Staff::treble, Staff::bass })
            if (filled[(size_t) staff])
                setNotesPerBeat (content.staves[(size_t) staff]);

        // Held on into the next measure, the alternate staff's notes with it. A root or octave is
        // held on into the next measure too when it has the same chord, even if the chord's
        // played again.
        const auto lastIndex = numChords - 1;

        if (score.isChordTiedOver (part, measure))
        {
            for (auto staff : { Staff::treble, Staff::bass })
                if (filled[(size_t) staff])
                    tieOver (content.staves[(size_t) staff], beatsPerMeasure);
        }
        else if (const auto* last = score.getChord (part, measure, lastIndex);
                 last != nullptr && last->hasNotes() && filled[(size_t) music::getOtherStaff (last->style.staff)]
                 && isAlternateTiedOver (score, part, measure, lastIndex, *last))
        {
            tieOver (content.staves[(size_t) music::getOtherStaff (last->style.staff)], beatsPerMeasure);
        }

        // Held on from the measure before, a rolled chord isn't rolled again.
        if (measure > 0 && score.isChordTiedOver (part, measure - 1))
            for (auto& staffContent : content.staves)
                if (! staffContent.events.empty())
                    staffContent.events.front().rolled = false;
    }

    const auto keyAlterations = music::getKeyAlterations (score.getKey());

    for (auto staff : { Staff::treble, Staff::bass })
        if (! filled[(size_t) staff])
            content.staves[(size_t) staff] = getNotesContent (score, part, staff, measure, keyAlterations);

    return content;
}

std::vector<music::Tone> getChordNoteChoices (const Score& score, int part, int measure, int index)
{
    const auto* measureChord = score.getChord (part, measure, index);

    if (measureChord == nullptr || ! music::isMelodic (measureChord->style.type))
        return {};

    if (const auto chord = score.getChordNotes (part, measure, index))
        return getNoteChoices (*chord);

    return {};
}
