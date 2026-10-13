#include "EffectsWindow.h"

namespace
{
    constexpr int padding = 16;
    constexpr int headingHeight = 18;
    constexpr int rowHeight = 30;
    constexpr int boxWidth = 178, boxHeight = 62, boxGap = 26;      // the gap has an arrow in it
    constexpr int cardTitleHeight = 34;

    juce::String withEllipsis (const juce::String& text)
    {
        return text + juce::String (juce::CharPointer_UTF8 ("\xe2\x80\xa6"));
    }

    /** Main output, then the buses, as combo box items: 1 for the main output, a bus's id + 1 for it. */
    void fillOutputs (juce::ComboBox& box, const InstrumentHost& host, int firstBusPlace, int current)
    {
        box.clear (juce::dontSendNotification);
        box.addItem ("Main output", 1);
        const auto buses = host.getBuses();

        for (auto place = firstBusPlace; place < (int) buses.size(); ++place)
            box.addItem ("Bus " + juce::String (place + 1), buses[(size_t) place] + 1);

        box.setSelectedId (current + 1, juce::dontSendNotification);
    }

    void drawArrow (juce::Graphics& g, float fromX, float toX, float y)
    {
        g.setColour (controls::secondaryText);
        g.drawLine (fromX, y, toX - 4.0f, y, 1.4f);
        juce::Path head;
        head.addTriangle (toX, y, toX - 6.0f, y - 4.0f, toX - 6.0f, y + 4.0f);
        g.fillPath (head);
    }
}

//==============================================================================
EffectsPanel::EffectBox::EffectBox (EffectRack& rackToUse, int busId, int index)
{
    auto& host = rackToUse.getHost();
    auto* effect = host.getEffect (busId, index);
    bypassed = host.isEffectBypassed (busId, index);

    name.setText (effect != nullptr ? effect->getName() : juce::String(), juce::dontSendNotification);
    name.setFont (juce::FontOptions (13.5f, juce::Font::bold));
    name.setMinimumHorizontalScale (0.7f);
    addAndMakeVisible (name);

    removeButton.setButtonText (juce::String (juce::CharPointer_UTF8 ("\xe2\x9c\x95")));
    editButton.setTooltip ("Open its own window, to change its settings");
    bypassButton.setTooltip ("Pass over it, as if it weren't there");
    removeButton.setTooltip ("Take it out of the chain");

    bypassButton.setClickingTogglesState (true);
    bypassButton.setToggleState (bypassed, juce::dontSendNotification);
    bypassButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xfffbe9c9));
    bypassButton.setColour (juce::TextButton::textColourOnId, juce::Colour (0xff8a5a00));
    earlierButton.setEnabled (index > 0);
    laterButton.setEnabled (index < host.getNumEffects (busId) - 1);
    earlierButton.setTooltip (earlierButton.isEnabled() ? "Move it earlier in the chain" : "It's first in the chain already");
    laterButton.setTooltip (laterButton.isEnabled() ? "Move it later in the chain" : "It's last in the chain already");

    editButton.onClick = [&rackToUse, busId, index] { rackToUse.showEditor (busId, index); };
    bypassButton.onClick = [&rackToUse, busId, index, this] { rackToUse.setEffectBypassed (busId, index, bypassButton.getToggleState()); };
    earlierButton.onClick = [&rackToUse, busId, index] { rackToUse.moveEffect (busId, index, index - 1); };
    laterButton.onClick = [&rackToUse, busId, index] { rackToUse.moveEffect (busId, index, index + 1); };
    removeButton.onClick = [&rackToUse, busId, index] { rackToUse.removeEffect (busId, index); };

    for (auto* button : std::initializer_list<juce::Button*> { &editButton, &bypassButton, &earlierButton, &laterButton, &removeButton })
    {
        button->setWantsKeyboardFocus (false);
        addAndMakeVisible (button);
    }
}

void EffectsPanel::EffectBox::MoveButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto ink = controls::drawSymbolButtonTile (g, *this, highlighted && isEnabled(), down);
    const auto centre = getLocalBounds().toFloat().getCentre();
    const auto direction = later ? 1.0f : -1.0f;
    juce::Path triangle;
    triangle.addTriangle (centre.x + 4.0f * direction, centre.y, centre.x - 3.0f * direction, centre.y - 5.0f, centre.x - 3.0f * direction, centre.y + 5.0f);
    g.setColour (isEnabled() ? ink : juce::Colours::black.withAlpha (0.2f));
    g.fillPath (triangle);
}

void EffectsPanel::EffectBox::paint (juce::Graphics& g)
{
    // Faded while it's bypassed
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (bypassed ? juce::Colour (0xfff3f3f5) : juce::Colours::white);
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (bypassed ? juce::Colours::black.withAlpha (0.15f) : controls::accent.withAlpha (0.55f));
    g.drawRoundedRectangle (bounds, 6.0f, 1.2f);
}

