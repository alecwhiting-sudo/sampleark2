#include "Panels.h"
#include "Theme.h"
#include "AudioEngine.h"
#include <cmath>

using namespace sa::theme;
using juce::Colour;
using juce::Graphics;
using juce::Rectangle;
using juce::String;
using juce::Justification;

namespace
{
// ---- shared drawing helpers ----

void drawPanel (Graphics& g, Rectangle<float> r, Colour fill, Colour border,
                float radius = (float) dim::panelRadius)
{
    g.setColour (fill);
    g.fillRoundedRectangle (r, radius);
    g.setColour (border);
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
}

// Mono uppercase section label, e.g. SOURCE / FX RACK / VARIATIONS.
void sectionLabel (Graphics& g, const String& text, Rectangle<int> r, Colour c,
                   float size = 10.0f, Justification j = Justification::centredLeft)
{
    g.setColour (c);
    g.setFont (monoFont (size, true));
    g.drawText (text.toUpperCase(), r, j);
}

// A pseudo-button: rounded plate + centred mono label. M0 has no interaction.
void pseudoButton (Graphics& g, Rectangle<float> r, const String& text,
                   Colour fill, Colour border, Colour textCol, float fontSize = 11.0f)
{
    g.setColour (fill);
    g.fillRoundedRectangle (r, (float) dim::ctrlRadius);
    if (! border.isTransparent())
    {
        g.setColour (border);
        g.drawRoundedRectangle (r.reduced (0.5f), (float) dim::ctrlRadius, 1.0f);
    }
    g.setColour (textCol);
    g.setFont (monoFont (fontSize, true));
    g.drawText (text, r, Justification::centred);
}

// Knob: dark dial with a value arc and an indicator line. value in 0..1.
void drawKnob (Graphics& g, Rectangle<float> dial, float value, bool accent)
{
    const auto cx = dial.getCentreX();
    const auto cy = dial.getCentreY();
    const auto rad = dial.getWidth() * 0.5f;

    // dial body
    g.setColour (Colour (0xff1a1916));
    g.fillEllipse (dial);
    g.setColour (Colour (0xff34322e));
    g.drawEllipse (dial.reduced (0.5f), 1.0f);

    const float a0 = juce::degreesToRadians (-135.0f);
    const float a1 = juce::degreesToRadians (135.0f);
    const float av = a0 + value * (a1 - a0);
    const float trackR = rad - 2.0f;

    // value arc
    juce::Path arc;
    arc.addCentredArc (cx, cy, trackR, trackR, 0.0f, a0, av, true);
    g.setColour (accent ? colour::accent : Colour (0xff8a847c));
    g.strokePath (arc, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));

    // indicator line
    const float ix = cx + std::sin (av) * (rad - 5.0f);
    const float iy = cy - std::cos (av) * (rad - 5.0f);
    g.setColour (colour::ink);
    g.drawLine (cx, cy, ix, iy, 2.0f);
}

// A labelled knob cell sitting on a (optionally core-tinted) plate.
void knobCell (Graphics& g, Rectangle<int> cell, int knobSize, float value,
               const String& label, bool core)
{
    auto r = cell.toFloat();
    drawPanel (g, r, core ? colour::accentTint : colour::panelAlt,
               core ? colour::accent.withAlpha (0.45f) : colour::borderSubtle);

    auto top = cell.removeFromTop (cell.getHeight() - 14);
    Rectangle<float> dial ((float) top.getCentreX() - knobSize * 0.5f,
                           (float) top.getCentreY() - knobSize * 0.5f + 2.0f,
                           (float) knobSize, (float) knobSize);
    drawKnob (g, dial, value, core);

    g.setColour (core ? colour::accent : colour::faint);
    g.setFont (monoFont (8.0f));
    g.drawText (label, cell, Justification::centred);
}

// Deterministic percussive-looking waveform (no randomness → stable across repaints).
void drawWaveform (Graphics& g, Rectangle<float> area, int nBars,
                   float startFrac, float endFrac, Colour on, Colour off, float seed = 0.0f)
{
    const float w = area.getWidth() / (float) nBars;
    const float midY = area.getCentreY();
    const float h = area.getHeight();
    for (int i = 0; i < nBars; ++i)
    {
        const float t = (float) i / (float) nBars;
        float env = std::exp (-t * 4.0f);
        if (i < 3) env *= 0.25f + 0.75f * ((float) i / 3.0f);
        const float texture = 0.45f + 0.55f * std::abs (std::sin ((i + 1) * 12.9898f + seed));
        float v = juce::jlimit (0.04f, 1.0f, env * texture);
        const float bh = juce::jmax (2.0f, v * h);
        const bool inTrim = (t >= startFrac && t <= endFrac);
        g.setColour (inTrim ? on : off);
        g.fillRect (area.getX() + i * w, midY - bh * 0.5f, juce::jmax (1.0f, w - 0.0f), bh);
    }
}

// Beat grid fitted to a lane spanning spanSeconds; downbeats (bar starts) brighter.
void drawTempoGrid (Graphics& g, Rectangle<float> w, double spanSeconds, double tempo)
{
    if (spanSeconds <= 0.0 || tempo <= 0.0) return;
    const double beat = 60.0 / tempo;
    const int n = (int) (spanSeconds / beat);
    if (n > 600) return;   // too dense to be useful
    for (int i = 1; i <= n; ++i)
    {
        const float x = w.getX() + (float) (i * beat / spanSeconds) * w.getWidth();
        const bool downbeat = (i % 4 == 0);
        g.setColour (downbeat ? Colour (0x30ffffff) : Colour (0x12ffffff));
        g.fillRect (x, w.getY(), 1.0f, w.getHeight());
    }
}

