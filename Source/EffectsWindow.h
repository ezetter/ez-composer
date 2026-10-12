#pragma once

#include "Controls.h"
#include "EffectRack.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

/** The effects page: where each instrument goes, the main output or a bus, and the buses, each a
    chain of effects played one after another, going on to the main output or a later bus. Effects
    can be added to a bus's chain, opened, bypassed, moved along it and taken out, and buses added
    and taken out.
*/
class EffectsPanel final : public juce::Component,
                           private juce::AsyncUpdater
{
public:
    explicit EffectsPanel (EffectRack&);

    /** What a part's called, e.g. "Instrument 1 · Pianoteq". */
    std::function<juce::String (int part)> getPartName;

    /** Shows the buses and where everything goes as they are now, once the event being handled
        is done with, as it may have come from one of the controls that go.
    */
    void refresh();

    /** Does it now. */
    void rebuild();

    /** How tall it needs to be, at its width, to show everything. */
    int getIdealHeight() const;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void handleAsyncUpdate() override;

    /** One effect in a bus's chain: its name, and buttons to open it, bypass it, move it along the
        chain, or take it out.
    */
    struct EffectBox final : public juce::Component
    {
        EffectBox (EffectRack&, int busId, int index);
        void paint (juce::Graphics&) override;
        void resized() override;

        /** A small button with a triangle pointing left or right, for moving an effect along its chain. */
        struct MoveButton final : public juce::Button
        {
            explicit MoveButton (bool pointsLater) : juce::Button (pointsLater ? "Later" : "Earlier"), later (pointsLater) {}
            void paintButton (juce::Graphics&, bool highlighted, bool down) override;
            const bool later;
        };

        juce::Label name;
        juce::TextButton editButton { "Edit" }, bypassButton { "Bypass" }, removeButton;
        MoveButton earlierButton { false }, laterButton { true };
        bool bypassed = false;
    };

    /** A bus: its name, where it goes, a button to take it out, and its chain of effects, with a
        button to add another at the end.
    */
    struct BusCard final : public juce::Component
    {
        BusCard (EffectRack&, int busId, int place);
        void paint (juce::Graphics&) override;
        void resized() override;
        int getIdealHeight (int width) const;

        int busId;
        juce::Label title, toLabel;
        juce::ComboBox output;
        controls::BinButton removeButton;
        juce::OwnedArray<EffectBox> effects;
        juce::TextButton addEffectButton;
    };

    /** A part, and where it goes. */
    struct PartRow
    {
        juce::Label name;
        juce::ComboBox output;
    };

    EffectRack& rack;
    juce::Label instrumentsHeading, busesHeading, emptyNote;
    juce::OwnedArray<PartRow> parts;
    juce::OwnedArray<BusCard> buses;
    juce::TextButton addBusButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectsPanel)
};

/** The window the effects page is shown in, scrolling if it's too small for it. */
class EffectsWindow final : public juce::DocumentWindow
{
public:
    /** The window leaves its owner to delete it when its close button is pressed. */
    EffectsWindow (EffectRack&, std::function<juce::String (int part)> getPartName, std::function<void()> onCloseButtonPressed);

    EffectsPanel& getPanel() noexcept { return panel; }

    /** Shows the rack as it is now. */
    void refresh();

    void closeButtonPressed() override;
    void resized() override;

private:
    EffectsPanel panel;
    juce::Viewport viewport;
    std::function<void()> onCloseButtonPressed;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectsWindow)
};
