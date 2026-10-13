#include "PageTabs.h"

namespace
{
    constexpr int firstTabLeft = 8;
    constexpr int tabGap = 2;
    constexpr int tabTop = 5;
    constexpr int addButtonSize = 22;
    constexpr int dragThreshold = 4;        // how far a tab has to be dragged before it moves, rather than being clicked
    constexpr int namePadding = 10;         // either side of a tab's name
    constexpr int minEditorWidth = 120;     // the box for typing a name, wide enough to type in over a narrow tab

    juce::Font getTabFont (bool chosen)
    {
        return juce::FontOptions (13.0f, chosen ? juce::Font::bold : juce::Font::plain);
    }
}

PageTabs::PageTabs()
{
    addButton.setLookAndFeel (&roundButtonLookAndFeel);
    addButton.setColour (juce::TextButton::textColourOffId, controls::accent);
    addButton.setTooltip ("Add a page, with blank staves for every instrument");
    addButton.setWantsKeyboardFocus (false);
    addButton.onClick = [this] { if (onAddPage != nullptr) onAddPage(); };
    addAndMakeVisible (addButton);

    // The box for typing a page's name, which Return or a click anywhere else keeps, and Esc doesn't
    nameEditor.setFont (getTabFont (true));
    nameEditor.setJustification (juce::Justification::centred);
    nameEditor.setInputRestrictions (Score::maxNameLength);
    nameEditor.setColour (juce::TextEditor::outlineColourId, controls::accent);
    nameEditor.setColour (juce::TextEditor::focusedOutlineColourId, controls::accent);
    nameEditor.setTooltip ("The page's name: Return keeps it, and Esc leaves it as it was");
    nameEditor.onReturnKey = [this] { finishRenaming (true); };
    nameEditor.onEscapeKey = [this] { finishRenaming (false); };
    nameEditor.onFocusLost = [this] { finishRenaming (true); };
    addChildComponent (nameEditor);

    setPages ({ "Page 1" }, 0);
}

PageTabs::~PageTabs()
{
    juce::Desktop::getInstance().removeGlobalMouseListener (&clickListener);
    addButton.setLookAndFeel (nullptr);
}