// Per-effect editor graph drawn from the slot's live parameters.
void drawFxGraph (Graphics& g, Rectangle<float> a, const sa::FxSlot& slot)
{
    const auto& p = slot.params;
    auto X = [&] (float t) { return a.getX() + t * a.getWidth(); };
    auto Y = [&] (float v) { return a.getY() + (1.0f - v) * a.getHeight(); };
    const int N = 72;
    juce::Path path;

    switch (slot.type)
    {
        case sa::FxType::Filter:
        {
            const float cut = p[0], res = p[1];
            const int ty = (int) std::round (p.size() > 3 ? p[3] : 0.0f);
            for (int i = 0; i <= N; ++i)
            {
                const float x = (float) i / N;
                float m = (ty == 0) ? (x < cut ? 1.0f : juce::jmax (0.0f, 1.0f - (x - cut) * 4.5f))
                        : (ty == 2) ? (x > cut ? 1.0f : juce::jmax (0.0f, 1.0f - (cut - x) * 4.5f))
                                    : juce::jmax (0.0f, 1.0f - std::abs (x - cut) / 0.16f);
                m += res * 0.5f * std::exp (-std::pow ((x - cut) / 0.045f, 2.0f));
                m = juce::jlimit (0.0f, 1.25f, m);
                const float px = X (x), py = Y (m / 1.25f);
                if (i == 0) path.startNewSubPath (px, py); else path.lineTo (px, py);
            }
            g.setColour (colour::accent.withAlpha (0.35f));
            g.fillRect (X (cut), a.getY(), 1.0f, a.getHeight());
            g.setColour (colour::accent); g.strokePath (path, juce::PathStrokeType (2.2f));
            break;
        }
        case sa::FxType::Distortion:
        {
            const float k = 1.0f + p[0] * 9.0f;
            for (int i = 0; i <= N; ++i)
            {
                const float x = (float) i / N, inp = x * 2.0f - 1.0f;
                const float out = (float) (std::tanh (inp * k) / std::tanh (k));
                const float px = X (x), py = Y (out * 0.46f + 0.5f);
                if (i == 0) path.startNewSubPath (px, py); else path.lineTo (px, py);
            }
            g.setColour (colour::accent); g.strokePath (path, juce::PathStrokeType (2.0f));
            break;
        }
        case sa::FxType::Bitcrush:
        {
            const int lv = juce::jmax (2, (int) std::round (2 + p[0] * 12));
            const int hold = juce::jmax (1, (int) std::round ((1 - p[1]) * 6) + 1);
            for (int i = 0; i <= N; ++i)
            {
                const int si = (i / hold) * hold;
                const float raw = std::sin ((float) si / N * juce::MathConstants<float>::pi * 4.0f);
                const float q = std::round ((raw * 0.5f + 0.5f) * (lv - 1)) / (lv - 1);
                const float px = X ((float) i / N), py = Y (q * 0.8f + 0.1f);
                if (i == 0) path.startNewSubPath (px, py); else path.lineTo (px, py);
            }
            g.setColour (colour::accent); g.strokePath (path, juce::PathStrokeType (1.6f));
            break;
        }
        case sa::FxType::Delay:
        {
            const float sp = 0.09f + p[0] * 0.16f, dec = 0.5f + p[1] * 0.45f;
            float x = 0.07f, h = 0.82f;
            for (int i = 0; i < 7 && x < 0.98f; ++i)
            {
                g.setColour (i == 0 ? colour::accent : colour::accent.withAlpha (0.7f));
                g.drawLine (X (x), a.getBottom(), X (x), Y (h), i == 0 ? 2.4f : 2.0f);
                x += sp; h *= dec;
            }
            break;
        }
        default: break;
    }
}
} // namespace

