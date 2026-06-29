#include "Panels.h"
#include "Theme.h"
#include "AudioEngine.h"
#include "Variations.h"
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

// Centred peak bars across a wave-area rect (same style for SOURCE + OUTPUT so they read alike).
void drawPeakBars (Graphics& g, juce::Rectangle<float> w, const std::vector<float>& peaks)
{
    const float midY = w.getCentreY(), h = w.getHeight();
    const int cols = (int) peaks.size();
    for (int i = 0; i < cols; ++i)
    {
        const float bh = juce::jmax (1.0f, peaks[(size_t) i] * h);
        g.fillRect (w.getX() + (float) i, midY - bh * 0.5f, 1.0f, bh);
    }
}

// Draw a transformer's shape across a wave-area rect: optional beat grid, the curve, and a
// playhead line at the current output position. Shared by the TRANSFORMERS panel and the
// SAMPLE overlay so both align with the audio. `outLen` = total output seconds.
void drawTransformerShape (Graphics& g, juce::Rectangle<float> wa, const sa::Transformer& t,
                           double outLen, double tempo, double posSec, bool playing, bool drawGrid)
{
    if (drawGrid)
    {
        const double gridSpan = (t.basis == 1)
            ? ((t.rateDiv <= 0) ? 1.0 / juce::jmax (0.01f, t.freqHz) : sa::transRateSeconds (t.rateDiv, tempo))
            : outLen;
        drawTempoGrid (g, wa, gridSpan, tempo);
    }

    juce::Path path;
    for (int i = 0; i < sa::kCurvePoints; ++i)
    {
        const float x = wa.getX() + (float) i / (float) (sa::kCurvePoints - 1) * wa.getWidth();
        const float y = wa.getBottom() - t.curve[(size_t) i] * wa.getHeight();
        if (i == 0) path.startNewSubPath (x, y); else path.lineTo (x, y);
    }
    g.setColour (t.on ? colour::accent : colour::faint);
    g.strokePath (path, juce::PathStrokeType (2.0f));

    if (outLen > 0.0)   // playhead: where playback is up to (cyclic wraps within one cycle)
    {
        double f;
        if (t.basis == 1)
        {
            const double cyc = (t.rateDiv <= 0) ? 1.0 / juce::jmax (0.01f, t.freqHz) : sa::transRateSeconds (t.rateDiv, tempo);
            const double ph = (cyc > 0.0 ? posSec / cyc : 0.0) + (double) t.phase;
            f = ph - std::floor (ph);
        }
        else f = juce::jlimit (0.0, 1.0, posSec / outLen);

        const float px = wa.getX() + (float) f * wa.getWidth();
        g.setColour (playing ? colour::accentLight2 : Colour (0x66e9e7e2));
        g.fillRect (px, wa.getY(), playing ? 1.5f : 1.0f, wa.getHeight());
    }
}

