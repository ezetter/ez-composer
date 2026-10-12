#include "PageTabs.h"

namespace
{
    constexpr int firstTabLeft = 8;
    constexpr int tabGap = 2;
    constexpr int tabTop = 5;
    constexpr int addButtonSize = 22;
}

PageTabs::PageTabs()
{
    addButton.setLookAndFeel (&roundButtonLookAndFeel);
    addButton.setColour (juce::TextButton::textColourOffId, controls::accent);
    addButton.setTooltip ("Add a page, with blank staves for every instrument");
    addButton.setWantsKeyboardFocus (false);
    addButton.onClick = [this] { if (onAddPage != nullptr) onAddPage(); };
    addAndMakeVisible (addButton);

    setPages (1, 0);
}

PageTabs::~PageTabs()
{
    addButton.setLookAndFeel (nullptr);
}

void PageTabs::setPages (int numPages, int chosen)
{
    chosenPage = chosen;

    while (tabs.size() > numPages)
        tabs.removeLast();

    while (tabs.size() < numPages)
        addAndMakeVisible (tabs.add (std::make_unique<Tab> (*this, tabs.size())));

    for (auto* tab : tabs)
    {
        tab->setToggleState (tab->page == chosen, juce::dontSendNotification);
        tab->setTooltip (tab->page == chosen ? juce::String ("The page you're editing. Right-click to copy or delete it.")
                                             : "Edit page " + juce::String (tab->page + 1) + ". Right-click to copy or delete it.");
    }

    resized();
    repaint();
}

void PageTabs::paint (juce::Graphics& g)
{
    // A line along the bottom, where the tabs meet the ruler, but for under the chosen tab
    g.setColour (juce::Colours::black.withAlpha (0.15f));
    g.fillRect (0, getHeight() - 1, getWidth(), 1);

    if (auto* chosen = juce::isPositiveAndBelow (chosenPage, tabs.size()) ? tabs[chosenPage] : nullptr)
    {
        g.setColour (chosenColour);
        g.fillRect (chosen->getX() + 1, getHeight() - 1, chosen->getWidth() - 2, 1);
    }
}

void PageTabs::resized()
{
    // As wide as they can be, down to a minimum, leaving room for the +
    const auto room = getWidth() - firstTabLeft - addButtonSize - 16;
    const auto width = tabs.isEmpty() ? tabWidth
                                      : juce::jlimit (minTabWidth, tabWidth, (room - (tabs.size() - 1) * tabGap) / tabs.size());
    auto x = firstTabLeft;

    for (auto* tab : tabs)
    {
        tab->setBounds (x, tabTop, width, getHeight() - tabTop);
        x += width + tabGap;
    }

    addButton.setBounds (x + 6, (getHeight() - addButtonSize) / 2 + 1, addButtonSize, addButtonSize);
}

juce::PopupMenu PageTabs::createMenu (int page) const
{
    // Deleting asks first, so it ends with an ellipsis. The only page can't be deleted.
    juce::PopupMenu menu;
    menu.addItem (copyPageItem, "Copy Page " + juce::String (page + 1));
    menu.addItem (deletePageItem, "Delete Page " + juce::String (page + 1) + juce::String (juce::CharPointer_UTF8 ("\xe2\x80\xa6")), tabs.size() > 1);
    return menu;
}

void PageTabs::showMenu (int page, juce::Component& tab)
{
    createMenu (page).showMenuAsync (juce::PopupMenu::Options().withTargetComponent (tab),
                                     [safeThis = juce::Component::SafePointer (this), page] (int result)
                                     {
                                         if (safeThis == nullptr)
                                             return;

                                         if (result == copyPageItem && safeThis->onCopyPage != nullptr)
                                             safeThis->onCopyPage (page);
                                         else if (result == deletePageItem && safeThis->onDeletePage != nullptr)
                                             safeThis->onDeletePage (page);
                                     });
}

//==============================================================================
PageTabs::Tab::Tab (PageTabs& ownerToUse, int pageToShow)
    : juce::Button ("Page " + juce::String (pageToShow + 1)),
      owner (ownerToUse),
      page (pageToShow)
{
    setWantsKeyboardFocus (false);
    onClick = [this] { if (owner.onPageChosen != nullptr) owner.onPageChosen (page); };
}

void PageTabs::Tab::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        owner.showMenu (page, *this);
        return;
    }

    juce::Button::mouseDown (e);
}

void PageTabs::Tab::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    // The chosen tab's the colour of the ruler it's joined to; the others are greyer.
    const auto chosen = getToggleState();
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f, 0.0f).withTrimmedTop (0.5f);
    juce::Path shape;
    shape.addRoundedRectangle (bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight() + 6.0f, 6.0f, 6.0f, true, true, false, false);

    g.saveState();
    g.reduceClipRegion (getLocalBounds());
    g.setColour (chosen ? chosenColour : down ? juce::Colour (0xffd9d9de) : highlighted ? juce::Colour (0xffe8e8ec) : juce::Colour (0xffe1e1e6));
    g.fillPath (shape);
    g.setColour (juce::Colours::black.withAlpha (chosen ? 0.15f : 0.08f));
    g.strokePath (shape, juce::PathStrokeType (1.0f));
    g.restoreState();

    g.setColour (chosen ? controls::accent : controls::secondaryText);
    g.setFont (juce::FontOptions (13.0f, chosen ? juce::Font::bold : juce::Font::plain));
    g.drawText (getName(), getLocalBounds().withTrimmedTop (1), juce::Justification::centred, true);
}