namespace sa
{
// ===================== TOP BAR =====================
TopBar::TopBar()
{
    playB.setColours (colour::playBg, colour::playBorder, colour::playText);
    stopB.setColours (colour::buttonNeutral, colour::border, Colour (0xffbfbcb5));
    loopB.setColours (colour::buttonNeutral, colour::border, Colour (0xffbfbcb5));
    loadB.setColours (colour::buttonNeutral2, Colour (0xff4a4640), Colour (0xff8c8980));
    loadB.setFontSize (10.5f);
    for (auto* btn : { &playB, &stopB, &loopB, &loadB }) addAndMakeVisible (btn);
    playB.onClick = [this] { if (onPlay) onPlay(); };
    stopB.onClick = [this] { if (onStop) onStop(); };
    loopB.onClick = [this] { if (onLoop) onLoop(); };
    loadB.onClick = [this] { if (onLoad) onLoad(); };

    everyBox.addItem ("Off", 1);
    everyBox.addItem ("1/4", 2);
    everyBox.addItem ("1/2", 3);
    everyBox.addItem ("Bar", 4);
    everyBox.setSelectedId (1, juce::dontSendNotification);
    everyBox.setColour (juce::ComboBox::backgroundColourId, colour::well2);
    everyBox.setColour (juce::ComboBox::textColourId, colour::ink);
    everyBox.setColour (juce::ComboBox::outlineColourId, colour::borderSubtle2);
    everyBox.setColour (juce::ComboBox::arrowColourId, colour::faint);
    everyBox.onChange = [this] { if (onEvery) onEvery (everyBox.getSelectedId()); };
    addAndMakeVisible (everyBox);
}

void TopBar::refresh()
{
    const bool on = (engine != nullptr && engine->isLoopOn());
    loopB.setColours (on ? colour::accent : colour::buttonNeutral,
                      on ? colour::accentLight : colour::border,
                      on ? Colour (0xff1a1410) : Colour (0xffbfbcb5));
    repaint();
}

void TopBar::resized()
{
    const int btnH = 28;
    const int midY = (getHeight() - btnH) / 2;
    auto cv = [&] (Rectangle<int> a) { return a.withSizeKeepingCentre (a.getWidth(), btnH); };

    auto r = getLocalBounds().reduced (12, 0);
    playB.setBounds (cv (r.removeFromLeft (78)));
    r.removeFromLeft (9);
    stopB.setBounds (cv (r.removeFromLeft (64)));
    r.removeFromLeft (9);
    loopB.setBounds (cv (r.removeFromLeft (60)));
    r.removeFromLeft (21); // gap + divider + gap (divider drawn in paint)
    loadB.setBounds (cv (r.removeFromLeft (140)));

    // EVERY combo sits just left of the TEMPO chip
    const int tempoX = getWidth() - 12 - 210;
    everyBox.setBounds (tempoX - 12 - 84, midY + 2, 84, 24);
}

void TopBar::paint (Graphics& g)
{
    g.setColour (colour::panelAlt); g.fillRect (getLocalBounds());
    g.setColour (juce::Colours::black); g.fillRect (0, getHeight() - 1, getWidth(), 1);

    const int btnH = 28;
    const int midY = (getHeight() - btnH) / 2;

    // divider between LOOP and LOAD
    g.setColour (Colour (0xff3a3833));
    g.fillRect (loopB.getRight() + 10, (getHeight() - 24) / 2, 1, 24);

    // "PLAY EVERY" label left of the combo
    g.setColour (colour::faint); g.setFont (monoFont (8.5f, true));
    g.drawText ("PLAY EVERY", juce::Rectangle<int> (everyBox.getX() - 80, midY, 76, btnH),
                Justification::centredRight);

    // TEMPO chip (right) — static until M6
    Rectangle<int> tempoI (getWidth() - 12 - 210, midY, 210, btnH);
    auto tempo = tempoI.toFloat();
    drawPanel (g, tempo, colour::well2, colour::borderSubtle2, (float) dim::ctrlRadius);
    auto ti = tempo.reduced (10, 0);
    g.setColour (colour::faint); g.setFont (monoFont (9.0f, true));
    g.drawText ("TEMPO", ti.removeFromLeft (44), Justification::centredLeft);
    g.setColour (colour::ink); g.setFont (monoFont (13.0f, true));
    g.drawText ("120.0", ti.removeFromLeft (70), Justification::centred);
    pseudoButton (g, ti.removeFromRight (52).reduced (0, 4), "DETECT",
                  colour::buttonNeutral2, Colour (0xff3a3833), colour::dim, 9.0f);

    // filename chip (between LOAD and the EVERY label) — driven by the engine
    const int chipRight = everyBox.getX() - 80 - 8;
    Rectangle<int> chipI (loadB.getRight() + 12, midY, chipRight - (loadB.getRight() + 12), btnH);
    auto chip = chipI.toFloat();
    drawPanel (g, chip, colour::well2, colour::borderSubtle2, (float) dim::ctrlRadius);
    auto ci = chip.reduced (13, 0);
    const bool has = (engine != nullptr && engine->hasFile());
    g.setColour (has ? colour::ink : colour::faint); g.setFont (monoFont (12.0f));
    g.drawText (has ? engine->fileName() : juce::String ("no sample loaded"),
                ci.removeFromLeft (180), Justification::centredLeft);
    if (has)
    {
        const double fileSecs = engine->sampleRate() > 0.0 ? engine->lengthFrames() / engine->sampleRate() : 0.0;
        g.setColour (colour::faint); g.setFont (monoFont (10.0f));
        g.drawText (juce::String (fileSecs, 2) + "s   "
                    + juce::String (engine->sampleRate() / 1000.0, 1) + " kHz - "
                    + juce::String (engine->bitDepth()) + "b",
                    ci, Justification::centredLeft);
    }
    else
    {
        g.setColour (colour::faint2); g.setFont (monoFont (10.0f));
        g.drawText ("drop a .wav or click LOAD SAMPLE", ci, Justification::centredLeft);
    }
}

// ===================== SAMPLE (SOURCE / OUTPUT / BOTH) =====================
juce::Rectangle<int> SourcePanel::toggleBounds() const
{
    auto inner = getLocalBounds().reduced (13, 11);
    auto header = inner.removeFromTop (18);
    return header.removeFromRight (128).withSizeKeepingCentre (128, 15);
}

juce::Rectangle<float> SourcePanel::sourceWaveArea() const
{
    if (viewMode == 1) return {};   // output-only: no source lane
    auto inner = getLocalBounds().toFloat().reduced (13.0f, 11.0f);
    inner.removeFromTop (18.0f);
    inner.removeFromTop (9.0f);
    auto well = inner;
    if (viewMode == 2)              // both: source is the top lane
        well = well.removeFromTop (well.getHeight() * 0.5f - 3.0f);
    return well.reduced (8.0f, 6.0f);
}

void SourcePanel::drawSource (Graphics& g, juce::Rectangle<float> lane)
{
    drawPanel (g, lane, colour::well, colour::borderSubtle, 3.0f);
    auto w = lane.reduced (8.0f, 6.0f);
    g.setColour (Colour (0x10ffffff));
    g.fillRect (w.getX(), w.getCentreY(), w.getWidth(), 1.0f);

    const auto& p = engine->prep();
    const float xStart = w.getX() + (float) p.startFrac * w.getWidth();
    const float xEnd   = w.getX() + (float) p.endFrac   * w.getWidth();

    g.setColour (colour::waveOn);
    engine->thumbnail().drawChannels (g, w.toNearestInt(), 0.0, engine->thumbnail().getTotalLength(), 1.0f);

    const double srcGridSecs = juce::jmax (1.0, (double) engine->lengthFrames()) / engine->sampleRate();
    drawTempoGrid (g, w, srcGridSecs, engine->tempo());

    g.setColour (Colour (0xaa161513));
    g.fillRect (lane.getX(), lane.getY(), xStart - lane.getX(), lane.getHeight());
    g.fillRect (xEnd, lane.getY(), lane.getRight() - xEnd, lane.getHeight());

    const double srcSecs = juce::jmax (1.0, (double) engine->lengthFrames()) / engine->sampleRate();
    const float fiPx = (float) (p.fadeInMs  * 0.001 / srcSecs) * w.getWidth();
    const float foPx = (float) (p.fadeOutMs * 0.001 / srcSecs) * w.getWidth();
    g.setColour (colour::accent.withAlpha (0.7f));
    if (fiPx > 1.0f) g.drawLine (xStart, lane.getBottom(), xStart + fiPx, lane.getY(), 1.2f);
    if (foPx > 1.0f) g.drawLine (xEnd - foPx, lane.getY(), xEnd, lane.getBottom(), 1.2f);

    g.setColour (colour::accent);
    g.fillRect (xStart, lane.getY(), 2.0f, lane.getHeight());
    g.fillRect (xEnd - 2.0f, lane.getY(), 2.0f, lane.getHeight());
    g.fillRect (xStart, lane.getY(), 7.0f, 5.0f);
    g.fillRect (xEnd - 7.0f, lane.getY(), 7.0f, 5.0f);

    // playhead = dry-region progress (freezes at the trim end while the tail rings on)
    const double dry = engine->dryRegionSeconds();
    if (dry > 0.0)
    {
        const float f = juce::jlimit (0.0f, 1.0f, (float) (engine->positionSeconds() / dry));
        const float px = xStart + f * (xEnd - xStart);
        g.setColour (engine->isPlaying() ? colour::accentLight2 : Colour (0x66e9e7e2));
        g.fillRect (px, lane.getY(), engine->isPlaying() ? 1.5f : 1.0f, lane.getHeight());
    }
}

void SourcePanel::drawOutput (Graphics& g, juce::Rectangle<float> lane)
{
    drawPanel (g, lane, colour::well, colour::borderSubtle, 3.0f);
    auto w = lane.reduced (8.0f, 6.0f);
    g.setColour (Colour (0x10ffffff));
    g.fillRect (w.getX(), w.getCentreY(), w.getWidth(), 1.0f);

    const int cols = juce::jmax (1, (int) w.getWidth());
    const auto peaks = engine->outputPeaks (cols);
    const float midY = w.getCentreY(), h = w.getHeight();
    g.setColour (colour::accentLight2);   // the "printed" result, warm
    for (int i = 0; i < cols; ++i)
    {
        const float bh = juce::jmax (1.0f, peaks[(size_t) i] * h);
        g.fillRect (w.getX() + (float) i, midY - bh * 0.5f, 1.0f, bh);
    }

    drawTempoGrid (g, w, engine->lengthSeconds(), engine->tempo());

    const double tot = engine->lengthSeconds();   // region + tail
    if (tot > 0.0)
    {
        // OUTPUT fade ramps (anchored to the dynamic ends)
        const auto& pp = engine->prep();
        const float fiPx = (float) (pp.outFadeInMs * 0.001 / tot) * w.getWidth();
        const float foPx = (float) (juce::jmax (0.003, pp.outFadeOutMs * 0.001) / tot) * w.getWidth();
        g.setColour (colour::accent.withAlpha (0.7f));
        if (fiPx > 1.0f) g.drawLine (w.getX(), w.getBottom(), w.getX() + fiPx, w.getY(), 1.2f);
        if (foPx > 1.0f) g.drawLine (w.getRight() - foPx, w.getY(), w.getRight(), w.getBottom(), 1.2f);

        const float f = juce::jlimit (0.0f, 1.0f, (float) (engine->positionSeconds() / tot));
        const float px = w.getX() + f * w.getWidth();
        g.setColour (engine->isPlaying() ? colour::accent : Colour (0x66e9e7e2));
        g.fillRect (px, lane.getY(), engine->isPlaying() ? 1.5f : 1.0f, lane.getHeight());
    }

    // background re-render in progress
    if (engine->isRenderBusy())
    {
        g.setColour (Colour (0xcc141312));
        g.fillRect (lane.reduced (1.0f));
        g.setColour (colour::accent); g.setFont (monoFont (10.0f, true));
        g.drawText ("redrawing...", lane.toNearestInt(), Justification::centred);
    }
}

void SourcePanel::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    drawPanel (g, b, colour::panel, colour::border);
    auto inner = b.reduced (13.0f, 11.0f);
    const bool has = (engine != nullptr && engine->hasFile());