void EffectsPanel::EffectBox::resized()
{
    auto bounds = getLocalBounds().reduced (6, 5);
    auto top = bounds.removeFromTop (22);
    removeButton.setBounds (top.removeFromRight (22));
    name.setBounds (top);
    bounds.removeFromTop (4);
    editButton.setBounds (bounds.removeFromLeft (44));
    bounds.removeFromLeft (4);
    bypassButton.setBounds (bounds.removeFromLeft (58));
    laterButton.setBounds (bounds.removeFromRight (24));
    earlierButton.setBounds (bounds.removeFromRight (24));
    name.setColour (juce::Label::textColourId, bypassed ? controls::secondaryText : juce::Colour (0xff1b1b1b));
}

//==============================================================================
EffectsPanel::BusCard::BusCard (EffectRack& rackToUse, int busIdToShow, int place)
    : busId (busIdToShow)
{
    auto& host = rackToUse.getHost();

    title.setText ("Bus " + juce::String (place + 1), juce::dontSendNotification);
    title.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    addAndMakeVisible (title);

    toLabel.setText ("goes to", juce::dontSendNotification);
    toLabel.setColour (juce::Label::textColourId, controls::secondaryText);
    toLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (toLabel);

    // On to the main output, or a bus after it, so nothing goes round in a loop
    fillOutputs (output, host, place + 1, host.getBusOutput (busId));
    output.setTooltip ("Where this bus goes next: the main output, or another bus after it, to go through its effects too");
    output.onChange = [&rackToUse, this] { rackToUse.setBusOutput (busId, output.getSelectedId() - 1); };
    addAndMakeVisible (output);

    removeButton.setTooltip ("Take this bus out, with its effects. What goes to it goes to the main output instead.");
    removeButton.onClick = [&rackToUse, id = busId] { rackToUse.removeBus (id); };
    addAndMakeVisible (removeButton);

    for (int i = 0; i < host.getNumEffects (busId); ++i)
        addAndMakeVisible (effects.add (std::make_unique<EffectBox> (rackToUse, busId, i)));

    addEffectButton.setButtonText (withEllipsis ("Add Effect"));
    addEffectButton.setTooltip ("Load a VST3 effect at the end of this bus's chain");
    addEffectButton.setWantsKeyboardFocus (false);
    addEffectButton.onClick = [&rackToUse, id = busId, this] { rackToUse.chooseEffect (id, addEffectButton); };
    addAndMakeVisible (addEffectButton);
}

int EffectsPanel::BusCard::getIdealHeight (int width) const
{
    // The effects, and the button to add one, in rows as wide as fit
    const auto perRow = juce::jmax (1, (width - 24 + boxGap) / (boxWidth + boxGap));
    const auto rows = (effects.size() + perRow) / perRow;
    return cardTitleHeight + rows * boxHeight + (rows - 1) * 12 + 14;
}

void EffectsPanel::BusCard::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (controls::sidebarBackground);
    g.fillRoundedRectangle (bounds, 8.0f);
    g.setColour (juce::Colours::black.withAlpha (0.12f));
    g.drawRoundedRectangle (bounds, 8.0f, 1.0f);

    // Arrows from each effect to the next, along each row
    for (int i = 0; i + 1 < effects.size(); ++i)
        if (effects[i]->getY() == effects[i + 1]->getY())
            drawArrow (g, (float) effects[i]->getRight() + 3.0f, (float) effects[i + 1]->getX() - 3.0f, (float) effects[i]->getBounds().getCentreY());
}

void EffectsPanel::BusCard::resized()
{
    auto bounds = getLocalBounds().reduced (12, 0);
    auto top = bounds.removeFromTop (cardTitleHeight).withSizeKeepingCentre (bounds.getWidth(), 26);
    removeButton.setBounds (top.removeFromRight (26));
    top.removeFromRight (8);
    output.setBounds (top.removeFromRight (150));
    top.removeFromRight (4);
    toLabel.setBounds (top.removeFromRight (60));
    title.setBounds (top);

    // The chain, a row at a time, with the add button after the last effect
    const auto perRow = juce::jmax (1, (bounds.getWidth() + boxGap) / (boxWidth + boxGap));

    for (int i = 0; i <= effects.size(); ++i)
    {
        const auto row = i / perRow, column = i % perRow;
        const juce::Rectangle<int> area (bounds.getX() + column * (boxWidth + boxGap), bounds.getY() + row * (boxHeight + 12), boxWidth, boxHeight);

        if (i < effects.size())
            effects[i]->setBounds (area);
        else
            addEffectButton.setBounds (area.withSizeKeepingCentre (130, 28).withX (area.getX()));
    }
}

