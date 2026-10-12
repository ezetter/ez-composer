#pragma once

#include "Piece.h"

#include <juce_events/juce_events.h>

#include <functional>
#include <utility>
#include <vector>

/** Remembers how the piece was before each change, every page of it, so changes can be undone,
    and undone changes redone. Undoing or redoing a change on another page than the one being
    edited goes to that page, to show it.

    Everything that changes in the piece while handling one event, such as a click or a menu
    command, counts as one change, so it's undone in one go, and so does everything changed
    during a gesture, such as an eraser stroke. Choosing another page to edit isn't a change.
*/
class ScoreHistory final : private juce::ChangeListener,
                           private juce::AsyncUpdater
{
public:
    explicit ScoreHistory (Piece&);
    ~ScoreHistory() override;

    bool canUndo();
    bool canRedo();

    /** Puts the piece back as it was before the last change, or the one before that, and so on. */
    void undo();

    /** Makes the last change undone again, unless the score's been changed since. */
    void redo();

    /** Forgets every change, e.g. when a score's opened, so the piece as it is now is where undoing stops. */
    void clear();

    /** Changes made from here until endGesture() count as one, e.g. everything an eraser stroke
        takes out. Undo and Redo do nothing in the meantime.
    */
    void beginGesture();
    void endGesture();

    /** Called when what can be undone or redone changes. */
    std::function<void()> onChange;

    /** How many changes are remembered. The oldest are forgotten first. */
    static constexpr size_t maxChanges = 500;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void handleAsyncUpdate() override;
    /** The piece, as each of its pages' JSON */
    using State = std::vector<juce::String>;

    State capture() const;

    /** Puts the piece back as it was, going to the page that's changed from how it is. */
    void restore (const State& state, const State& from);
    void changed();

    Piece& piece;
    State current;                          // the piece as it was after the last change remembered
    std::vector<State> undoStates, redoStates;
    bool restoring = false;
    int gestures = 0;               // how many are under way

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScoreHistory)
};