    auto header = inner.removeFromTop (18.0f).toNearestInt();
    sectionLabel (g, "SAMPLE", header.removeFromLeft (70), colour::dim);
    g.setColour (has ? colour::accent : colour::faint2);
    g.fillEllipse ((float) header.getX() - 56.0f, (float) header.getCentreY() - 3.5f, 7.0f, 7.0f);

    // INPUT | OUT | BOTH view toggle
    auto tg = toggleBounds();
    const char* modes[] = { "INPUT", "OUT", "BOTH" };
    const int segW[] = { 48, 34, 46 };
    for (int i = 0; i < 3; ++i)
    {
        auto seg = tg.removeFromLeft (segW[i]);
        const bool a = (i == viewMode);
        pseudoButton (g, seg.toFloat(), modes[i],
                      a ? colour::accent : colour::buttonNeutral2,
                      a ? colour::accentLight : colour::borderSubtle,
                      a ? Colour (0xff1a1410) : colour::faint, 8.0f);
    }
    g.setColour (colour::faint); g.setFont (monoFont (9.0f));
    g.drawText (has ? (juce::String (engine->sampleRate() / 1000.0, 1) + " kHz / "
                       + juce::String (engine->bitDepth()) + "-bit - "
                       + juce::String (engine->lengthFrames()) + " frames")
                    : juce::String ("no sample loaded"),
                header.withTrimmedRight (136), Justification::centredRight);

    inner.removeFromTop (9.0f);
    auto well = inner;

    if (! (has && engine->thumbnail().getTotalLength() > 0.0))
    {
        drawPanel (g, well, colour::well, colour::borderSubtle, 3.0f);
        auto w = well.reduced (8.0f, 6.0f);
        g.setColour (colour::border.withAlpha (0.6f));
        g.drawRoundedRectangle (w.reduced (10.0f), 4.0f, 1.0f);
        g.setColour (colour::faint); g.setFont (monoFont (11.0f));
        g.drawText (has ? "decoding..." : "drop a .wav file here  -  or click LOAD SAMPLE",
                    w.toNearestInt(), Justification::centred);
        return;
    }

    auto laneLabel = [&] (juce::Rectangle<float> lane, const char* t)
    {
        g.setColour (colour::faint); g.setFont (monoFont (7.5f, true));
        g.drawText (t, juce::Rectangle<int> ((int) lane.getX() + 4, (int) lane.getY() + 2, 60, 10),
                    Justification::topLeft);
    };

    if (viewMode == 0)       { drawSource (g, well); }
    else if (viewMode == 1)  { drawOutput (g, well); laneLabel (well, "OUTPUT"); }
    else
    {
        auto top = well.removeFromTop (well.getHeight() * 0.5f - 3.0f);
        well.removeFromTop (6.0f);
        drawSource (g, top);   laneLabel (top, "SOURCE");
        drawOutput (g, well);  laneLabel (well, "OUTPUT");
    }
}

void SourcePanel::mouseDown (const juce::MouseEvent& e)
{
    if (engine == nullptr || ! engine->hasFile()) return;
    if (toggleBounds().contains (e.getPosition()))   // view toggle
    {
        const int rel = e.x - toggleBounds().getX();
        viewMode = rel < 48 ? 0 : (rel < 82 ? 1 : 2);
        repaint();
        return;
    }
    auto w = sourceWaveArea();
    if (w.isEmpty() || ! w.toFloat().expanded (0.0f, 6.0f).contains (e.position)) { dragHandle = 0; return; }
    const auto& p = engine->prep();
    const float xStart = w.getX() + (float) p.startFrac * w.getWidth();
    const float xEnd   = w.getX() + (float) p.endFrac   * w.getWidth();
    dragHandle = (std::abs (e.position.x - xStart) <= std::abs (e.position.x - xEnd)) ? 1 : 2;
    mouseDrag (e);
}

void SourcePanel::mouseDrag (const juce::MouseEvent& e)
{
    if (engine == nullptr || ! engine->hasFile() || dragHandle == 0) return;
    auto w = sourceWaveArea();
    if (w.isEmpty()) return;
    const float frac = juce::jlimit (0.0f, 1.0f, (e.position.x - w.getX()) / w.getWidth());
    const auto& p = engine->prep();
    if (dragHandle == 1) engine->setTrim (frac, p.endFrac);
    else                 engine->setTrim (p.startFrac, frac);
}

