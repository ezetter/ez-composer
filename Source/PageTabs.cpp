#include "PageTabs.h"

namespace
{
    constexpr int firstTabLeft = 8;
    constexpr int tabGap = 2;
    constexpr int tabTop = 5;
    constexpr int addButtonSize = 22;
    constexpr int dragThreshold = 4;        // how far a tab has to be dragged before it moves, rather than being clicked
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

int PageTabs::getTabWidth() const
{
    // As wide as they can be, down to a minimum, leaving room for the +
    const auto room = getWidth() - firstTabLeft - addButtonSize - 16;
    return tabs.isEmpty() ? tabWidth : juce::jlimit (minTabWidth, tabWidth, (room - (tabs.size() - 1) * tabGap) / tabs.size());
}

int PageTabs::getTabX (int place) const
{
    return firstTabLeft + place * (getTabWidth() + tabGap);
}

void PageTabs::resized()
{
    for (int place = 0; place < tabs.size(); ++place)
        tabs[place]->setBounds (getTabX (place), tabTop, getTabWidth(), getHeight() - tabTop);

    addButton.setBounds (getTabX (tabs.size()) + 6, (getHeight() - addButtonSize) / 2 + 1, addButtonSize, addButtonSize);

    if (draggedPage.has_value() && dragMoved)
        layOutDragging();
}

//==============================================================================
void PageTabs::startDragging (int page, juce::Point<int> mouse)
{
    draggedPage = page;
    dragStartX = mouse.x;
    dragOffset = mouse.x - getTabX (page);
    dragX = getTabX (page);
    dragPlace = page;
    dragMoved = false;
}

void PageTabs::dragTo (juce::Point<int> mouse)
{
    if (! draggedPage.has_value() || tabs.size() < 2 || (! dragMoved && std::abs (mouse.x - dragStartX) < dragThreshold))
        return;

    // Along the row of tabs, no further than the first place or the last
    dragMoved = true;
    dragX = juce::jlimit (getTabX (0), getTabX (tabs.size() - 1), mouse.x - dragOffset);
    dragPlace = juce::jlimit (0, tabs.size() - 1, juce::roundToInt ((float) (dragX - firstTabLeft) / (float) (getTabWidth() + tabGap)));
    layOutDragging();
}

void PageTabs::layOutDragging()
{
    // The other tabs in order, leaving a gap where the dragged one would go
    auto place = 0;

    for (int page = 0; page < tabs.size(); ++page)
    {
        if (page == *draggedPage)
            continue;

        if (place == dragPlace)
            ++place;

        tabs[page]->setTopLeftPosition (getTabX (place), tabTop);
        ++place;
    }

    auto& dragged = *tabs[*draggedPage];
    dragged.setTopLeftPosition (dragX, tabTop);
    dragged.toFront (false);
    dragged.setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    repaint();
}

bool PageTabs::finishDragging()
{
    const auto moved = draggedPage.has_value() && dragMoved;
    const auto from = draggedPage.value_or (0), to = dragPlace;

    if (draggedPage.has_value() && juce::isPositiveAndBelow (*draggedPage, tabs.size()))
        tabs[*draggedPage]->setMouseCursor (juce::MouseCursor::NormalCursor);

    draggedPage.reset();
    dragMoved = false;
    resized();
    repaint();

    // Once the mouse has been dealt with, as moving the page shows the tabs again
    if (moved && from != to)
        juce::MessageManager::callAsync ([safeThis = juce::Component::SafePointer (this), from, to]
        {
            if (safeThis != nullptr && safeThis->onMovePage != nullptr)
                safeThis->onMovePage (from, to);
        });

    return moved;
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

    // A tab that's been dragged isn't clicked, as it's let go.
    onClick = [this] { if (! owner.ignoringClick && owner.onPageChosen != nullptr) owner.onPageChosen (page); };
}

void PageTabs::Tab::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        owner.showMenu (page, *this);
        return;
    }

    owner.startDragging (page, e.getEventRelativeTo (&owner).getPosition());
    juce::Button::mouseDown (e);
}

void PageTabs::Tab::mouseDrag (const juce::MouseEvent& e)
{
    juce::Button::mouseDrag (e);

    if (! e.mods.isPopupMenu())
        owner.dragTo (e.getEventRelativeTo (&owner).getPosition());
}

void PageTabs::Tab::mouseUp (const juce::MouseEvent& e)
{
    // A drag isn't a click as well. The page is moved once the mouse has been dealt with.
    owner.ignoringClick = owner.finishDragging();
    juce::Button::mouseUp (e);
    owner.ignoringClick = false;
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