//==============================================================================
EffectsPanel::EffectsPanel (EffectRack& rackToShow)
    : rack (rackToShow)
{
    for (auto [label, text] : { std::pair { &instrumentsHeading, "Instruments" }, { &busesHeading, "Buses" } })
    {
        controls::makeHeading (*label, text);
        addAndMakeVisible (*label);
    }

    emptyNote.setText ("A bus is a chain of effects. Add one, add effects to it, and send instruments to it above.", juce::dontSendNotification);
    emptyNote.setColour (juce::Label::textColourId, controls::secondaryText);
    addAndMakeVisible (emptyNote);

    addBusButton.setButtonText ("Add Bus");
    addBusButton.setTooltip ("Add a bus: a chain of effects, which instruments can be sent through");
    addBusButton.setWantsKeyboardFocus (false);
    addBusButton.onClick = [this] { rack.addBus(); };
    addAndMakeVisible (addBusButton);

    rebuild();
}

void EffectsPanel::refresh()
{
    triggerAsyncUpdate();
}

void EffectsPanel::handleAsyncUpdate()
{
    rebuild();
}

void EffectsPanel::rebuild()
{
    cancelPendingUpdate();
    auto& host = rack.getHost();

    // Each part, and where it goes
    parts.clear();

    for (int part = 0; part < host.getNumParts(); ++part)
    {
        auto* row = parts.add (std::make_unique<PartRow>());
        row->name.setText (getPartName != nullptr ? getPartName (part) : "Instrument " + juce::String (part + 1), juce::dontSendNotification);
        fillOutputs (row->output, host, 0, host.getPartOutput (part));
        row->output.setTooltip ("Where this instrument goes: straight to the main output, or through a bus's effects");
        row->output.onChange = [this, part, box = &row->output] { rack.setPartOutput (part, box->getSelectedId() - 1); };
        addAndMakeVisible (row->name);
        addAndMakeVisible (row->output);
    }

    buses.clear();
    const auto ids = host.getBuses();

    for (size_t place = 0; place < ids.size(); ++place)
        addAndMakeVisible (buses.add (std::make_unique<BusCard> (rack, ids[place], (int) place)));

    emptyNote.setVisible (ids.empty());
    setSize (getWidth(), getIdealHeight());
    resized();
    repaint();
}

int EffectsPanel::getIdealHeight() const
{
    auto height = padding + headingHeight + 6 + parts.size() * (rowHeight + 4) + 18 + headingHeight + 6;

    for (auto* bus : buses)
        height += bus->getIdealHeight (getWidth() - 2 * padding) + 12;

    return height + (buses.isEmpty() ? 28 : 0) + 30 + padding;
}

void EffectsPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::white);
}

void EffectsPanel::resized()
{
    auto bounds = getLocalBounds().reduced (padding);

    instrumentsHeading.setBounds (bounds.removeFromTop (headingHeight));
    bounds.removeFromTop (6);

    for (auto* row : parts)
    {
        auto line = bounds.removeFromTop (rowHeight);
        bounds.removeFromTop (4);
        row->output.setBounds (line.removeFromRight (180).reduced (0, 2));
        row->name.setBounds (line);
    }

    bounds.removeFromTop (18);
    busesHeading.setBounds (bounds.removeFromTop (headingHeight));
    bounds.removeFromTop (6);

    if (buses.isEmpty())
    {
        emptyNote.setBounds (bounds.removeFromTop (22));
        bounds.removeFromTop (6);
    }

    for (auto* bus : buses)
    {
        bus->setBounds (bounds.removeFromTop (bus->getIdealHeight (bounds.getWidth())));
        bounds.removeFromTop (12);
    }

    addBusButton.setBounds (bounds.removeFromTop (28).withWidth (110));
}

//==============================================================================
EffectsWindow::EffectsWindow (EffectRack& rack, std::function<juce::String (int)> getPartName, std::function<void()> onClose)
    : DocumentWindow ("Effects", juce::Colours::white, DocumentWindow::closeButton),
      panel (rack),
      onCloseButtonPressed (std::move (onClose))
{
    panel.getPartName = std::move (getPartName);
    panel.setSize (800, 100);
    panel.rebuild();

    viewport.setViewedComponent (&panel, false);
    viewport.setScrollBarsShown (true, false);
    setUsingNativeTitleBar (true);
    setContentNonOwned (&viewport, false);
    setResizable (true, false);
    setResizeLimits (640, 320, 2000, 2000);
    centreWithSize (840, 560);
    setVisible (true);
}

void EffectsWindow::refresh()
{
    panel.refresh();
}

void EffectsWindow::resized()
{
    DocumentWindow::resized();

    // The panel's as wide as the window, and as tall as it needs, scrolling up and down.
    panel.setSize (viewport.getMaximumVisibleWidth(), panel.getIdealHeight());
    panel.rebuild();
}

void EffectsWindow::closeButtonPressed()
{
    const auto callback = onCloseButtonPressed;
    callback();
}