void SourcePanel::mouseMove (const juce::MouseEvent& e)
{
    auto w = sourceWaveArea();
    if (engine == nullptr || ! engine->hasFile() || w.isEmpty())
    {
        setMouseCursor (juce::MouseCursor::NormalCursor);
        return;
    }
    const auto& p = engine->prep();
    const float xStart = w.getX() + (float) p.startFrac * w.getWidth();
    const float xEnd   = w.getX() + (float) p.endFrac   * w.getWidth();
    const bool nearHandle = w.toFloat().expanded (0.0f, 6.0f).contains (e.position)
                            && std::min (std::abs (e.position.x - xStart), std::abs (e.position.x - xEnd)) < 8.0f;
    setMouseCursor (nearHandle ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
}

// ===================== PREP (M2 interactive) =====================
PrepPanel::PrepPanel()
{
    const char* names[] = { "TRIM", "SHAPE", "PITCH" };
    for (int i = 0; i < 3; ++i)
    {
        auto* t = new FlatButton (names[i]);
        t->setFontSize (9.0f);
        t->onClick = [this, i] { mode = i; buildMode(); };
        addAndMakeVisible (t);
        tabs.add (t);
    }
    applyLooks();
}

void PrepPanel::buildMode()
{
    knobs.clear();
    toggles.clear();
    startK = endK = fiK = foK = ofiK = ofoK = gainK = nullptr;
    normT = nullptr;
    if (engine == nullptr) return;

    const auto& p = engine->prep();
    auto addKnob = [this] (const char* lbl, float val, bool core, std::function<void(float)> cb) -> Knob*
    {
        auto* k = new Knob (lbl);
        k->setCore (core);
        k->setValue (val);
        k->onValueChange = std::move (cb);
        addAndMakeVisible (k);
        knobs.add (k);
        return k;
    };
    auto addToggle = [this] (const char* lbl, std::function<void()> cb) -> FlatButton*
    {
        auto* btn = new FlatButton (lbl);
        btn->setFontSize (9.5f);
        btn->onClick = std::move (cb);
        addAndMakeVisible (btn);
        toggles.add (btn);
        return btn;
    };

    if (mode == 0) // TRIM
    {
        startK = addKnob ("Start", (float) p.startFrac, true,
                          [this] (float v) { engine->setTrim (v, engine->prep().endFrac); });
        endK   = addKnob ("End", (float) p.endFrac, true,
                          [this] (float v) { engine->setTrim (engine->prep().startFrac, v); });
        addKnob ("Transient", 0.5f, false, [] (float) {})->setInert (true);  // M2-later
        fiK = addKnob ("Fade In",  (float) (p.fadeInMs  / 250.0), false,
                       [this] (float v) { engine->setFadeInMs (v * 250.0); });
        foK = addKnob ("Fade Out", (float) (p.fadeOutMs / 250.0), false,
                       [this] (float v) { engine->setFadeOutMs (v * 250.0); });
        ofiK = addKnob ("Fade In 2",  (float) (p.outFadeInMs  / 500.0), false,
                        [this] (float v) { engine->setOutFadeInMs (v * 500.0); });
        ofoK = addKnob ("Fade Out 2", (float) (p.outFadeOutMs / 4000.0), false,
                        [this] (float v) { engine->setOutFadeOutMs (v * 4000.0); });
        addToggle ("Auto-Detect", [] {})                                     // M2-later
            ->setColours (colour::panelAlt, colour::borderSubtle, colour::faint2);
    }
    else if (mode == 1) // SHAPE
    {
        gainK = addKnob ("Gain", (float) ((p.gainDb + 24.0) / 48.0), true,
                         [this] (float v) { engine->setGainDb (v * 48.0 - 24.0); });
        addKnob ("Attack", 0.12f, true, [] (float) {})->setInert (true);    // M2-later
        addKnob ("Hold", 0.34f, false, [] (float) {})->setInert (true);
        addKnob ("Release", 0.42f, false, [] (float) {})->setInert (true);
        normT = addToggle ("Normalize", [this] { engine->setNormalize (! engine->prep().normalize); });
    }
    else // PITCH (M6)
    {
        addKnob ("Tune", 0.5f, true, [] (float) {})->setInert (true);
        addKnob ("Transpose", 0.5f, false, [] (float) {})->setInert (true);
        addKnob ("Formant", 0.5f, false, [] (float) {})->setInert (true);
    }

    applyLooks();
    resized();
    repaint();
}

void PrepPanel::applyLooks()
{
    for (int i = 0; i < tabs.size(); ++i)
    {
        const bool a = (i == mode);
        tabs[i]->setColours (a ? colour::accent : colour::buttonNeutral2,
                             a ? colour::accentLight : Colour (0xff38352f),
                             a ? Colour (0xff1a1410) : colour::faint);
    }
    if (normT != nullptr && engine != nullptr)
    {
        const bool on = engine->prep().normalize;
        normT->setColours (on ? colour::accentTint : colour::buttonNeutral2,
                           on ? colour::accent : colour::borderSubtle,
                           on ? colour::accent : colour::faint);
    }
}

void PrepPanel::refresh()
{
    if (engine == nullptr) return;
    const auto& p = engine->prep();
    if (startK) startK->setValue ((float) p.startFrac);
    if (endK)   endK->setValue ((float) p.endFrac);
    if (fiK)    fiK->setValue ((float) (p.fadeInMs / 250.0));
    if (foK)    foK->setValue ((float) (p.fadeOutMs / 250.0));
    if (ofiK)   ofiK->setValue ((float) (p.outFadeInMs / 500.0));
    if (ofoK)   ofoK->setValue ((float) (p.outFadeOutMs / 4000.0));
    if (gainK)  gainK->setValue ((float) ((p.gainDb + 24.0) / 48.0));
    applyLooks();
}

void PrepPanel::resized()
{
    auto inner = getLocalBounds().reduced (13, 9);
    auto left = inner.removeFromLeft (188);
    left.removeFromTop (18);                 // PREP label (painted)
    auto tabsRow = left.removeFromTop (24);
    for (auto* t : tabs) { t->setBounds (tabsRow.removeFromLeft (58)); tabsRow.removeFromLeft (4); }

    inner.removeFromLeft (12 + 1 + 12);      // gap + divider + gap
    auto row = inner.removeFromTop (58);
    for (auto* k : knobs) { k->setBounds (row.removeFromLeft (52)); row.removeFromLeft (6); }
    row.removeFromLeft (4);
    for (auto* t : toggles) { t->setBounds (row.removeFromLeft (80).withSizeKeepingCentre (80, 38)); row.removeFromLeft (6); }
}

void PrepPanel::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    drawPanel (g, b, colour::panel, colour::border);
    sectionLabel (g, "PREP", getLocalBounds().reduced (13, 9).removeFromLeft (188).removeFromTop (12),
                  colour::faint, 9.0f);
    g.setColour (colour::borderSubtle2);
    g.fillRect (13 + 188 + 12, 12, 1, getHeight() - 24);
}

// ===================== FX RACK (M3 interactive) =====================
int RackPanel::rowAt (juce::Point<int> p) const
{
    if (p.y < 39) return -1;
    const int r = (p.y - 39) / dim::fxRowH;
    return (r >= 0 && r < kNumSlots) ? r : -1;
}

void RackPanel::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    drawPanel (g, b, colour::panel, colour::border);

    auto header = getLocalBounds().removeFromTop (38).reduced (12, 0);
    sectionLabel (g, "FX RACK", header.removeFromLeft (120), colour::accent);
    g.setColour (colour::faint); g.setFont (monoFont (8.5f));
    g.drawText ("IN -> OUT - drag to reorder", header, Justification::centredRight);
    g.setColour (colour::borderSubtle);
    g.fillRect (0.0f, 38.0f, (float) getWidth(), 1.0f);

    if (engine == nullptr) return;
    const auto& slots = engine->rack().slots();
    const int selectedIdx = engine->selectedSlot();

    int y = 39;
    for (int i = 0; i < kNumSlots; ++i)
    {
        const auto& slot = slots[i];
        const auto& info = fxInfo (slot.type);
        const bool on = slot.on;
        const bool impl = info.implemented;
        const bool sel = (i == selectedIdx);

        auto rowR = Rectangle<int> (0, y, getWidth(), dim::fxRowH - 1);
        if (sel) { g.setColour (colour::accentTint); g.fillRect (rowR); g.setColour (colour::accent); g.fillRect (rowR.removeFromLeft (3)); }
        else      rowR.removeFromLeft (3);
        g.setColour (colour::hairline);
        g.fillRect (0, y + dim::fxRowH - 1, getWidth(), 1);

        auto row = rowR.reduced (9, 0);
        g.setColour (colour::faint2); g.setFont (monoFont (12.0f));
        g.drawText ("::", row.removeFromLeft (14), Justification::centred);
        row.removeFromLeft (4);
        auto led = row.removeFromLeft (13).withSizeKeepingCentre (13, 13).toFloat();
        g.setColour (on ? colour::accent : Colour (0xff1a1916));
        g.fillEllipse (led);
        g.setColour (on ? colour::accentLight : Colour (0xff3a3833));
        g.drawEllipse (led.reduced (0.5f), 1.0f);
        row.removeFromLeft (8);
        g.setColour (sel ? colour::accent : (on ? colour::dim : Colour (0xff4f4c47)));
        g.setFont (monoFont (10.0f, sel));
        g.drawText (String (i + 1), row.removeFromLeft (14), Justification::centred);
        row.removeFromLeft (4);
        pseudoButton (g, row.removeFromRight (20).withSizeKeepingCentre (20, 20).toFloat(), "<>",
                      colour::buttonNeutral2, Colour (0xff38352f), colour::faint, 9.0f);
        row.removeFromRight (6);
        auto badge = row.removeFromRight (40).withSizeKeepingCentre (40, 16).toFloat();
        drawPanel (g, badge, colour::buttonNeutral2, Colour (0xff34322e), 2.0f);
        g.setColour (impl ? colour::faint : Colour (0xff4f4c47)); g.setFont (monoFont (7.5f, true));
        g.drawText ("CORE", badge, Justification::centred);

        // name + summary (live params for implemented effects; "v1.5" for placeholders)
        String sub;
        if (impl)
        {
            sub = String (info.params[0].label).toLowerCase() + " " + String (juce::roundToInt (slot.params[0] * 100));
            if (info.params.size() >= 2)
                sub += " - " + String (info.params[1].label).toLowerCase() + " " + String (juce::roundToInt (slot.params[1] * 100));
        }
        else sub = "passthrough - v1.5";

        auto txt = row;
        g.setColour (! impl ? colour::faint2 : (on ? (sel ? colour::ink : Colour (0xffcfccc4)) : colour::faint2));
        g.setFont (uiFont (11.5f, true));
        g.drawText (info.name, txt.removeFromTop (txt.getHeight() / 2), Justification::bottomLeft);
        g.setColour (colour::faint); g.setFont (monoFont (8.0f));
        g.drawText (sub, txt, Justification::topLeft);

        y += dim::fxRowH;
    }
}

