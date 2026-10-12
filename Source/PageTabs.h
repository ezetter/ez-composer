#pragma once

#include "Controls.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

/** A tab for each of the piece's pages, along the top of the staves, the chosen one joined to
    the ruler under it, with a + after them for adding a page. Clicking a tab chooses its page,
    and right-clicking it offers to take the page out.
*/
class PageTabs final : public juce::Component
{
public:
    PageTabs();
    ~PageTabs() override;

    /** Shows a tab for each page, with one chosen. */
    void setPages (int numPages, int chosen);

    /** Called when a tab's clicked, with its page, counting from 0. */
    std::function<void (int page)> onPageChosen;

    /** Called when the + is clicked. */
    std::function<void()> onAddPage;

    /** Called when Delete Page is chosen from a tab's menu. */
    std::function<void (int page)> onDeletePage;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int height = 30;
    static constexpr int tabWidth = 84, minTabWidth = 52;

    /** The colour of the chosen tab, and the ruler it's joined to */
    inline static const juce::Colour chosenColour { 0xfff7f7f9 };

private:
    /** A page's tab, with rounded corners at the top. */
    struct Tab final : public juce::Button
    {
        Tab (PageTabs&, int page);
        void paintButton (juce::Graphics&, bool highlighted, bool down) override;
        void mouseDown (const juce::MouseEvent&) override;

        PageTabs& owner;
        const int page;
    };

    void showMenu (int page, juce::Component& tab);

    juce::OwnedArray<Tab> tabs;
    int chosenPage = 0;
    controls::RoundButtonLookAndFeel roundButtonLookAndFeel;
    juce::TextButton addButton { "+" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PageTabs)
};
