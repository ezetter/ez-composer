#include "Rhythms.h"

#include "MeasureContent.h"

namespace
{
    int toSlots (double beats)
    {
        return (int) std::lround (beats * Score::slotsPerBeat);
    }

    /** Whether a tie goes over the barline at the end of a measure, on either staff */
    bool isTiedOver (const MeasureContent& content, int slotsPerMeasure)
    {
        for (const auto& staff : content.staves)
            for (const auto& event : staff.events)
                if (! event.isRest() && toSlots (event.onset) + toSlots (getBeats (event.duration)) >= slotsPerMeasure)
                    for (const auto& [onset, pitch] : staff.tiedNotes)
                        if (juce::exactlyEqual (onset, event.onset))
                            return true;

        return false;
    }
}

//==============================================================================
//==============================================================================
SavedRhythm getRhythm (const Score& score, int part, int firstMeasure, int numMeasures)
{
    SavedRhythm rhythm;
    rhythm.beatsPerMeasure = score.getBeatsPerMeasure();
    rhythm.numMeasures = numMeasures;
    const auto slotsPerMeasure = score.getBeatsPerMeasure() * Score::slotsPerBeat;

    for (int i = 0; i < numMeasures && firstMeasure + i < score.getNumMeasures(); ++i)
    {
        const auto content = getMeasureContent (score, part, firstMeasure + i);

        for (auto staff : { Staff::treble, Staff::bass })
        {
            const auto& written = content.staves[(size_t) staff];

            for (const auto& event : written.events)
            {
                if (event.isRest())
                    continue;

                // Tied on, if any of its notes is
                const auto tied = std::any_of (written.tiedNotes.begin(), written.tiedNotes.end(),
                                               [&] (const auto& tie) { return juce::exactlyEqual (tie.first, event.onset); });
                rhythm.staves[(size_t) staff].push_back ({ i * slotsPerMeasure + toSlots (event.onset), toSlots (getBeats (event.duration)),
                                                          event.staccato, tied });
            }
        }
    }

    return rhythm;
}

std::vector<int> getRhythmLengthChoices (const Score& score, int part, int firstMeasure)
{
    std::vector<int> choices;
    const auto slotsPerMeasure = score.getBeatsPerMeasure() * Score::slotsPerBeat;
    auto anyNotes = false;

    for (auto measure = juce::jmax (0, firstMeasure); measure < score.getNumMeasures(); ++measure)
    {
        const auto content = getMeasureContent (score, part, measure);

        for (const auto& staff : content.staves)
            anyNotes = anyNotes || std::any_of (staff.events.begin(), staff.events.end(), [] (const StaffEvent& e) { return ! e.isRest(); });

        if (anyNotes && ! isTiedOver (content, slotsPerMeasure))
            choices.push_back (measure - firstMeasure + 1);
    }

    return choices;
}

RhythmPlan planRhythm (const SavedRhythm& rhythm, int times, const Note& start, int beatsPerMeasure)
{
    RhythmPlan plan;
    const auto slotsPerMeasure = beatsPerMeasure * Score::slotsPerBeat;
    const auto origin = start.measure * slotsPerMeasure + toSlots (start.beat);
    const auto bothStaves = rhythm.hasNotes (Staff::treble) && rhythm.hasNotes (Staff::bass);
    const auto length = rhythm.getLength();
    times = juce::jmax (1, times);

    for (auto from : { Staff::treble, Staff::bass })
    {
        const auto& hits = rhythm.staves[(size_t) from];

        if (hits.empty())
            continue;

        // On the note's staff, at its pitch, or on the other one, an octave from it
        const auto staff = bothStaves ? from : start.staff;
        const auto pitch = staff == start.staff ? start.pitch
                                                : music::Pitch { start.pitch.step + (staff == Staff::treble ? 7 : -7), start.pitch.alter };
        plan.spans[(size_t) staff] = juce::Range<int> (origin, origin + times * length);

        for (int time = 0; time < times; ++time)
        {
            for (const auto& hit : hits)
            {
                // In notes that fit, as long as they can be, tied together
                auto at = origin + time * length + hit.onset;
                auto left = hit.length;

                while (left > 0)
                {
                    const auto slot = at % slotsPerMeasure;
                    const auto piece = Score::fitNoteLength (left, slotsPerMeasure - slot);
                    const auto last = piece >= left;
                    plan.notes.push_back ({ { staff, at / slotsPerMeasure, (double) slot / Score::slotsPerBeat, pitch, start.part,
                                              (double) piece / Score::slotsPerBeat, last && hit.staccato },
                                            ! last || hit.tiedToNext });
                    at += piece;
                    left -= piece;
                }
            }
        }
    }

    return plan;
}