void RackPanel::mouseDown (const juce::MouseEvent& e)
{
    if (engine == nullptr) return;
    const int row = rowAt (e.getPosition());
    if (row < 0) return;
    const int x = e.x;
    if (x < 24)        { engine->selectSlot (row); dragRow = row; }   // handle -> reorder
    else if (x < 46)   { engine->rackToggleBypass (row); }           // LED -> bypass
    else               { engine->selectSlot (row); }                 // row -> select
}

void RackPanel::mouseDrag (const juce::MouseEvent& e)
{
    if (engine == nullptr || dragRow < 0) return;
    const int target = juce::jlimit (0, kNumSlots - 1, (e.getPosition().y - 39) / dim::fxRowH);
    if (target != dragRow) { engine->rackMove (dragRow, target); dragRow = target; }
}

void RackPanel::mouseUp (const juce::MouseEvent&)
{
    dragRow = -1;
}

// ===================== EFFECT EDITOR (M3 interactive) =====================
void DetailPanel::buildEditor()
{
    knobs.clear();
    segButtons.clear();
    knobParamIndex.clear();
    segParams.clear();
    segCounts.clear();
    if (engine == nullptr) { builtSlot = -1; return; }

    const int sel = engine->selectedSlot();
    builtSlot = sel;
    const auto& slot = engine->rack().slots()[sel];
    const auto& info = fxInfo (slot.type);

    for (int i = 0; i < (int) info.params.size(); ++i)
    {
        const auto& pr = info.params[i];
        if (pr.isSeg())
        {
            segParams.push_back (i);
            segCounts.push_back ((int) pr.options.size());
            for (int o = 0; o < (int) pr.options.size(); ++o)
            {
                auto* btn = new FlatButton (pr.options[o]);
                btn->setFontSize (9.0f);
                btn->onClick = [this, sel, i, o] { engine->rackSetParam (sel, i, (float) o); };
                addAndMakeVisible (btn);
                segButtons.add (btn);
            }
        }
        else
        {
            auto* k = new Knob (pr.label);
            k->setCore (false);
            k->setValue (slot.params[i]);
            const int idx = i;
            k->onValueChange = [this, sel, idx] (float v) { engine->rackSetParam (sel, idx, v); };
            if (! info.implemented) k->setInert (true);
            addAndMakeVisible (k);
            knobs.add (k);
            knobParamIndex.push_back (i);
        }
    }
    applyEditorLooks();
    resized();
    repaint();
}

void DetailPanel::applyEditorLooks()
{
    if (engine == nullptr || segButtons.isEmpty()) return;
    const auto& slot = engine->rack().slots()[engine->selectedSlot()];
    int btn = 0;
    for (size_t grp = 0; grp < segParams.size(); ++grp)
    {
        const int active = (int) std::round (slot.params[(size_t) segParams[grp]]);
        for (int o = 0; o < segCounts[grp] && btn < segButtons.size(); ++o, ++btn)
        {
            const bool a = (o == active);
            segButtons[btn]->setColours (a ? colour::accent : colour::panelAlt,
                                         a ? colour::accentLight : colour::borderSubtle,
                                         a ? Colour (0xff1a1410) : colour::dim);
        }
    }
}

void DetailPanel::refresh()
{
    if (engine == nullptr) return;
    if (engine->selectedSlot() != builtSlot) { buildEditor(); return; }
    const auto& slot = engine->rack().slots()[engine->selectedSlot()];
    for (int i = 0; i < knobs.size(); ++i)
        knobs[i]->setValue (slot.params[(size_t) knobParamIndex[(size_t) i]]);
    applyEditorLooks();
    repaint();
}

void DetailPanel::resized()
{
    auto body = getLocalBounds().withTrimmedTop (39).reduced (13, 12);
    const int graphH = juce::jmin (body.getHeight() - 66, 200);
    body.removeFromTop (graphH);
    body.removeFromTop (12);
    auto row = body.removeFromTop (54);
    for (auto* k : knobs) { k->setBounds (row.removeFromLeft (54)); row.removeFromLeft (9); }
    int sb = 0;
    for (size_t grp = 0; grp < segParams.size(); ++grp)
    {
        body.removeFromTop (8);
        auto segRow = body.removeFromTop (24);
        segRow.removeFromLeft (62);   // label space (drawn in paint)
        for (int o = 0; o < segCounts[grp] && sb < segButtons.size(); ++o, ++sb)
        {
            segButtons[sb]->setBounds (segRow.removeFromLeft (44));
            segRow.removeFromLeft (4);
        }
    }
}

juce::Rectangle<float> DetailPanel::graphBounds() const
{
    auto body = getLocalBounds().withTrimmedTop (39).reduced (13, 12);
    const int graphH = juce::jmin (body.getHeight() - 66, 200);
    return body.removeFromTop (graphH).toFloat();
}

void DetailPanel::mouseDown (const juce::MouseEvent& e) { handleGraphDrag (e); }
void DetailPanel::mouseDrag (const juce::MouseEvent& e) { handleGraphDrag (e); }

// Drag anywhere on the graph as an alternative to the knobs. X/Y map to the two most
// musical params per effect.
void DetailPanel::handleGraphDrag (const juce::MouseEvent& e)
{
    if (engine == nullptr) return;
    const auto gb = graphBounds();
    if (! gb.contains (e.position)) return;

    const int sel = engine->selectedSlot();
    const auto& slot = engine->rack().slots()[sel];
    if (! fxInfo (slot.type).implemented) return;

    const float nx = juce::jlimit (0.0f, 1.0f, (e.position.x - gb.getX()) / gb.getWidth());
    const float ny = juce::jlimit (0.0f, 1.0f, 1.0f - (e.position.y - gb.getY()) / gb.getHeight());

    switch (slot.type)
    {
        case FxType::Filter:     engine->rackSetParam (sel, 0, nx); engine->rackSetParam (sel, 1, ny); break; // cutoff / reso
        case FxType::Distortion: engine->rackSetParam (sel, 0, ny); engine->rackSetParam (sel, 1, nx); break; // drive / tone
        case FxType::Bitcrush:   engine->rackSetParam (sel, 0, ny); engine->rackSetParam (sel, 1, nx); break; // bits / rate
        case FxType::Delay:   // X = time (free mode only), Y = feedback
        {
            const int syncIdx = (int) std::round (slot.params.size() > 5 ? slot.params[5] : 0.0f);
            if (syncIdx <= 0) engine->rackSetParam (sel, 0, nx);
            engine->rackSetParam (sel, 1, ny);
            break;
        }
        default: break;
    }
}

