#include "ScoreHistory.h"

ScoreHistory::ScoreHistory (Piece& pieceToUse)
    : piece (pieceToUse),
      current (capture())
{
    piece.addChangeListener (this);
}

ScoreHistory::~ScoreHistory()
{
    piece.removeChangeListener (this);
}

bool ScoreHistory::canUndo()
{
    handleUpdateNowIfNeeded();
    return gestures == 0 && ! undoStates.empty();
}

bool ScoreHistory::canRedo()
{
    handleUpdateNowIfNeeded();
    return gestures == 0 && ! redoStates.empty();
}

void ScoreHistory::undo()
{
    // A change still waiting to be remembered is the one undone.
    handleUpdateNowIfNeeded();

    if (gestures > 0 || undoStates.empty())
        return;

    redoStates.push_back (current);
    current = undoStates.back();
    undoStates.pop_back();
    restore (current, redoStates.back());
    changed();
}

void ScoreHistory::redo()
{
    handleUpdateNowIfNeeded();

    if (gestures > 0 || redoStates.empty())
        return;

    undoStates.push_back (current);
    current = redoStates.back();
    redoStates.pop_back();
    restore (current, undoStates.back());
    changed();
}

void ScoreHistory::clear()
{
    cancelPendingUpdate();
    undoStates.clear();
    redoStates.clear();
    current = capture();
    changed();
}

void ScoreHistory::beginGesture()
{
    // What changed before it is a change of its own.
    handleUpdateNowIfNeeded();
    ++gestures;
}

void ScoreHistory::endGesture()
{
    jassert (gestures > 0);

    if (gestures > 0 && --gestures == 0)
    {
        cancelPendingUpdate();
        handleAsyncUpdate();
    }
}

void ScoreHistory::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // The changes made while handling this event are remembered together, once it's done.
    if (! restoring)
        triggerAsyncUpdate();
}

void ScoreHistory::handleAsyncUpdate()
{
    // A gesture's changes are remembered together when it ends.
    if (gestures > 0)
        return;

    auto now = capture();

    if (now == current)
        return;

    undoStates.push_back (std::exchange (current, std::move (now)));

    if (undoStates.size() > maxChanges)
        undoStates.erase (undoStates.begin());

    // A new change starts a new history from here, so what was undone can't be redone.
    redoStates.clear();
    changed();
}

ScoreHistory::State ScoreHistory::capture() const
{
    // With the parts' ids, so a part that's taken out and put back is known to be the same one.
    // A page that hasn't changed shares its text with the last time, so it takes no more memory.
    return piece.capturePages();
}

void ScoreHistory::restore (const State& state, const State& from)
{
    const juce::ScopedValueSetter<bool> setter (restoring, true);

    // The page being edited, if it's changed, or else the first page that has
    const auto differs = [&] (int page) { return (size_t) page >= from.size() || state[(size_t) page] != from[(size_t) page]; };
    auto pageToEdit = juce::jlimit (0, (int) state.size() - 1, piece.getCurrentPage());

    if (! differs (pageToEdit))
        for (int page = 0; page < (int) state.size(); ++page)
            if (differs (page))
            {
                pageToEdit = page;
                break;
            }

    piece.restorePages (state, pageToEdit);
}

void ScoreHistory::changed()
{
    if (onChange != nullptr)
        onChange();
}
