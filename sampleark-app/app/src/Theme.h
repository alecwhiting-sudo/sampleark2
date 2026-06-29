#pragma once

#include <juce_graphics/juce_graphics.h>

// Central style system for SampleArk. All colours, fonts, and key dimensions live here
// (Arch04 design tokens). The app is dense; ad-hoc styling makes it feel messy fast, so
// every panel should pull from this namespace rather than hardcoding values.
namespace sa::theme
{
using juce::Colour;

// ---- Colour tokens (Arch04) ----
namespace colour
{
    inline const Colour bg            { 0xff161513 }; // window background
    inline const Colour panel         { 0xff201f1d }; // raised panels
    inline const Colour panelAlt      { 0xff1b1a18 }; // toolbar, right region
    inline const Colour panelAlt2     { 0xff1d1c19 }; // strips, footer
    inline const Colour well          { 0xff141312 }; // waveform / graph wells
    inline const Colour well2         { 0xff121110 }; // chips, deep insets
    inline const Colour border        { 0xff423f3b }; // panel borders
    inline const Colour borderSubtle  { 0xff2c2a26 }; // inset borders
    inline const Colour borderSubtle2 { 0xff2e2c28 };
    inline const Colour buttonNeutral { 0xff2b2926 };
    inline const Colour buttonNeutral2{ 0xff211f1d };
    inline const Colour hairline      { 0x12ffffff }; // ~rgba(255,255,255,.07) row separators

    inline const Colour accent        { 0xffcf6a2c }; // burnt orange — primary accent
    inline const Colour accentLight   { 0xffe0843f };
    inline const Colour accentLight2  { 0xfff4a05a };
    inline const Colour accentTint    { 0x17cf6a2c }; // ~rgba(207,106,44,.09) selected fill

    inline const Colour playBg        { 0xff235e38 };
    inline const Colour playBorder    { 0xff2f7a48 };
    inline const Colour playText      { 0xffdff3e4 };
    inline const Colour success       { 0xff3aa35c }; // N/8 count when exactly 8

    inline const Colour yellow        { 0xfff4be4f }; // transient tick
    inline const Colour ink           { 0xffe9e7e2 }; // primary text
    inline const Colour dim           { 0xff9a978f }; // section labels
    inline const Colour faint         { 0xff6f6c65 }; // meta text
    inline const Colour faint2        { 0xff5f5c56 };

    inline const Colour waveOn        { 0xffc8c5bd }; // waveform inside trim
    inline const Colour waveOff       { 0xff403e3a }; // waveform outside trim

    inline const Colour trafficRed    { 0xffed6a5e };
    inline const Colour trafficYellow { 0xfff4be4f };
    inline const Colour trafficGreen  { 0xff61c554 };
}

// ---- Dimensions (Arch04) ----
namespace dim
{
    constexpr int toolbarH      = 48;
    constexpr int fxRowH        = 46;
    constexpr int varRowH       = 46;
    constexpr int prepKnob      = 38;
    constexpr int detailKnob    = 42;
    constexpr int panelRadius   = 4;
    constexpr int ctrlRadius    = 3;
    constexpr int panelMargin   = 12;
    constexpr int panelPad      = 12;
    constexpr float leftFrac    = 0.64f; // left region width fraction
}

// ---- Fonts ----
// UI font = system; mono = the "scientific" voice for values/filenames/labels.
// Scale every label up, and floor the result so the smallest secondary/meta texts (badges,
// summaries, hints) can't render too tiny relative to the main labels.
inline float scaledHeight (float height)
{
    return juce::jmax (height * 1.18f, 10.0f);
}

inline juce::Font uiFont (float height, bool bold = false)
{
    return juce::Font (juce::FontOptions()
                           .withHeight (scaledHeight (height))
                           .withStyle (bold ? "Bold" : "Regular"));
}

inline juce::Font monoFont (float height, bool bold = false)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), scaledHeight (height),
                                          bold ? juce::Font::bold : juce::Font::plain));
}
}
