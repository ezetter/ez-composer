#pragma once

#include "Score.h"

#include <functional>
#include <memory>
#include <optional>
#include <vector>

/** A piece of music in pages, each a score of its own, with its own key, time signature,
    measures, repeats and music, all for the same instruments and at the same tempo.

    One page at a time is edited, in the editing score, which the staves, the sidebar and the
    rest of the app show. The other pages are kept in scores of their own. Choosing another page
    keeps the editing score's music as its page's, and puts the chosen page's music in it.

    The parts, by their ids, and the tempo are the piece's, the same on every page: adding or
    taking out a part in the editing score does the same on every other page, and its tempo is
    every page's.

    Listeners hear about every change to any page, and to the pages themselves, but not about
    choosing another page to edit, which isn't a change to the piece; onPagesChanged says so.
*/
class Piece final : public juce::ChangeBroadcaster,
                    private juce::ChangeListener
{
public:
    explicit Piece (Score& editing);
    ~Piece() override;

    /** The score the page being edited is in. */
    Score& getEditingScore() noexcept { return editing; }

    int getNumPages() const noexcept { return (int) pages.size(); }

    /** The page being edited, counting from 0. */
    int getCurrentPage() const noexcept { return current; }

    /** A page's score: the editing score for the page being edited, or the page's own. */
    Score& getPage (int);
    const Score& getPage (int) const;

    /** Chooses a page to edit, keeping the music of the one that was. */
    void choosePage (int);

    /** Adds a page after the last, with blank staves: as many empty measures as a new score has,
        for every part, in the key and time signature of the page being edited, and with its
        parts' alternate staffs. The new page is chosen to edit. Returns its number.
    */
    int addPage();

    /** Adds a copy of a page just after it, with all its music and settings, to be edited on its
        own. The copy is chosen to edit. Returns its number, or -1 if there's no such page.
    */
    int copyPage (int);

    /** Takes out a page, unless it's the only one. If it's the one being edited, the page after
        it is chosen, or the one before if it was the last.
    */
    void removePage (int);

    /** Moves a page to another place among them, counting places in the order they're in after
        it's moved. The page being edited stays the one edited, wherever it is now.
    */
    void movePage (int from, int to);

    /** Called after another page has been chosen to edit, or pages have been added or taken out. */
    std::function<void()> onPagesChanged;

    /** Whether a part has nothing in it, on any page. */
    bool isPartEmpty (int part) const;

    /** Starts again with one page, a new, empty score, with two parts. */
    void clear();

    //==============================================================================
    /** The whole piece, as JSON to save in a file: the first page as a score is saved, with the
        other pages, if there are any, in a list, "morePages", which an app from before there
        were pages leaves out, opening the first page.
    */
    juce::var toJSON (bool withPartIds = false) const;

    /** Replaces the piece with one saved by toJSON(), or a score saved before there were pages,
        as its only page. The first page is chosen to edit.
    */
    juce::Result loadJSON (const juce::var&);

    /** Each page as it is now, as JSON with the parts' ids, for remembering it to undo back to.
        A page that hasn't changed since the last time comes as the same text, sharing its memory.
    */
    std::vector<juce::String> capturePages() const;

    /** Puts the pages back as captured, choosing one of them to edit. */
    void restorePages (const std::vector<juce::String>&, int pageToEdit);

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    /** Gives the other pages the editing score's parts and tempo, if they've changed. */
    void matchOtherPages();

    /** Says the pages changed, once they're all changed together. */
    void pagesChanged (bool changedPiece);

    std::unique_ptr<Score> createPage();

    Score& editing;
    std::vector<std::unique_ptr<Score>> pages;      // every page's score; the edited page's is out of date, its music being in the editing score
    int current = 0;
    bool quiet = false;                             // while pages are changed together, listeners hear once, at the end
    mutable std::vector<std::optional<juce::String>> captured;     // each other page's JSON, while it hasn't changed

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Piece)
};
