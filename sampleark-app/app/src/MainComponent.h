#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Panels.h"

namespace sa
{
// Root content component. Owns the static panels and lays them out per Arch04:
//   toolbar (top) ; body split left 64% / right 36% ;
//   left vertical stack: source -> prep -> (rack + detail) -> mutate strip ;
//   right: variations (which paints its own header + footer).
class MainComponent : public juce::Component
{
public:
    MainComponent();
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    TopBar topBar;
    SourcePanel source;
    PrepPanel prep;
    RackPanel rack;
    DetailPanel detail;
    MutateStrip mutate;
    VariationsPanel variations;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
}
