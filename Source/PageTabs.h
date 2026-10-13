#pragma once

#include "Controls.h"
#include "Score.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <optional>

/** A tab for each of the piece's pages, along the top of the staves, the chosen one joined to
    the ruler under it, with a + after them for adding a page. Clicking a tab chooses its page,
    right-clicking it offers to rename the page, copy it or take it out, and dragging it along
    moves the page to another place among them, the other tabs making room for it as it goes.
    The tabs are as wide as the longest name needs, up to a point, unless they're crowded.
*/
class PageTabs final : public juce::Component
{
public:
    PageTabs();
    ~PageTabs() override;

    /** Shows a tab for each page, with its name, and one chosen. */
    void setPages (const juce::StringArray& names, int chosen);

    /** Called when a tab's clicked, with its page, counting from 0. */
    std::function<void (int page)> onPageChosen;

    /** Called when the + is clicked. */
    std::function<void()> onAddPage;

    /** Called when Copy Page or Delete Page is chosen from a tab's menu. */
    std::function<void (int page)> onCopyPage, onDeletePage;

    /** Called when a tab's been dragged to another place, with its page and the place it goes
        to, counting places as they are once it's moved.
    */
    std::function<void (int from, int to)> onMovePage;

    /** Called when a page's been given a new name on its tab, with the name typed. */
    std::function<void (int page, const juce::String& name)> onRenamePage;

    /** Shows a box over a page's tab, with its name selected, for typing a new one, as Rename
        Page in the tab's menu does. Return, or a click anywhere else, gives the page the name
        typed, and Esc leaves it as it was.
    */
    void startRenaming (int page);

    /** Whether a page's name is being typed. */
    bool isRenaming() const noexcept { return renamingPage.has_value(); }

    /** The menu right-clicking a page's tab shows. */
    juce::PopupMenu createMenu() const;

    static constexpr int copyPageItem = 1, deletePageItem = 2, renamePageItem = 3;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int height = 30;
    static constexpr int tabWidth = 84, minTabWidth = 52, maxTabWidth = 160;

    /** The colour of the chosen tab, and the ruler it's joined to */
    inline static const juce::Colour chosenColour { 0xfff7f7f9 };

private:
    /** A page's tab, with rounded corners at the top. */
    struct Tab final : public juce::Button
    {
        Tab (PageTabs&, int page);
        void paintButton (juce::Graphics&, bool highlighted, bool down) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;

        PageTabs& owner;
        const int page;
    };

    void showMenu (int page, juce::Component& tab);

    /** Gives the page being renamed the name typed, or leaves it as it was. */
    void finishRenaming (bool rename);
    void layOutNameEditor();

    /** While a page is being renamed, hears every click, as one away from its box gives it the name typed. */
    struct ClickListener final : public juce::MouseListener
    {
        explicit ClickListener (PageTabs& ownerToUse) : owner (ownerToUse) {}
        void mouseDown (const juce::MouseEvent&) override;

        PageTabs& owner;
    };

    /** How wide the tabs are, and where the one at a place goes, across. */
    int getTabWidth() const;
    int getTabX (int place) const;

    /** Dragging a tab: the mouse going down on it, moving with its button down, and being let go.
        Letting go says whether it moved far enough to be a drag rather than a click.
    */
    void startDragging (int page, juce::Point<int> mouse);
    void dragTo (juce::Point<int> mouse);
    bool finishDragging();

    /** Lays the tabs out with the one being dragged under the mouse, and the others making room
        for it where it would go.
    */
    void layOutDragging();

    juce::OwnedArray<Tab> tabs;
    int chosenPage = 0;
    std::optional<int> draggedPage;     // the page whose tab the mouse went down on
    int dragStartX = 0, dragOffset = 0; // where the mouse went down, and how far into the tab
    int dragX = 0;                      // where the dragged tab is, across
    int dragPlace = 0;                  // where it would go
    bool dragMoved = false;             // whether it's moved far enough to be a drag
    bool ignoringClick = false;         // as a dragged tab is let go
    int widestName = 0;                 // how wide the widest name is, which the tabs are made wide enough for
    controls::RoundButtonLookAndFeel roundButtonLookAndFeel;
    juce::TextButton addButton { "+" };
    juce::TextEditor nameEditor;        // over a tab, while its page's name is typed
    std::optional<int> renamingPage;
    ClickListener clickListener { *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PageTabs)
};