void DetailPanel::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    drawPanel (g, b, colour::panel, colour::border);
    if (engine == nullptr) return;

    const auto& slot = engine->rack().slots()[engine->selectedSlot()];
    const auto& info = fxInfo (slot.type);
    const bool impl = info.implemented;

    auto header = getLocalBounds().removeFromTop (38).reduced (13, 0);
    g.setColour (slot.on && impl ? colour::accent : colour::faint2);
    g.fillEllipse ((float) header.getX(), (float) header.getCentreY() - 3.5f, 7.0f, 7.0f);
    header.removeFromLeft (14);
    g.setColour (colour::ink); g.setFont (monoFont (11.0f, true));
    g.drawText (info.name, header.removeFromLeft (90), Justification::centredLeft);
    auto badge = header.removeFromLeft (64).withSizeKeepingCentre (60, 16).toFloat();
    drawPanel (g, badge, colour::buttonNeutral2, Colour (0xff34322e), 2.0f);
    g.setColour (colour::faint); g.setFont (monoFont (7.5f, true));
    g.drawText ("BUILT-IN", badge, Justification::centred);
    juce::String hint = impl ? "drag the graph or knobs" : "full effect in v1.5";
    bool hintAccent = false;
    if (impl && slot.type == FxType::Delay)
    {
        const double sec = sa::delayTimeSeconds (slot.params, engine->tempo());
        const int syncIdx = (int) std::round (slot.params.size() > 5 ? slot.params[5] : 0.0f);
        hint = (syncIdx <= 0 ? juce::String ("free  ") : (juce::String (info.params[5].options[syncIdx]) + "  "))
             + juce::String (juce::roundToInt (sec * 1000.0)) + " ms";
        hintAccent = true;
    }
    g.setColour (hintAccent ? colour::accent : colour::faint); g.setFont (monoFont (8.5f, hintAccent));
    g.drawText (hint, header, Justification::centredRight);

    g.setColour (colour::borderSubtle);
    g.fillRect (0.0f, 38.0f, (float) getWidth(), 1.0f);

    auto body = getLocalBounds().withTrimmedTop (39).reduced (13, 12);
    const int graphH = juce::jmin (body.getHeight() - 66, 200);
    auto graph = body.removeFromTop (graphH).toFloat();
    drawPanel (g, graph, colour::well, colour::borderSubtle, 3.0f);
    g.setColour (Colour (0x0dffffff));
    g.fillRect (graph.getX(), graph.getCentreY(), graph.getWidth(), 1.0f);
    g.fillRect (graph.getCentreX(), graph.getY(), 1.0f, graph.getHeight());

    if (impl)
        drawFxGraph (g, graph, slot);
    else
    {
        g.setColour (colour::faint); g.setFont (monoFont (11.0f));
        g.drawText ("passthrough - full effect arrives in v1.5", graph.toNearestInt(), Justification::centred);
    }

    // segmented control labels (Type / Sync / Mode) to the left of each group
    int sb = 0;
    for (size_t grp = 0; grp < segParams.size(); ++grp)
    {
        if (sb < segButtons.size())
        {
            auto first = segButtons[sb]->getBounds();
            g.setColour (colour::faint); g.setFont (monoFont (8.5f, true));
            g.drawText (info.params[(size_t) segParams[grp]].label,
                        juce::Rectangle<int> (13, first.getY(), first.getX() - 13 - 4, first.getHeight()),
                        Justification::centredLeft);
        }
        sb += segCounts[grp];
    }
}

// ===================== MUTATE AFFECTS =====================
void MutateStrip::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    // One framed "MUTATE" module (accent border) so DEPTH + AFFECTS read as a single
    // driver -> targets system rather than two unrelated rows of buttons.
    drawPanel (g, b, colour::panelAlt2, colour::accent.withAlpha (0.30f));

    auto area = getLocalBounds();

    // module legend
    auto title = area.removeFromTop (16).reduced (12, 0);
    g.setColour (colour::accent); g.setFont (monoFont (9.0f, true));
    g.drawText ("MUTATE", title.withTrimmedTop (2), Justification::topLeft);

    area = area.reduced (12, 0);
    area.removeFromBottom (7);

    // DEPTH = one continuous mutation-severity scale; Gentle/Vibing/Massive/Unsafe are
    // named zones along it. This IS the old "amount" knob — a single value, two reads:
    // drag the handle for fine control, or click a zone to snap. (Interaction lands later.)
    auto depthRow = area.removeFromTop (24);
    g.setColour (colour::faint); g.setFont (monoFont (8.5f, true));
    g.drawText ("DEPTH", depthRow.removeFromLeft (60), Justification::centredLeft);
    depthRow.removeFromLeft (6);
    auto pctArea = depthRow.removeFromRight (42);
    depthRow.removeFromRight (8);
    auto track = depthRow.withSizeKeepingCentre (depthRow.getWidth(), 22).toFloat();
    drawPanel (g, track, colour::well2, colour::borderSubtle, 4.0f);

    const float severity = 0.45f; // == the mutate amount shown on the MUTATE button
    const char* zones[] = { "Gentle", "Vibing", "Massive", "Unsafe" };
    const int activeZone = juce::jlimit (0, 3, (int) (severity * 4.0f));
    auto fill = track.reduced (1.5f);
    g.setColour (colour::accent.withAlpha (0.16f));
    g.fillRoundedRectangle (fill.withWidth (fill.getWidth() * severity), 3.0f); // "how hard" fill
    const float zw = track.getWidth() / 4.0f;
    for (int i = 0; i < 4; ++i)
    {
        auto z = track.withX (track.getX() + i * zw).withWidth (zw);
        const bool active = (i == activeZone);
        g.setColour (active ? colour::accent : colour::faint);
        g.setFont (monoFont (8.5f, active));
        g.drawText (zones[i], z.toNearestInt(), Justification::centred);
        if (i > 0) { g.setColour (colour::borderSubtle); g.drawLine (z.getX(), track.getY() + 3.0f, z.getX(), track.getBottom() - 3.0f, 1.0f); }
    }
    const float apexX = track.getX() + severity * track.getWidth(); // handle = funnel apex
    g.setColour (colour::accent);
    g.fillRect (apexX - 1.0f, track.getY() - 2.0f, 2.0f, track.getHeight() + 4.0f);
    g.fillEllipse (apexX - 4.0f, track.getY() - 4.0f, 8.0f, 8.0f);
    g.setFont (monoFont (9.5f, true));
    g.drawText (String (juce::roundToInt (severity * 100)) + "%", pctArea, Justification::centred);

    // AFFECTS row (the targets)
    auto affectsRow = area.removeFromBottom (24);
    g.setColour (colour::faint); g.setFont (monoFont (8.5f, true));
    g.drawText ("AFFECTS", affectsRow.removeFromLeft (60), Justification::centredLeft);
    affectsRow.removeFromLeft (6);

    // FUNNEL: the selected depth fans down and spreads across the whole affects span.
    auto funnel = area; // middle band between the two rows
    const float topY = (float) funnel.getY();
    const float botY = (float) funnel.getBottom() - 1.0f;
    const float ax0 = (float) affectsRow.getX();
    const float ax1 = (float) affectsRow.getRight();
    juce::Path fp;
    fp.startNewSubPath (apexX - 7.0f, topY);
    fp.lineTo (apexX + 7.0f, topY);
    fp.lineTo (ax1, botY);
    fp.lineTo (ax0, botY);
    fp.closeSubPath();
    g.setColour (colour::accent.withAlpha (0.10f));
    g.fillPath (fp);
    g.setColour (colour::accent.withAlpha (0.45f));
    g.drawLine (apexX - 7.0f, topY, ax0, botY, 1.2f);
    g.drawLine (apexX + 7.0f, topY, ax1, botY, 1.2f);
    // small downward chevron at the apex (force pushing down)
    g.setColour (colour::accent);
    g.drawLine (apexX - 3.5f, topY + 0.5f, apexX, topY + 4.0f, 1.4f);
    g.drawLine (apexX + 3.5f, topY + 0.5f, apexX, topY + 4.0f, 1.4f);

    // affects chips
    auto r = affectsRow;
    const char* chips[] = { "Everything", "Plug-ins", "Filter", "Envelopes", "Dynamics",
                            "Crossfades", "Timing", "Pitch", "Mangle", "Tail" };
    for (int i = 0; i < 10; ++i)
    {
        const bool every = (i == 0);
        const String label (chips[i]);
        const int w = label.length() * 6 + 18; // mono ~6px/char at 9px
        auto chip = r.removeFromLeft (w).withSizeKeepingCentre (w, 22).toFloat(); r.removeFromLeft (6);
        if (every)
            pseudoButton (g, chip, label, colour::accent, colour::accentLight, Colour (0xff1a1410), 9.0f);
        else
            pseudoButton (g, chip, label, colour::accentTint.withAlpha (0.12f),
                          colour::accent.withAlpha (0.4f), Colour (0xffcf9a6a), 9.0f);
    }
}

