#include "Rhythms.h"

#include "MeasureContent.h"

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
juce::String SavedRhythm::describe() const
{
    const auto count = [] (size_t n) { return juce::String ((int) n) + (n == 1 ? " note" : " notes"); };
    const auto measures = juce::String (numMeasures) + (numMeasures == 1 ? " measure" : " measures") + " of " + juce::String (beatsPerMeasure) + "/4";
    const auto& treble = staves[(size_t) Staff::treble];
    const auto& bass = staves[(size_t) Staff::bass];

    if (! treble.empty() && ! bass.empty())
        return measures + ": " + count (treble.size()) + " on the treble staff, " + juce::String ((int) bass.size()) + " on the bass";

    return measures + ": " + count (treble.empty() ? bass.size() : treble.size()) + " on one staff";
}

juce::var SavedRhythm::toJSON() const
{
    auto* object = new juce::DynamicObject();
    object->setProperty ("name", name);
    object->setProperty ("beatsPerMeasure", beatsPerMeasure);
    object->setProperty ("measures", numMeasures);

    for (auto staff : { Staff::treble, Staff::bass })
    {
        juce::Array<juce::var> hits;

        for (const auto& hit : staves[(size_t) staff])
            hits.add (juce::Array<juce::var> { hit.onset, hit.length, hit.staccato, hit.tiedToNext });

        object->setProperty (staff == Staff::treble ? "treble" : "bass", hits);
    }

    return juce::var (object);
}

std::optional<SavedRhythm> SavedRhythm::fromJSON (const juce::var& json)
{
    if (! json.isObject())
        return {};

    SavedRhythm rhythm;
    rhythm.name = tidySavedName (json.getProperty ("name", {}).toString(), maxNameLength);
    rhythm.beatsPerMeasure = readInt (json.getProperty ("beatsPerMeasure", {}), 1, Score::maxBeatsPerMeasure, 4);
    rhythm.numMeasures = readInt (json.getProperty ("measures", {}), 1, 1000, 1);

    // Notes in order, each after the one before ends, within the rhythm, as long as a note can be
    for (auto staff : { Staff::treble, Staff::bass })
    {
        auto& hits = rhythm.staves[(size_t) staff];

        if (const auto* list = json.getProperty (staff == Staff::treble ? "treble" : "bass", {}).getArray())
        {
            for (const auto& item : *list)
            {
                const Hit hit { readInt (getListItem (item, 0), 0, rhythm.getLength() - 1, -1), readInt (getListItem (item, 1), 1, Score::maxSlotsPerMeasure, -1),
                                (bool) getListItem (item, 2), (bool) getListItem (item, 3) };

                if (hit.onset >= 0 && hit.length > 0 && hit.onset + hit.length <= rhythm.getLength()
                    && (hits.empty() || hits.back().onset + hits.back().length <= hit.onset))
                    hits.push_back (hit);
            }
        }
    }

    if (rhythm.name.isEmpty() || ! rhythm.hasNotes())
        return {};

    return rhythm;
}

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