// Per-effect editor graph drawn from the slot's live parameters.
void drawFxGraph (Graphics& g, Rectangle<float> a, const sa::FxSlot& slot,
                  const std::vector<float>* paramsOverride = nullptr)
{
    const auto& p = (paramsOverride != nullptr && paramsOverride->size() == slot.params.size())
                        ? *paramsOverride : slot.params;
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
        case sa::FxType::Compression:
        {
            const float thr = p[0], rat = p[1];
            const double thrDb = -48.0 * (1.0 - thr), ratio = 1.0 + rat * 19.0;
            for (int i = 0; i <= N; ++i)
            {
                const float x = (float) i / N;                       // input amplitude 0..1
                const double inDb = 20.0 * std::log10 (juce::jmax (1.0e-3f, x));
                const double outDb = inDb > thrDb ? thrDb + (inDb - thrDb) / ratio : inDb;
                const float out = (float) std::pow (10.0, outDb / 20.0);
                const float px = X (x), py = Y (out);
                if (i == 0) path.startNewSubPath (px, py); else path.lineTo (px, py);
            }
            g.setColour (colour::faint); g.drawLine (X (0), Y (0), X (1), Y (1), 0.6f);  // unity ref
            const float tx = (float) std::pow (10.0, thrDb / 20.0);
            g.setColour (colour::accent.withAlpha (0.35f)); g.fillRect (X (tx), a.getY(), 1.0f, a.getHeight());
            g.setColour (colour::accent); g.strokePath (path, juce::PathStrokeType (2.0f));
            break;
        }
        case sa::FxType::Limiter:
        {
            const float ceil = 0.2f + p[0] * 0.8f;
            for (int i = 0; i <= N; ++i)
            {
                const float x = (float) i / N, out = juce::jmin (x, ceil);
                const float px = X (x), py = Y (out);
                if (i == 0) path.startNewSubPath (px, py); else path.lineTo (px, py);
            }
            g.setColour (colour::accent.withAlpha (0.35f)); g.fillRect (a.getX(), Y (ceil), a.getWidth(), 1.0f);
            g.setColour (colour::accent); g.strokePath (path, juce::PathStrokeType (2.0f));
            break;
        }
        case sa::FxType::Reverb:
        {
            const float size = p[0], decay = p.size() > 1 ? p[1] : 0.5f;
            const float k = 5.5f - decay * 4.5f - size * 0.5f;       // slower decay = longer tail
            for (int i = 0; i <= N; ++i)
            {
                const float x = (float) i / N;
                const float env = std::exp (-x * k);
                const float px = X (x), py = Y (env);
                if (i == 0) path.startNewSubPath (px, py); else path.lineTo (px, py);
            }
            path.lineTo (X (1), Y (0)); path.lineTo (X (0), Y (0)); path.closeSubPath();
            g.setColour (colour::accent.withAlpha (0.18f)); g.fillPath (path);
            g.setColour (colour::accent); g.strokePath (path, juce::PathStrokeType (2.0f));
            break;
        }
        case sa::FxType::Autopan:
        {
            const float rate = p[0], depth = p.size() > 1 ? p[1] : 0.45f;
            const float cycles = 1.0f + rate * 4.0f;
            for (int i = 0; i <= N; ++i)
            {
                const float x = (float) i / N;
                const float v = 0.5f + 0.5f * depth * std::sin (x * juce::MathConstants<float>::twoPi * cycles);
                const float px = X (x), py = Y (v);
                if (i == 0) path.startNewSubPath (px, py); else path.lineTo (px, py);
            }
            g.setColour (colour::faint); g.drawLine (a.getX(), Y (0.5f), a.getRight(), Y (0.5f), 0.6f);
            g.setColour (colour::accent); g.strokePath (path, juce::PathStrokeType (2.0f));
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

    FlatButton* vb[] = { &vSample, &vTrans, &vFx, &vMut, &vVars };
    for (int i = 0; i < 5; ++i)
    {
        vb[i]->setFontSize (8.5f);
        vb[i]->onClick = [this, i] { if (onView) onView (i); };
        addAndMakeVisible (vb[i]);
    }
    vInputs.setFontSize (8.5f);
    vInputs.onClick = [this] { if (onView) onView (5); };   // zone 5; shown first in the row
    addAndMakeVisible (vInputs);

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

void TopBar::setViewLit (int zone, bool lit)
{
    FlatButton* vb[] = { &vSample, &vTrans, &vFx, &vMut, &vVars, &vInputs };
    if (zone < 0 || zone > 5) return;
    vb[zone]->setColours (lit ? colour::accent : colour::buttonNeutral2,
                          lit ? colour::accentLight : colour::borderSubtle,
                          lit ? Colour (0xff1a1410) : colour::faint);
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

    // view toggles (show/hide major zones); INPUTS sits first per design
    r.removeFromLeft (12);
    vInputs.setBounds (r.removeFromLeft (56).withSizeKeepingCentre (56, 22));
    r.removeFromLeft (3);
    FlatButton* vb[] = { &vSample, &vTrans, &vFx, &vMut, &vVars };
    const int vw[] = { 44, 50, 30, 40, 44 };
    for (int i = 0; i < 5; ++i)
    {
        vb[i]->setBounds (r.removeFromLeft (vw[i]).withSizeKeepingCentre (vw[i], 22));
        r.removeFromLeft (3);
    }

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

    // filename chip (between the view toggles and the EVERY label) — driven by the engine
    const int chipRight = everyBox.getX() - 80 - 8;
    const int chipLeft = vVars.getRight() + 12;
    Rectangle<int> chipI (chipLeft, midY, chipRight - chipLeft, btnH);
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

juce::Rectangle<int> SourcePanel::stereoToggleBounds() const
{
    auto t = toggleBounds();
    return { t.getX() - 10 - 92, t.getY(), 92, t.getHeight() };
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

juce::Rectangle<float> SourcePanel::outputWaveArea() const
{
    if (viewMode == 0) return {};   // source-only: no output lane
    auto inner = getLocalBounds().toFloat().reduced (13.0f, 11.0f);
    inner.removeFromTop (18.0f);
    inner.removeFromTop (9.0f);
    auto well = inner;
    if (viewMode == 2)              // both: output is the bottom lane
    {
        well.removeFromTop (well.getHeight() * 0.5f - 3.0f);
        well.removeFromTop (6.0f);
    }
    return well.reduced (8.0f, 6.0f);
}

void SourcePanel::drawTransformerOverlay (Graphics& g, juce::Rectangle<float> w)
{
    if (engine == nullptr || overlayTrans < 0 || overlayTrans >= sa::kNumTransformers) return;
    const auto& t = engine->transformers()[(size_t) overlayTrans];
    drawTransformerShape (g, w, t, engine->lengthSeconds(), engine->tempo(),
                          engine->positionSeconds(), engine->isPlaying(), false);   // lane already drew the grid
    g.setColour (colour::accent.withAlpha (0.85f)); g.setFont (monoFont (7.5f, true));
    g.drawText ("TRANS " + juce::String (overlayTrans + 1), w.toNearestInt().reduced (3), Justification::topRight);
}

bool SourcePanel::editOverlayAt (const juce::MouseEvent& e)
{
    if (engine == nullptr || overlayTrans < 0 || overlayTrans >= sa::kNumTransformers) return false;
    auto w = outputWaveArea();
    if (w.isEmpty() || ! w.contains (e.position)) return false;
    const float fx = juce::jlimit (0.0f, 1.0f, (e.position.x - w.getX()) / w.getWidth());
    const float fy = juce::jlimit (0.0f, 1.0f, 1.0f - (e.position.y - w.getY()) / w.getHeight());
    auto t = engine->transformers()[(size_t) overlayTrans];
    const int idx = juce::jlimit (0, sa::kCurvePoints - 1, (int) std::round (fx * (sa::kCurvePoints - 1)));
    t.curve[(size_t) idx] = fy;
    if (idx > 0) t.curve[(size_t) (idx - 1)] = (t.curve[(size_t) (idx - 1)] + fy) * 0.5f;
    if (idx < sa::kCurvePoints - 1) t.curve[(size_t) (idx + 1)] = (t.curve[(size_t) (idx + 1)] + fy) * 0.5f;
    t.shapeType = -1;
    engine->setTransformer (overlayTrans, t);
    return true;
}

void SourcePanel::drawSource (Graphics& g, juce::Rectangle<float> lane)
{
    drawPanel (g, lane, colour::well, colour::borderSubtle, 3.0f);
    auto w = lane.reduced (8.0f, 6.0f);

    const auto& p = engine->prep();
    const float xStart = w.getX() + (float) p.startFrac * w.getWidth();
    const float xEnd   = w.getX() + (float) p.endFrac   * w.getWidth();

    // Waveform: combined (summed peaks) or stereo (L band over R band). OUTPUT stays combined.
    const int cols = juce::jmax (1, (int) w.getWidth());
    if (stereoView && engine->sourceChannels() >= 2)
    {
        auto topB = w.withHeight (w.getHeight() * 0.5f - 1.0f);
        auto botB = w.withTrimmedTop (w.getHeight() * 0.5f + 1.0f);
        g.setColour (Colour (0x10ffffff));
        g.fillRect (topB.getX(), topB.getCentreY(), topB.getWidth(), 1.0f);
        g.fillRect (botB.getX(), botB.getCentreY(), botB.getWidth(), 1.0f);
        g.setColour (colour::waveOn);
        drawPeakBars (g, topB, engine->sourcePeaks (cols, 0));
        drawPeakBars (g, botB, engine->sourcePeaks (cols, 1));
    }
    else
    {
        g.setColour (Colour (0x10ffffff));
        g.fillRect (w.getX(), w.getCentreY(), w.getWidth(), 1.0f);
        g.setColour (colour::waveOn);
        drawPeakBars (g, w, engine->sourcePeaks (cols, -1));
    }

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

    // Same Stereo/Combined choice as the SOURCE lane (gated on the source's channel count).
    const int cols = juce::jmax (1, (int) w.getWidth());
    g.setColour (colour::accentLight2);   // the "printed" result, warm
    if (stereoView && engine->sourceChannels() >= 2)
    {
        auto topB = w.withHeight (w.getHeight() * 0.5f - 1.0f);
        auto botB = w.withTrimmedTop (w.getHeight() * 0.5f + 1.0f);
        g.setColour (Colour (0x10ffffff));
        g.fillRect (topB.getX(), topB.getCentreY(), topB.getWidth(), 1.0f);
        g.fillRect (botB.getX(), botB.getCentreY(), botB.getWidth(), 1.0f);
        g.setColour (colour::accentLight2);
        drawPeakBars (g, topB, engine->outputPeaks (cols, 0));
        drawPeakBars (g, botB, engine->outputPeaks (cols, 1));
    }
    else
    {
        g.setColour (Colour (0x10ffffff));
        g.fillRect (w.getX(), w.getCentreY(), w.getWidth(), 1.0f);
        g.setColour (colour::accentLight2);
        drawPeakBars (g, w, engine->outputPeaks (cols, -1));
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

    drawTransformerOverlay (g, w);   // active transformer curve over the output (Overlay view)
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

    // STEREO / COMBINED channel-display toggle (affects the SOURCE lane only; OUTPUT is always combined)
    const bool canStereo = (has && engine->sourceChannels() >= 2);
    pseudoButton (g, stereoToggleBounds().toFloat(), stereoView ? "STEREO" : "COMBINED",
                  colour::buttonNeutral2, colour::borderSubtle,
                  canStereo ? colour::ink : colour::faint, 8.0f);

    g.setColour (colour::faint); g.setFont (monoFont (9.0f));
    g.drawText (has ? (juce::String (engine->sampleRate() / 1000.0, 1) + " kHz / "
                       + juce::String (engine->bitDepth()) + "-bit - "
                       + juce::String (engine->lengthFrames()) + " frames")
                    : juce::String ("no sample loaded"),
                header.withTrimmedRight (136 + 10 + 92), Justification::centredRight);

    inner.removeFromTop (9.0f);
    auto well = inner;

    if (! has)
    {
        drawPanel (g, well, colour::well, colour::borderSubtle, 3.0f);
        auto w = well.reduced (8.0f, 6.0f);
        g.setColour (colour::border.withAlpha (0.6f));
        g.drawRoundedRectangle (w.reduced (10.0f), 4.0f, 1.0f);
        g.setColour (colour::faint); g.setFont (monoFont (11.0f));
        g.drawText ("drop a .wav file here  -  or click LOAD SAMPLE",
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
    if (stereoToggleBounds().contains (e.getPosition())) { stereoView = ! stereoView; repaint(); return; }
    if (editOverlayAt (e)) { dragHandle = 0; return; }   // Overlay view: draw the curve on the output
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
    if (engine == nullptr || ! engine->hasFile()) return;
    if (dragHandle == 0) { editOverlayAt (e); return; }   // continue drawing the overlay curve
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

// ===================== TRANSFORMERS (M3a) =====================
TransformerPanel::TransformerPanel()
{
    for (int i = 0; i < kNumTransformers; ++i)
    {
        auto* btn = new FlatButton (juce::String (i + 1));
        btn->setFontSize (8.5f);
        btn->onClick = [this, i] { active = i; if (onActiveChanged) onActiveChanged (i); refresh(); };
        addAndMakeVisible (btn);
        laneBtns.add (btn);
    }

    viewBtn.setFontSize (8.5f);
    viewBtn.onClick = [this] { overlay = ! overlay; viewBtn.setButtonText (overlay ? "Separate" : "Overlay");
                               if (onOverlayToggle) onOverlayToggle (overlay); resized(); refresh(); };
    addAndMakeVisible (viewBtn);

    auto editActive = [this] (std::function<void (Transformer&)> fn)
    {
        if (engine == nullptr) return;
        auto t = engine->transformers()[(size_t) active];
        fn (t);
        engine->setTransformer (active, t);
    };

    onBtn.setFontSize (9.0f);
    onBtn.onClick = [this, editActive] { editActive ([] (Transformer& t) { t.on = ! t.on; }); };
    addAndMakeVisible (onBtn);

    basisOne.setFontSize (8.5f); basisCyc.setFontSize (8.5f);
    basisOne.onClick = [this, editActive] { editActive ([] (Transformer& t) { t.basis = 0; }); };
    basisCyc.onClick = [this, editActive] { editActive ([] (Transformer& t) { t.basis = 1; }); };
    addAndMakeVisible (basisOne); addAndMakeVisible (basisCyc);

    auto styleCombo = [] (juce::ComboBox& c)
    {
        c.setColour (juce::ComboBox::backgroundColourId, colour::well2);
        c.setColour (juce::ComboBox::textColourId, colour::ink);
        c.setColour (juce::ComboBox::outlineColourId, colour::borderSubtle2);
        c.setColour (juce::ComboBox::arrowColourId, colour::faint);
    };
    styleCombo (targetBox); styleCombo (shapeBox); styleCombo (rateBox);

    for (int s = 0; s < kNumShapes; ++s) shapeBox.addItem (shapeName (s), s + 1);
    shapeBox.addItem ("Drawn", kNumShapes + 1);
    shapeBox.onChange = [this, editActive]
    {
        const int id = shapeBox.getSelectedId();
        if (id >= 1 && id <= kNumShapes) editActive ([id] (Transformer& t) { t.applyShape (id - 1); });
    };
    addAndMakeVisible (shapeBox);

    const char* rates[] = { "Free", "1/1", "1/2", "1/4", "1/8", "1/16" };
    for (int i = 0; i < 6; ++i) rateBox.addItem (rates[i], i + 1);
    rateBox.onChange = [this, editActive] { const int d = rateBox.getSelectedId() - 1; editActive ([d] (Transformer& t) { t.rateDiv = d; }); };
    addAndMakeVisible (rateBox);

    freqKnob.setCore (false);
    freqKnob.onValueChange = [this, editActive] (float v) { const float hz = 0.1f * std::pow (200.0f, v); editActive ([hz] (Transformer& t) { t.freqHz = hz; t.rateDiv = 0; }); };   // turning Freq engages Free-Hz
    freqKnob.valueText = [] (float v) { const float hz = 0.1f * std::pow (200.0f, v); return hz < 10.0f ? juce::String (hz, 1) + " Hz" : juce::String (juce::roundToInt (hz)) + " Hz"; };
    freqKnob.setValueField (true);
    addAndMakeVisible (freqKnob);
    phaseKnob.setCore (false);
    phaseKnob.onValueChange = [this, editActive] (float v) { editActive ([v] (Transformer& t) { t.phase = v; }); };
    phaseKnob.valueText = [] (float v) { return juce::String (juce::roundToInt (v * 360.0f)) + juce::String::fromUTF8 ("\xc2\xb0"); };
    phaseKnob.setValueField (true);
    addAndMakeVisible (phaseKnob);

    targetBox.onChange = [this, editActive]
    {
        const int id = targetBox.getSelectedId();
        if (id >= 1 && id <= (int) targetRefs.size())
        {
            const auto r = targetRefs[(size_t) (id - 1)];
            editActive ([r] (Transformer& t) { t.kind = (TransTarget) r.kind; t.slot = r.slot; t.param = r.param; });
        }
    };
    addAndMakeVisible (targetBox);

    depthKnob.setCore (true);
    depthKnob.onValueChange = [this, editActive] (float v) { editActive ([v] (Transformer& t) { t.depth = v; }); };
    depthKnob.valueText = [] (float v) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; };
    depthKnob.setValueField (true);
    addAndMakeVisible (depthKnob);
}

void TransformerPanel::rebuildTargets()
{
    targetRefs.clear();
    targetBox.clear (juce::dontSendNotification);
    int id = 1;
    targetRefs.push_back ({ 1, 0, 0 }); targetBox.addItem ("Pre-Amp (in)",  id++);
    targetRefs.push_back ({ 2, 0, 0 }); targetBox.addItem ("Post-Amp (out)", id++);
    if (engine == nullptr) return;
    const auto& slots = engine->rack().slots();
    for (int s = 0; s < kNumSlots; ++s)
    {
        const auto& info = fxInfo (slots[s].type);
        if (! info.implemented) continue;
        for (int p = 0; p < (int) info.params.size(); ++p)
        {
            if (info.params[p].isSeg()) continue;                       // skip discrete
            if (slots[s].type == FxType::Delay && p == 0) continue;     // skip delay time
            targetRefs.push_back ({ 0, s, p });
            targetBox.addItem (juce::String (s + 1) + " " + info.name + " " + info.params[p].label, id++);
        }
    }
}

juce::Rectangle<float> TransformerPanel::graphBounds() const
{
    if (overlay) return {};
    auto inner = getLocalBounds().reduced (13, 9);
    inner.removeFromTop (18 + 6);        // header + gap
    inner.removeFromBottom (74 + 6);     // controls band + gap
    return inner.toFloat();
}

void TransformerPanel::refresh()
{
    if (engine == nullptr) return;
    rebuildTargets();   // keep the target list (labels + slot indices) in sync after a reorder
    const auto& t = engine->transformers()[(size_t) active];

    for (int i = 0; i < laneBtns.size(); ++i)
    {
        const bool on  = engine->transformers()[(size_t) i].on;
        const bool act = (i == active);
        // Active (displayed) lane = bright orange; other ON lanes = faded orange so the user can
        // see which selected transformer is currently shown; OFF lanes stay neutral.
        const Colour bg     = act ? colour::accent : on ? colour::accent.withAlpha (0.34f) : colour::buttonNeutral2;
        const Colour border = act ? colour::accentLight2 : on ? colour::accent.withAlpha (0.55f) : colour::borderSubtle;
        const Colour text   = act ? Colour (0xff1a1410) : on ? colour::accentLight2 : colour::faint;
        laneBtns[i]->setColours (bg, border, text);
    }
    auto lit = [] (FlatButton& b, bool on)
    {
        b.setColours (on ? colour::accent : colour::buttonNeutral2,
                      on ? colour::accentLight : colour::borderSubtle,
                      on ? Colour (0xff1a1410) : colour::faint);
    };
    lit (onBtn, t.on); lit (basisOne, t.basis == 0); lit (basisCyc, t.basis == 1);
    lit (viewBtn, overlay);

    for (int i = 0; i < (int) targetRefs.size(); ++i)
    {
        const auto& r = targetRefs[(size_t) i];
        if ((int) t.kind == r.kind && (r.kind != 0 || (r.slot == t.slot && r.param == t.param)))
        { targetBox.setSelectedId (i + 1, juce::dontSendNotification); break; }
    }
    shapeBox.setSelectedId (t.shapeType >= 0 ? t.shapeType + 1 : kNumShapes + 1, juce::dontSendNotification);
    rateBox.setSelectedId (t.rateDiv + 1, juce::dontSendNotification);
    depthKnob.setValue (t.depth);
    freqKnob.setValue (juce::jlimit (0.0f, 1.0f, std::log (juce::jmax (0.1f, t.freqHz) / 0.1f) / std::log (200.0f)));
    phaseKnob.setValue (t.phase);

    const bool cyc = (t.basis == 1);
    rateBox.setVisible (cyc);
    freqKnob.setVisible (cyc); phaseKnob.setVisible (cyc);
    freqKnob.setInert (! cyc);    // live in cyclic; turning it switches the rate to Free-Hz
    phaseKnob.setInert (! cyc);
    repaint();
}

void TransformerPanel::resized()
{
    auto inner = getLocalBounds().reduced (13, 9);
    auto header = inner.removeFromTop (18);
    auto lanes = header.removeFromRight (8 * 22 + 7 * 2);
    for (auto* b : laneBtns) { b->setBounds (lanes.removeFromLeft (22).withSizeKeepingCentre (22, 16)); lanes.removeFromLeft (2); }
    header.removeFromRight (8);
    viewBtn.setBounds (header.removeFromRight (64).withSizeKeepingCentre (64, 16));

    // Controls band runs the full width UNDERNEATH the graph: knobs on the right, combos +
    // toggles on the left. The graph (separate mode) fills everything above this band.
    auto controls = inner.removeFromBottom (74);
    auto knobs = controls.removeFromRight (172);
    depthKnob.setBounds (knobs.removeFromLeft (52).withSizeKeepingCentre (52, 70)); knobs.removeFromLeft (8);
    freqKnob.setBounds (knobs.removeFromLeft (52).withSizeKeepingCentre (52, 70)); knobs.removeFromLeft (8);
    phaseKnob.setBounds (knobs.removeFromLeft (52).withSizeKeepingCentre (52, 70));
    controls.removeFromRight (16);

    targetBox.setBounds (controls.removeFromTop (22).removeFromLeft (300)); controls.removeFromTop (4);
    shapeBox.setBounds (controls.removeFromTop (22).removeFromLeft (220)); controls.removeFromTop (4);
    auto br = controls.removeFromTop (22);
    onBtn.setBounds (br.removeFromLeft (34).withSizeKeepingCentre (34, 22)); br.removeFromLeft (6);
    basisOne.setBounds (br.removeFromLeft (46)); br.removeFromLeft (3);
    basisCyc.setBounds (br.removeFromLeft (46)); br.removeFromLeft (8);
    rateBox.setBounds (br.removeFromLeft (62).withSizeKeepingCentre (62, 22));
}

void TransformerPanel::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    drawPanel (g, b, colour::panel, colour::border);
    sectionLabel (g, "TRANSFORMERS", getLocalBounds().reduced (13, 9).removeFromTop (14).removeFromLeft (150), colour::dim, 9.0f);
    if (engine == nullptr) return;

    if (! overlay)
    {
        auto gb = graphBounds();
        drawPanel (g, gb, colour::well, colour::borderSubtle, 3.0f);
        auto wa = gb.reduced (8.0f, 6.0f);   // same inset the SAMPLE waveform uses -> time axes line up
        g.setColour (Colour (0x10ffffff)); g.fillRect (wa.getX(), wa.getCentreY(), wa.getWidth(), 1.0f);
        const auto& t = engine->transformers()[(size_t) active];
        drawTransformerShape (g, wa, t, engine->lengthSeconds(), engine->tempo(),
                              engine->positionSeconds(), engine->isPlaying(), true);
        g.setColour (colour::faint2); g.setFont (monoFont (8.0f));
        g.drawText ("drag to draw", wa.toNearestInt().reduced (3), Justification::topLeft);
    }
    else
    {
        auto hint = getLocalBounds().reduced (13, 9); hint.removeFromTop (22);
        g.setColour (colour::faint2); g.setFont (monoFont (8.5f));
        g.drawText ("curve drawn over the OUTPUT waveform above", hint.removeFromTop (14), Justification::centredLeft);
    }
}

void TransformerPanel::mouseDown (const juce::MouseEvent& e) { drawCurveTo (e); }
void TransformerPanel::mouseDrag (const juce::MouseEvent& e) { drawCurveTo (e); }

void TransformerPanel::drawCurveTo (const juce::MouseEvent& e)
{
    if (overlay) return;
    if (engine == nullptr) return;
    auto wa = graphBounds().reduced (8.0f, 6.0f);
    if (! wa.contains (e.position)) return;
    const float fx = juce::jlimit (0.0f, 1.0f, (e.position.x - wa.getX()) / wa.getWidth());
    const float fy = juce::jlimit (0.0f, 1.0f, 1.0f - (e.position.y - wa.getY()) / wa.getHeight());
    auto t = engine->transformers()[(size_t) active];
    const int idx = juce::jlimit (0, kCurvePoints - 1, (int) std::round (fx * (kCurvePoints - 1)));
    t.curve[(size_t) idx] = fy;
    if (idx > 0) t.curve[(size_t) (idx - 1)] = (t.curve[(size_t) (idx - 1)] + fy) * 0.5f;   // smooth gaps
    if (idx < kCurvePoints - 1) t.curve[(size_t) (idx + 1)] = (t.curve[(size_t) (idx + 1)] + fy) * 0.5f;
    t.shapeType = -1;   // user-drawn now
    engine->setTransformer (active, t);
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
        k->setValueField (true);
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
        auto ms = [] (double scale) { return [scale] (float v) { return juce::String (juce::roundToInt (v * scale)) + " ms"; }; };
        fiK = addKnob ("Fade In",  (float) (p.fadeInMs  / 250.0), false,
                       [this] (float v) { engine->setFadeInMs (v * 250.0); });
        fiK->valueText = ms (250.0);
        foK = addKnob ("Fade Out", (float) (p.fadeOutMs / 250.0), false,
                       [this] (float v) { engine->setFadeOutMs (v * 250.0); });
        foK->valueText = ms (250.0);
        ofiK = addKnob ("Fade In 2",  (float) (p.outFadeInMs  / 500.0), false,
                        [this] (float v) { engine->setOutFadeInMs (v * 500.0); });
        ofiK->valueText = ms (500.0);
        ofoK = addKnob ("Fade Out 2", (float) (p.outFadeOutMs / 4000.0), false,
                        [this] (float v) { engine->setOutFadeOutMs (v * 4000.0); });
        ofoK->valueText = ms (4000.0);
        addToggle ("Auto-Detect", [] {})                                     // M2-later
            ->setColours (colour::panelAlt, colour::borderSubtle, colour::faint2);
    }
    else if (mode == 1) // SHAPE
    {
        gainK = addKnob ("Gain", (float) ((p.gainDb + 24.0) / 48.0), true,
                         [this] (float v) { engine->setGainDb (v * 48.0 - 24.0); });
        gainK->valueText = [] (float v) { return juce::String (v * 48.0f - 24.0f, 1) + " dB"; };
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
    auto row = inner.removeFromTop (70);
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
int RackPanel::maxScroll() const
{
    const int contentH = kNumSlots * dim::fxRowH;
    const int viewH = getHeight() - 39;
    return juce::jmax (0, contentH - viewH);
}

int RackPanel::rowAt (juce::Point<int> p) const
{
    if (p.y < 39) return -1;
    const int r = (p.y - 39 + scrollY) / dim::fxRowH;
    return (r >= 0 && r < kNumSlots) ? r : -1;
}

void RackPanel::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    const int prev = scrollY;
    scrollY = juce::jlimit (0, maxScroll(), scrollY - juce::roundToInt (w.deltaY * (float) dim::fxRowH * 3.0f));
    if (scrollY != prev) repaint();
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

    scrollY = juce::jlimit (0, maxScroll(), scrollY);
    Graphics::ScopedSaveState clip (g);
    g.reduceClipRegion (0, 39, getWidth(), getHeight() - 39);   // keep rows out of the header

    int y = 39 - scrollY;
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

    // Scrollbar thumb when the stack overflows.
    if (const int ms = maxScroll(); ms > 0)
    {
        const float viewH = (float) (getHeight() - 39);
        const float contentH = (float) (kNumSlots * dim::fxRowH);
        const float thumbH = juce::jmax (24.0f, viewH * viewH / contentH);
        const float thumbY = 39.0f + (viewH - thumbH) * (float) scrollY / (float) ms;
        g.setColour (Colour (0x33ffffff));
        g.fillRoundedRectangle ((float) getWidth() - 5.0f, thumbY, 3.0f, thumbH, 1.5f);
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
    const int target = juce::jlimit (0, kNumSlots - 1, (e.getPosition().y - 39 + scrollY) / dim::fxRowH);
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
    scrollY = 0;   // start each effect's editor at the top
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
            k->setValueField (true);
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

int DetailPanel::graphHeight() const
{
    // Squeeze toward 200 when the panel is tall; floor at 120 so the controls
    // (not the graph) are what scrolls when the panel is short.
    int segH = 0;
    for (size_t grp = 0; grp < segParams.size(); ++grp) segH += 8 + 24;
    const int avail = getHeight() - 39 - 24;     // header + top/bottom (12 each) margins
    return juce::jlimit (120, 200, avail - (12 + 70 + segH));
}

int DetailPanel::contentHeight() const
{
    int segH = 0;
    for (size_t grp = 0; grp < segParams.size(); ++grp) segH += 8 + 24;
    return 39 + 12 + graphHeight() + 12 + 70 + segH + 12;   // header + margins + graph + controls
}

int DetailPanel::maxScroll() const
{
    return juce::jmax (0, contentHeight() - getHeight());
}

void DetailPanel::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    const int prev = scrollY;
    scrollY = juce::jlimit (0, maxScroll(), scrollY - juce::roundToInt (w.deltaY * 120.0f));
    if (scrollY != prev) { resized(); repaint(); }
}

void DetailPanel::resized()
{
    scrollY = juce::jlimit (0, maxScroll(), scrollY);
    auto body = getLocalBounds().withTrimmedTop (39).reduced (13, 12);
    body.setY (body.getY() - scrollY);
    body.setHeight (contentHeight());            // natural height so rows below land correctly
    body.removeFromTop (graphHeight());
    body.removeFromTop (12);
    auto row = body.removeFromTop (70);
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
    body.setY (body.getY() - scrollY);
    return body.removeFromTop (graphHeight()).toFloat();
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

    {
        Graphics::ScopedSaveState clip (g);
        g.reduceClipRegion (0, 39, getWidth(), getHeight() - 39);   // scrolled content stays below the header

        auto graph = graphBounds();
        const bool dyn = impl && (slot.type == FxType::Compression || slot.type == FxType::Limiter);
        juce::Rectangle<float> meterArea;
        if (dyn) { meterArea = graph.removeFromRight (90.0f); graph.removeFromRight (10.0f); }   // OUT + GR meters
        drawPanel (g, graph, colour::well, colour::borderSubtle, 3.0f);
        g.setColour (Colour (0x0dffffff));
        g.fillRect (graph.getX(), graph.getCentreY(), graph.getWidth(), 1.0f);
        g.fillRect (graph.getCentreX(), graph.getY(), 1.0f, graph.getHeight());

        if (impl)
        {
            // While playing, morph the graph with the transformer modulation at the playhead.
            const bool morphing = engine->isSlotModulated (engine->selectedSlot()) && engine->isPlaying();
            std::vector<float> live;
            if (morphing) live = engine->liveParams (engine->selectedSlot());
            drawFxGraph (g, graph, slot, morphing ? &live : nullptr);

            if (morphing)
            {
                auto tag = graph.withTrimmedTop (4.0f).withTrimmedRight (6.0f).removeFromTop (12).removeFromRight (54);
                g.setColour (colour::accent); g.setFont (monoFont (8.0f, true));
                g.drawText ("~ LIVE", tag, Justification::centredRight);
            }
        }
        else
        {
            g.setColour (colour::faint); g.setFont (monoFont (11.0f));
            g.drawText ("passthrough - full effect arrives in v1.5", graph.toNearestInt(), Justification::centred);
        }

        // Output-level + gain-reduction meters for the dynamics processors. Live at the playhead
        // while playing; otherwise hold the peak/max from the last render.
        if (dyn)
        {
            const int   s       = engine->selectedSlot();
            const bool  playing = engine->isPlaying();
            const int   pos     = (int) (engine->positionSeconds() * engine->sampleRate());
            const float outDb   = playing ? engine->meterOutDbAt (s, pos) : engine->meterPeakOutDb (s);
            const float grDb    = playing ? engine->meterGrDbAt  (s, pos) : engine->meterMaxGrDb   (s);
            const float peakOut = engine->meterPeakOutDb (s);
            const float maxGr   = engine->meterMaxGrDb   (s);

            auto bar = [&] (juce::Rectangle<float> c, const char* lab, float frac, bool downward,
                            Colour fill, juce::String txt, float holdFrac)
            {
                auto labR = c.removeFromBottom (12);
                auto valR = c.removeFromBottom (13); c.removeFromBottom (2);
                drawPanel (g, c, colour::well2, colour::borderSubtle, 2.0f);
                auto inner = c.reduced (2.0f);
                frac = juce::jlimit (0.0f, 1.0f, frac);
                auto f = inner; auto fillR = downward ? f.removeFromTop (inner.getHeight() * frac)
                                                      : f.removeFromBottom (inner.getHeight() * frac);
                g.setColour (fill); g.fillRect (fillR);
                holdFrac = juce::jlimit (0.0f, 1.0f, holdFrac);                // peak/max hold line
                const float hy = downward ? inner.getY() + inner.getHeight() * holdFrac
                                          : inner.getBottom() - inner.getHeight() * holdFrac;
                g.setColour (fill.brighter (0.5f)); g.fillRect (inner.getX(), hy - 0.5f, inner.getWidth(), 1.0f);
                g.setColour (colour::faint);  g.setFont (monoFont (8.0f, true)); g.drawText (lab, labR, Justification::centred);
                g.setColour (colour::ink);    g.setFont (monoFont (8.5f, true)); g.drawText (txt, valR, Justification::centred);
            };

            auto col   = meterArea;
            auto outCol = col.removeFromLeft (col.getWidth() * 0.5f).reduced (3.0f, 0.0f);
            auto grCol  = col.reduced (3.0f, 0.0f);
            const float outFrac = juce::jlimit (0.0f, 1.0f, (outDb + 48.0f) / 48.0f);   // -48..0 dBFS
            const float pkFrac  = juce::jlimit (0.0f, 1.0f, (peakOut + 48.0f) / 48.0f);
            const float grSpan  = 18.0f;                                                // 0..-18 dB GR
            bar (outCol, "OUT", outFrac, false, colour::accent,
                 outDb <= -119.0f ? juce::String ("--") : juce::String (juce::roundToInt (outDb)),
                 pkFrac);
            bar (grCol, "GR", grDb / grSpan, true, Colour (0xffd49a52),
                 grDb < 0.05f ? juce::String ("0") : "-" + juce::String (juce::roundToInt (grDb)),
                 maxGr / grSpan);
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

    // Scrollbar thumb when the controls overflow.
    if (const int ms = maxScroll(); ms > 0)
    {
        const float viewH = (float) (getHeight() - 39);
        const float contentH = (float) (contentHeight() - 39);
        const float thumbH = juce::jmax (24.0f, viewH * viewH / contentH);
        const float thumbY = 39.0f + (viewH - thumbH) * (float) scrollY / (float) ms;
        g.setColour (Colour (0x33ffffff));
        g.fillRoundedRectangle ((float) getWidth() - 5.0f, thumbY, 3.0f, thumbH, 1.5f);
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
    trackRect = track;   // cache for dragging
    drawPanel (g, track, colour::well2, colour::borderSubtle, 4.0f);

    const float severity = depth; // live mutation-severity value
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

    // affects chips (lit = enabled scope)
    auto r = affectsRow;
    const char* chips[] = { "Everything", "Plug-ins", "Filter", "Envelopes", "Dynamics",
                            "Crossfades", "Timing", "Pitch", "Mangle", "Tail" };
    for (int i = 0; i < 10; ++i)
    {
        const String label (chips[i]);
        const int w = label.length() * 6 + 18; // mono ~6px/char at 9px
        auto chip = r.removeFromLeft (w).withSizeKeepingCentre (w, 22); r.removeFromLeft (6);
        chipRects[i] = chip;
        if (scopeFlags[i])
            pseudoButton (g, chip.toFloat(), label, colour::accent, colour::accentLight, Colour (0xff1a1410), 9.0f);
        else
            pseudoButton (g, chip.toFloat(), label, colour::accentTint.withAlpha (0.12f),
                          colour::accent.withAlpha (0.4f), Colour (0xffcf9a6a), 9.0f);
    }
}

void MutateStrip::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < 10; ++i)
        if (chipRects[i].contains (e.getPosition()))
        {
            if (i == 0)   // Everything is the master: turn it on, clear the rest
            {
                scopeFlags[0] = true;
                for (int k = 1; k < 10; ++k) scopeFlags[k] = false;
            }
            else
            {
                scopeFlags[i] = ! scopeFlags[i];
                scopeFlags[0] = false;                       // a specific scope means "not Everything"
                bool anySpecific = false;
                for (int k = 1; k < 10; ++k) anySpecific |= scopeFlags[k];
                if (! anySpecific) scopeFlags[0] = true;     // none left -> back to Everything
            }
            repaint();
            if (onChanged) onChanged();
            return;
        }
    mouseDrag (e);   // otherwise treat as a depth-slider grab
}

void MutateStrip::mouseDrag (const juce::MouseEvent& e)
{
    if (trackRect.isEmpty()) return;
    if (! trackRect.expanded (4.0f, 10.0f).contains (e.position)) return;
    depth = juce::jlimit (0.0f, 1.0f, (e.position.x - trackRect.getX()) / trackRect.getWidth());
    repaint();
    if (onChanged) onChanged();
}

// ===================== VARIATIONS (M4) =====================
int VariationsPanel::maxScroll() const
{
    const int total = vars ? (int) vars->size() : 0;
    const int viewH = getHeight() - listTop() - footerH();
    return juce::jmax (0, total * dim::varRowH - viewH);
}

int VariationsPanel::rowAt (juce::Point<int> p) const
{
    if (p.y < listTop() || p.y >= getHeight() - footerH()) return -1;
    const int r = (p.y - listTop() + scrollY) / dim::varRowH;
    const int total = vars ? (int) vars->size() : 0;
    return (r >= 0 && r < total) ? r : -1;
}

juce::Rectangle<int> VariationsPanel::mutateBtn() const { const int ft = getHeight() - footerH(); return { 14, ft + 12, getWidth() - 28, 42 }; }
juce::Rectangle<int> VariationsPanel::keepBtn()   const { const int ft = getHeight() - footerH(); return { getWidth() - 14 - 110, ft + 64, 110, 24 }; }
juce::Rectangle<int> VariationsPanel::writeBtn()  const { const int ft = getHeight() - footerH(); return { 14, ft + 64, (getWidth() - 28) - 110 - 8, 24 }; }

void VariationsPanel::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    const int prev = scrollY;
    scrollY = juce::jlimit (0, maxScroll(), scrollY - juce::roundToInt (w.deltaY * (float) dim::varRowH * 3.0f));
    if (scrollY != prev) repaint();
}

void VariationsPanel::mouseDown (const juce::MouseEvent& e)
{
    if (mutateBtn().contains (e.getPosition())) { if (onMutate) onMutate(); return; }
    if (writeBtn().contains (e.getPosition()))  { if (onWrite) onWrite(); return; }
    if (keepBtn().contains (e.getPosition()))   { if (onKeepPlaying) onKeepPlaying(); return; }

    const int row = rowAt (e.getPosition());
    if (row < 0) return;
    const int W = getWidth();
    if (e.x >= W - 13 - 22)                                   { if (onToggleMute) onToggleMute (row); }    // M
    else if (e.x >= W - 13 - 22 - 8 - 22 && e.x < W - 13 - 22 - 8) { if (onToggleSelect) onToggleSelect (row); } // fav
    else                                                     { if (onRecall) onRecall (row); }             // recall recipe + audition
}

void VariationsPanel::paint (Graphics& g)
{
    g.setColour (colour::panelAlt);
    g.fillRect (getLocalBounds());

    const int total = vars ? (int) vars->size() : 0;
    int selected = 0, candidates = 0;
    if (vars) for (auto& v : *vars) { if (v.selected) ++selected; if (! v.baseline) ++candidates; }
    constexpr int kMaxFav = 8;

    // header
    auto header = getLocalBounds().removeFromTop (48).reduced (14, 0);
    g.setColour (juce::Colours::black); g.fillRect (0, 47, getWidth(), 1);
    sectionLabel (g, "VARIATIONS", header.removeFromLeft (110), colour::dim, 11.0f);
    g.setColour (colour::faint); g.setFont (monoFont (9.5f));
    g.drawText (status.isNotEmpty() ? status : (String (candidates) + " candidates"),
                header.removeFromLeft (130), Justification::centredLeft);
    g.setColour (selected >= kMaxFav ? colour::accent : colour::success); g.setFont (monoFont (13.0f, true));
    g.drawText (String (selected) + " / " + String (kMaxFav), header.removeFromRight (60), Justification::centredRight);
    header.removeFromRight (8);
    g.setColour (colour::faint); g.setFont (monoFont (9.0f));
    g.drawText ("FAVOURITES", header.removeFromRight (78), Justification::centredRight);

    // list (scrolled, clipped between header and footer)
    {
        Graphics::ScopedSaveState clip (g);
        g.reduceClipRegion (0, listTop(), getWidth(), getHeight() - listTop() - footerH());

        if (total == 0)
        {
            g.setColour (colour::faint2); g.setFont (monoFont (10.0f));
            g.drawText ("press MUTATE to generate variations",
                        juce::Rectangle<int> (0, listTop(), getWidth(), 40), Justification::centred);
        }

        int y = listTop() - scrollY;
        for (int i = 0; i < total; ++i, y += dim::varRowH)
        {
            const auto& v = (*vars)[(size_t) i];
            juce::Rectangle<int> rowR (0, y, getWidth(), dim::varRowH);
            const bool sel = v.selected;
            const bool active = (i == activeRow);
            const bool base = v.baseline;
            if (base)        { g.setColour (Colour (0xff211f1b)); g.fillRect (rowR); }     // pinned-anchor tint
            else if (sel)    { g.setColour (colour::accentTint.withAlpha (0.07f)); g.fillRect (rowR); }
            if (active)      { g.setColour (colour::accent.withAlpha (0.12f)); g.fillRect (rowR); }   // LIVE row
            g.setColour (active ? colour::accentLight : (sel ? colour::accent : juce::Colours::transparentBlack));
            g.fillRect (rowR.getX(), rowR.getY(), 3, rowR.getHeight());
            g.setColour (colour::hairline); g.fillRect (rowR.getX(), rowR.getBottom() - 1, rowR.getWidth(), 1);

            auto row = rowR.reduced (13, 0);
            auto play = row.removeFromLeft (24).withSizeKeepingCentre (24, 24).toFloat();
            g.setColour (active ? colour::accent : colour::buttonNeutral2); g.fillEllipse (play);
            g.setColour (active ? colour::accentLight : Colour (0xff3a3833)); g.drawEllipse (play.reduced (0.5f), 1.0f);
            g.setColour (active ? Colour (0xff1a1410) : Colour (0xffa8a59d)); g.setFont (uiFont (8.0f));
            g.drawText (active ? "*" : ">", play, Justification::centred);
            row.removeFromLeft (9);
            g.setColour (active ? colour::accentLight : (sel ? colour::accent : colour::faint));
            g.setFont (monoFont (10.0f, sel || active || base));
            g.drawText (base ? String ("00") : String (i).paddedLeft ('0', 2), row.removeFromLeft (18), Justification::centred);
            row.removeFromLeft (9);

            auto mute = row.removeFromRight (22).withSizeKeepingCentre (22, 22).toFloat();
            drawPanel (g, mute, v.muted ? colour::accentTint : colour::buttonNeutral2, Colour (0xff38352f), 4.0f);
            g.setColour (v.muted ? colour::accent : Colour (0xff4a4843)); g.setFont (monoFont (9.0f, true));
            g.drawText ("M", mute, Justification::centred);
            row.removeFromRight (8);
            auto fav = row.removeFromRight (22).withSizeKeepingCentre (22, 22).toFloat();
            g.setColour (sel ? colour::accent.withAlpha (0.25f) : colour::buttonNeutral2); g.fillEllipse (fav);
            g.setColour (sel ? colour::accent : Colour (0xff38352f)); g.drawEllipse (fav.reduced (0.5f), 1.0f);
            row.removeFromRight (8);
            g.setColour (sel ? Colour (0xffcfccc4) : colour::faint); g.setFont (monoFont (10.0f));
            g.drawText (v.name, row.removeFromRight (104), Justification::centredLeft);

            // mini-waveform from the candidate's peaks
            auto wv = row.toFloat().reduced (4.0f, 8.0f);
            const auto& pk = v.peaks;
            if (! pk.empty() && wv.getWidth() > 2.0f)
            {
                g.setColour ((sel ? colour::accent : colour::faint).withAlpha (v.muted ? 0.3f : 1.0f));
                const int cols = (int) wv.getWidth();
                for (int c = 0; c < cols; ++c)
                {
                    const int pi = juce::jlimit (0, (int) pk.size() - 1, c * (int) pk.size() / juce::jmax (1, cols));
                    const float bh = juce::jmax (1.0f, pk[(size_t) pi] * wv.getHeight());
                    g.fillRect (wv.getX() + (float) c, wv.getCentreY() - bh * 0.5f, 1.0f, bh);
                }
            }
        }
    }

    // footer
    const int ft = getHeight() - footerH();
    g.setColour (colour::panelAlt2); g.fillRect (0, ft, getWidth(), footerH());
    g.setColour (juce::Colours::black); g.fillRect (0, ft, getWidth(), 1);

    auto gen = mutateBtn().toFloat();
    drawPanel (g, gen, colour::buttonNeutral2, colour::accent);
    g.setColour (colour::accent); g.setFont (monoFont (12.5f, true));
    g.drawText ("MUTATE", gen.removeFromLeft (gen.getWidth() * 0.46f), Justification::centred);
    g.setColour (Colour (0xff8c8980)); g.setFont (monoFont (8.5f));
    g.drawText (candidates > 0 ? (String (candidates) + " candidates") : "generate a batch", gen, Justification::centred);

    pseudoButton (g, writeBtn().toFloat(), "WRITE SELECTED  ->",
                  colour::accent, colour::accentLight, Colour (0xff1a1410), 11.0f);
    pseudoButton (g, keepBtn().toFloat(), "KEEP PLAYING",
                  colour::buttonNeutral, colour::border, Colour (0xffbfbcb5), 10.5f);
}

// ===================== INPUTS browser (M3b) =====================
namespace { constexpr int kInRowH = 28; }

InputsPanel::InputsPanel()
{
    auto start = juce::File::getSpecialLocation (juce::File::userMusicDirectory);
    if (! start.isDirectory()) start = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
    setFolder (start);
}

void InputsPanel::setFolder (const juce::File& f)
{
    if (! f.isDirectory()) return;
    folder = f;
    scrollY = 0;
    refreshListing();
    repaint();
}

void InputsPanel::refreshListing()
{
    dirs.clear(); files.clear();
    if (folder.isDirectory())
    {
        dirs  = folder.findChildFiles (juce::File::findDirectories, false);
        files = folder.findChildFiles (juce::File::findFiles, false, "*.wav;*.aif;*.aiff");
        auto byName = [] (const juce::File& a, const juce::File& b) { return a.getFileName().compareIgnoreCase (b.getFileName()) < 0; };
        std::sort (dirs.begin(),  dirs.end(),  byName);
        std::sort (files.begin(), files.end(), byName);
    }
    const auto parent = folder.getParentDirectory();
    hasParent = (parent.isDirectory() && parent != folder);
}

int InputsPanel::rowCount() const { return (hasParent ? 1 : 0) + dirs.size() + files.size(); }

int InputsPanel::maxScroll() const
{
    return juce::jmax (0, rowCount() * kInRowH - (getHeight() - listTop()));
}

int InputsPanel::rowAt (juce::Point<int> p) const
{
    if (p.y < listTop()) return -1;
    const int r = (p.y - listTop() + scrollY) / kInRowH;
    return (r >= 0 && r < rowCount()) ? r : -1;
}

juce::Rectangle<int> InputsPanel::folderBtnBounds() const
{
    return juce::Rectangle<int> (getWidth() - 13 - 64, 11, 64, 18);
}

void InputsPanel::activateRow (int row)
{
    if (row < 0) return;
    if (hasParent && row == 0) { setFolder (folder.getParentDirectory()); return; }
    const int di = row - (hasParent ? 1 : 0);
    if (di < dirs.size()) { setFolder (dirs[di]); return; }
    const int fi = di - dirs.size();
    if (fi >= 0 && fi < files.size())
    {
        loaded = files[fi];
        if (onLoadFile) onLoadFile (loaded);
        repaint();
    }
}

void InputsPanel::step (int dir)
{
    if (files.isEmpty()) return;
    const int cur = files.indexOf (loaded);
    const int next = (cur < 0) ? (dir > 0 ? 0 : files.size() - 1)
                               : juce::jlimit (0, files.size() - 1, cur + dir);
    loaded = files[next];
    if (onLoadFile) onLoadFile (loaded);

    const int y = ((hasParent ? 1 : 0) + dirs.size() + next) * kInRowH;   // keep it visible
    const int viewH = getHeight() - listTop();
    if (y < scrollY) scrollY = y;
    else if (y + kInRowH > scrollY + viewH) scrollY = y + kInRowH - viewH;
    scrollY = juce::jlimit (0, maxScroll(), scrollY);
    repaint();
}

void InputsPanel::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    const int prev = scrollY;
    scrollY = juce::jlimit (0, maxScroll(), scrollY - juce::roundToInt (w.deltaY * (float) kInRowH * 3.0f));
    if (scrollY != prev) repaint();
}

void InputsPanel::mouseMove (const juce::MouseEvent& e)
{
    const int r = rowAt (e.getPosition());
    if (r != hoverRow) { hoverRow = r; repaint(); }
}

void InputsPanel::mouseDown (const juce::MouseEvent& e)
{
    if (folderBtnBounds().contains (e.getPosition()))
    {
        chooser = std::make_unique<juce::FileChooser> ("Choose a samples folder", folder);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                              [this] (const juce::FileChooser& fc) { auto d = fc.getResult(); if (d.isDirectory()) setFolder (d); });
        return;
    }
    activateRow (rowAt (e.getPosition()));
}

void InputsPanel::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    drawPanel (g, b, colour::panel, colour::border);
    sectionLabel (g, "INPUTS", getLocalBounds().reduced (13, 11).removeFromTop (14).removeFromLeft (80), colour::accent);

    pseudoButton (g, folderBtnBounds().toFloat(), "FOLDER", colour::buttonNeutral2, Colour (0xff4a4640), colour::faint, 8.0f);

    g.setColour (colour::faint); g.setFont (monoFont (8.5f));
    g.drawText (folder.getFullPathName(), 13, 30, getWidth() - 26, 14, Justification::centredLeft, true);

    g.setColour (colour::borderSubtle);
    g.fillRect (0.0f, (float) listTop() - 4.0f, (float) getWidth(), 1.0f);

    if (rowCount() == 0)
    {
        g.setColour (colour::faint2); g.setFont (monoFont (10.0f));
        g.drawText ("no audio files in this folder", getLocalBounds().withTrimmedTop (listTop()), Justification::centredTop);
        return;
    }

    Graphics::ScopedSaveState clip (g);
    g.reduceClipRegion (0, listTop(), getWidth(), getHeight() - listTop());

    int y = listTop() - scrollY;
    auto drawRow = [&] (const String& text, bool isDir, bool isParent, bool sel, int rowIdx)
    {
        Rectangle<int> rr (0, y, getWidth(), kInRowH - 1);
        if (sel)                     { g.setColour (colour::accentTint); g.fillRect (rr); g.setColour (colour::accent); g.fillRect (rr.removeFromLeft (3)); }
        else if (rowIdx == hoverRow) { g.setColour (Colour (0x14ffffff)); g.fillRect (rr); rr.removeFromLeft (3); }
        else                           rr.removeFromLeft (3);
        g.setColour (colour::hairline); g.fillRect (0, y + kInRowH - 1, getWidth(), 1);

        auto row = rr.reduced (10, 0);
        g.setColour (isDir ? colour::accentLight2 : colour::faint2);
        g.setFont (monoFont (9.0f, true));
        g.drawText (isParent ? "[..]" : (isDir ? "[+]" : "~"), row.removeFromLeft (22), Justification::centredLeft);
        g.setColour (sel ? colour::ink : (isDir ? Colour (0xffcfccc4) : colour::dim));
        g.setFont (uiFont (11.0f, sel));
        g.drawText (text, row, Justification::centredLeft);
        y += kInRowH;
    };

    int idx = 0;
    if (hasParent) { drawRow ("..", true, true, false, idx); ++idx; }
    for (auto& d : dirs)  { drawRow (d.getFileName(), true,  false, false, idx); ++idx; }
    for (auto& f : files) { drawRow (f.getFileName(), false, false, f == loaded, idx); ++idx; }

    if (const int ms = maxScroll(); ms > 0)
    {
        const float viewH = (float) (getHeight() - listTop());
        const float contentH = (float) (rowCount() * kInRowH);
        const float thumbH = juce::jmax (24.0f, viewH * viewH / contentH);
        const float thumbY = (float) listTop() + (viewH - thumbH) * (float) scrollY / (float) ms;
        g.setColour (Colour (0x33ffffff));
        g.fillRoundedRectangle ((float) getWidth() - 5.0f, thumbY, 3.0f, thumbH, 1.5f);
    }
}
}