// ===================== VARIATIONS =====================
void VariationsPanel::paint (Graphics& g)
{
    auto full = getLocalBounds();
    g.setColour (colour::panelAlt);
    g.fillRect (full);

    // header
    auto header = full.removeFromTop (48).reduced (14, 0);
    g.setColour (juce::Colours::black);
    g.fillRect (0, 47, getWidth(), 1);
    sectionLabel (g, "VARIATIONS", header.removeFromLeft (110), colour::dim, 11.0f);
    g.setColour (colour::faint); g.setFont (monoFont (9.5f));
    g.drawText ("20 candidates", header.removeFromLeft (110), Justification::centredLeft);
    g.setColour (colour::success); g.setFont (monoFont (13.0f, true));
    g.drawText ("8 / 8", header.removeFromRight (46), Justification::centredRight);
    header.removeFromRight (8);
    g.setColour (colour::faint); g.setFont (monoFont (9.0f));
    g.drawText ("SELECTED", header.removeFromRight (72), Justification::centredRight);

    // footer pinned to bottom
    auto footer = full.removeFromBottom (100);

    // list
    auto list = full;
    for (int i = 0; i < 20; ++i)
    {
        auto rowR = list.removeFromTop (dim::varRowH);
        if (rowR.getY() >= list.getBottom() + dim::varRowH) break;
        const bool sel = (i < 8);
        if (sel) { g.setColour (colour::accentTint.withAlpha (0.07f)); g.fillRect (rowR); }
        g.setColour (sel ? colour::accent : juce::Colours::transparentBlack);
        g.fillRect (rowR.getX(), rowR.getY(), 3, rowR.getHeight());
        g.setColour (colour::hairline);
        g.fillRect (rowR.getX(), rowR.getBottom() - 1, rowR.getWidth(), 1);

        auto row = rowR.reduced (13, 0).withTrimmedLeft (0);
        // play
        auto play = row.removeFromLeft (24).withSizeKeepingCentre (24, 24).toFloat();
        g.setColour (colour::buttonNeutral2); g.fillEllipse (play);
        g.setColour (Colour (0xff3a3833)); g.drawEllipse (play.reduced (0.5f), 1.0f);
        g.setColour (Colour (0xffa8a59d)); g.setFont (uiFont (8.0f));
        g.drawText (">", play, Justification::centred);
        row.removeFromLeft (9);
        // index
        g.setColour (sel ? colour::accent : colour::faint);
        g.setFont (monoFont (10.0f, sel));
        g.drawText (String (i + 1).paddedLeft ('0', 2), row.removeFromLeft (18), Justification::centred);
        row.removeFromLeft (9);
        // M button + fav dot + name on the right
        auto mute = row.removeFromRight (22).withSizeKeepingCentre (22, 22).toFloat();
        drawPanel (g, mute, colour::buttonNeutral2, Colour (0xff38352f), 4.0f);
        g.setColour (Colour (0xff4a4843)); g.setFont (monoFont (9.0f, true));
        g.drawText ("M", mute, Justification::centred);
        row.removeFromRight (8);
        auto fav = row.removeFromRight (22).withSizeKeepingCentre (22, 22).toFloat();
        g.setColour (sel ? colour::accent.withAlpha (0.2f) : colour::buttonNeutral2);
        g.fillEllipse (fav);
        g.setColour (sel ? colour::accent : Colour (0xff38352f));
        g.drawEllipse (fav.reduced (0.5f), 1.0f);
        row.removeFromRight (8);
        g.setColour (colour::faint); g.setFont (monoFont (10.0f));
        g.drawText ("kick_var_" + String (i + 1).paddedLeft ('0', 2),
                    row.removeFromRight (96), Justification::centredLeft);
        // mini waveform fills the middle
        drawWaveform (g, row.toFloat().reduced (4.0f, 8.0f), 60, 0.0f, 1.0f,
                      sel ? colour::accent : colour::faint,
                      sel ? colour::accent : colour::faint, (float) i);
    }

    // footer
    g.setColour (colour::panelAlt2); g.fillRect (footer);
    g.setColour (juce::Colours::black); g.fillRect (footer.getX(), footer.getY(), footer.getWidth(), 1);
    auto f = footer.reduced (14, 12);

    // No separate amount dial — severity lives on the DEPTH slider; the button echoes it.
    auto gen = f.removeFromTop (42).toFloat();
    drawPanel (g, gen, colour::buttonNeutral2, colour::accent);
    g.setColour (colour::accent); g.setFont (monoFont (12.5f, true));
    g.drawText ("MUTATE", gen.removeFromLeft (gen.getWidth() * 0.46f), Justification::centred);
    g.setColour (Colour (0xff8c8980)); g.setFont (monoFont (8.5f));
    g.drawText ("20 candidates - Vibing 45%", gen, Justification::centred);

    f.removeFromTop (10);
    auto actions = f;
    auto keep = actions.removeFromRight (110); actions.removeFromRight (8);
    pseudoButton (g, actions.toFloat(), "WRITE SELECTED  ->",
                  colour::accent, colour::accentLight, Colour (0xff1a1410), 11.0f);
    pseudoButton (g, keep.toFloat(), "KEEP PLAYING",
                  colour::buttonNeutral, colour::border, Colour (0xffbfbcb5), 10.5f);
}
}
