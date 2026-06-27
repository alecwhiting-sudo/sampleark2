#include "MainComponent.h"
#include "Theme.h"

namespace sa
{
MainComponent::MainComponent()
{
    addAndMakeVisible (topBar);
    addAndMakeVisible (source);
    addAndMakeVisible (prep);
    addAndMakeVisible (rack);
    addAndMakeVisible (detail);
    addAndMakeVisible (mutate);
    addAndMakeVisible (variations);

    setSize (1380, 860);
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (sa::theme::colour::bg);
}

void MainComponent::resized()
{
    using namespace sa::theme;
    auto area = getLocalBounds();

    topBar.setBounds (area.removeFromTop (dim::toolbarH));

    auto body = area;
    auto left = body.removeFromLeft (juce::roundToInt (body.getWidth() * dim::leftFrac));
    variations.setBounds (body); // right region

    // left interior, 12px outer margin and ~10px gaps
    auto L = left.reduced (12, 12);
    source.setBounds (L.removeFromTop (150));
    L.removeFromTop (10);
    prep.setBounds (L.removeFromTop (92));
    L.removeFromTop (10);
    mutate.setBounds (L.removeFromBottom (88)); // two rows: depth + affects
    L.removeFromBottom (10);

    // middle: rack (43%) + detail
    auto middle = L;
    rack.setBounds (middle.removeFromLeft (juce::roundToInt (middle.getWidth() * 0.43f)));
    middle.removeFromLeft (10);
    detail.setBounds (middle);
}
}