void PageTabs::setPages (const juce::StringArray& names, int chosen)
{
    chosenPage = chosen;

    // A page being renamed that's gone isn't renamed.
    if (renamingPage.has_value() && *renamingPage >= names.size())
        finishRenaming (false);

    while (tabs.size() > names.size())
        tabs.removeLast();

    while (tabs.size() < names.size())
        addAndMakeVisible (tabs.add (std::make_unique<Tab> (*this, tabs.size())));

    // Each named, and with a tip saying what it can do, starting with its name in full, which its
    // tab may cut short: as much as fits in a tip's two lines, however long the name.
    const auto fitTip = [] (std::initializer_list<juce::String> tips)
    {
        for (const auto& tip : tips)
            if (controls::TooltipLookAndFeel::layOut (tip).getNumLines() <= 2)
                return tip;

        return *std::prev (tips.end());
    };

    const auto actions = names.size() > 1 ? juce::String ("drag to move it, or right-click to rename, copy or delete it.")
                                          : juce::String ("right-click to rename or copy it.");
    widestName = 0;

    for (auto* tab : tabs)
    {
        const auto& name = names[tab->page];
        tab->setName (name);
        tab->setButtonText (name);
        tab->setToggleState (tab->page == chosen, juce::dontSendNotification);
        tab->setTooltip (tab->page == chosen ? fitTip ({ name + ", being edited: " + actions, name + ", being edited", name })
                                             : fitTip ({ name + ": click to edit it, " + actions, name + ": click to edit it", name }));
        widestName = juce::jmax (widestName, (int) std::ceil (juce::GlyphArrangement::getStringWidth (getTabFont (true), name)));
    }

    nameEditor.toFront (false);
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
    // Wide enough for the widest name, up to a point, or as wide as they can be, down to a
    // minimum, leaving room for the +
    const auto wanted = juce::jlimit (tabWidth, maxTabWidth, widestName + 2 * namePadding);
    const auto room = getWidth() - firstTabLeft - addButtonSize - 16;
    return tabs.isEmpty() ? wanted : juce::jlimit (minTabWidth, wanted, (room - (tabs.size() - 1) * tabGap) / tabs.size());
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

    layOutNameEditor();
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

juce::PopupMenu PageTabs::createMenu() const
{
    // Renaming and deleting ask for more, so they end with an ellipsis. The only page can't be deleted.
    const auto ellipsis = juce::String (juce::CharPointer_UTF8 ("\xe2\x80\xa6"));
    juce::PopupMenu menu;
    menu.addItem (renamePageItem, "Rename Page" + ellipsis);
    menu.addItem (copyPageItem, "Copy Page");
    menu.addSeparator();
    menu.addItem (deletePageItem, "Delete Page" + ellipsis, tabs.size() > 1);
    return menu;
}

void PageTabs::showMenu (int page, juce::Component& tab)
{
    createMenu().showMenuAsync (juce::PopupMenu::Options().withTargetComponent (tab),
                                [safeThis = juce::Component::SafePointer (this), page] (int result)
                                {
                                    if (safeThis == nullptr)
                                        return;

                                    if (result == renamePageItem)
                                        safeThis->startRenaming (page);
                                    else if (result == copyPageItem && safeThis->onCopyPage != nullptr)
                                        safeThis->onCopyPage (page);
                                    else if (result == deletePageItem && safeThis->onDeletePage != nullptr)
                                        safeThis->onDeletePage (page);
                                });
}

//==============================================================================
void PageTabs::startRenaming (int page)
{
    if (! juce::isPositiveAndBelow (page, tabs.size()))
        return;

    // Another page being renamed keeps the name typed for it.
    finishRenaming (true);
    renamingPage = page;
    nameEditor.setText (tabs[page]->getButtonText(), juce::dontSendNotification);

    // Emptied, it shows the name the page has without one: its number.
    nameEditor.setTextToShowWhenEmpty ("Page " + juce::String (page + 1), controls::secondaryText);
    layOutNameEditor();
    nameEditor.setVisible (true);
    nameEditor.toFront (false);

    if (nameEditor.isShowing())
        nameEditor.grabKeyboardFocus();

    nameEditor.selectAll();
    juce::Desktop::getInstance().addGlobalMouseListener (&clickListener);
}

void PageTabs::finishRenaming (bool rename)
{
    if (! renamingPage.has_value())
        return;

    const auto page = *renamingPage;
    renamingPage.reset();
    juce::Desktop::getInstance().removeGlobalMouseListener (&clickListener);

    const auto name = nameEditor.getText();
    nameEditor.setVisible (false);

    if (rename && onRenamePage != nullptr)
        onRenamePage (page, name);
}

void PageTabs::layOutNameEditor()
{
    if (! renamingPage.has_value() || ! juce::isPositiveAndBelow (*renamingPage, tabs.size()))
        return;

    // Over its tab, and wide enough to type in, even over a narrow one
    const auto tab = tabs[*renamingPage]->getBounds();
    nameEditor.setBounds (juce::Rectangle<int> (juce::jmax (tab.getWidth(), minEditorWidth), tab.getHeight() - 3)
                              .withCentre (tab.getCentre().translated (0, -1))
                              .constrainedWithin (getLocalBounds()));
}

void PageTabs::ClickListener::mouseDown (const juce::MouseEvent& e)
{
    if (e.originalComponent != &owner.nameEditor && ! owner.nameEditor.isParentOf (e.originalComponent))
        owner.finishRenaming (true);
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

    // Its name, shortened with an ellipsis if it doesn't fit
    g.setColour (chosen ? controls::accent : controls::secondaryText);
    g.setFont (getTabFont (chosen));
    g.drawText (getButtonText(), getLocalBounds().withTrimmedTop (1).reduced (namePadding / 2, 0), juce::Justification::centred, true);
}
