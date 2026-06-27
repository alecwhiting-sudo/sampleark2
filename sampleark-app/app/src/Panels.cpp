#include "Panels.h"
#include "Theme.h"
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
} // namespace

namespace sa
{
// ===================== TOP BAR =====================
void TopBar::paint (Graphics& g)
{
    auto b = getLocalBounds();
    g.setColour (colour::panelAlt);
    g.fillRect (b);
    g.setColour (juce::Colours::black);
    g.fillRect (b.removeFromBottom (1));

    auto r = getLocalBounds().reduced (12, 0);
    const int btnH = 28;
    auto centreV = [&] (Rectangle<int> a) { return a.withSizeKeepingCentre (a.getWidth(), btnH); };

    // PLAY (green) / STOP
    pseudoButton (g, centreV (r.removeFromLeft (78)).toFloat(), "> PLAY",
                  colour::playBg, colour::playBorder, colour::playText);
    r.removeFromLeft (9);
    pseudoButton (g, centreV (r.removeFromLeft (64)).toFloat(), "STOP",
                  colour::buttonNeutral, colour::border, Colour (0xffbfbcb5));
    r.removeFromLeft (10);
    g.setColour (Colour (0xff3a3833));
    g.fillRect (r.removeFromLeft (1).withSizeKeepingCentre (1, 24));
    r.removeFromLeft (10);

    // LOAD SAMPLE (click to browse; dropping onto the SAMPLE panel also loads — see M1)
    pseudoButton (g, centreV (r.removeFromLeft (140)).toFloat(), "(+) LOAD SAMPLE",
                  colour::buttonNeutral2, Colour (0xff4a4640), Colour (0xff8c8980), 10.5f);

    // TEMPO chip (right)
    auto tempo = centreV (r.removeFromRight (210)).toFloat();
    drawPanel (g, tempo, colour::well2, colour::borderSubtle2, (float) dim::ctrlRadius);
    auto ti = tempo.reduced (10, 0);
    g.setColour (colour::faint); g.setFont (monoFont (9.0f, true));
    g.drawText ("TEMPO", ti.removeFromLeft (44), Justification::centredLeft);
    g.setColour (colour::ink); g.setFont (monoFont (13.0f, true));
    g.drawText ("120.0", ti.removeFromLeft (70), Justification::centred);
    pseudoButton (g, ti.removeFromRight (52).reduced (0, 4), "DETECT",
                  colour::buttonNeutral2, Colour (0xff3a3833), colour::dim, 9.0f);

    // filename chip (centre, fills remaining)
    auto chip = centreV (r).toFloat().reduced (10.0f, 0.0f);
    drawPanel (g, chip, colour::well2, colour::borderSubtle2, (float) dim::ctrlRadius);
    auto ci = chip.reduced (13, 0);
    g.setColour (colour::ink); g.setFont (monoFont (12.0f));
    g.drawText ("kick_raw_07.wav", ci.removeFromLeft (150), Justification::centredLeft);
    g.setColour (colour::faint); g.setFont (monoFont (10.0f));
    g.drawText ("0.84s   48 kHz - 24b", ci, Justification::centredLeft);
}

// ===================== SOURCE =====================
void SourcePanel::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    drawPanel (g, b, colour::panel, colour::border);
    auto inner = b.reduced (13.0f, 11.0f);

    auto header = inner.removeFromTop (18.0f).toNearestInt();
    sectionLabel (g, "SAMPLE", header.removeFromLeft (70), colour::dim);
    g.setColour (colour::accent);
    g.fillEllipse ((float) header.getX() - 56.0f, (float) header.getCentreY() - 3.5f, 7.0f, 7.0f);
    g.setColour (colour::faint); g.setFont (monoFont (9.0f));
    g.drawText ("48 kHz / 24-bit - 40,320 frames - transient-locked",
                header, Justification::centredRight);

    inner.removeFromTop (9.0f);
    auto well = inner;
    drawPanel (g, well, colour::well, colour::borderSubtle, 3.0f);
    auto w = well.reduced (8.0f, 1.0f);

    // centre line
    g.setColour (Colour (0x10ffffff));
    g.fillRect (w.getX(), w.getCentreY(), w.getWidth(), 1.0f);

    const float startF = 0.06f, endF = 0.92f;
    drawWaveform (g, w, 230, startF, endF, colour::waveOn, colour::waveOff);

    auto xAt = [&] (float f) { return w.getX() + f * w.getWidth(); };
    g.setColour (colour::accent);
    g.fillRect (xAt (startF), well.getY(), 2.0f, well.getHeight());
    g.fillRect (xAt (endF),   well.getY(), 2.0f, well.getHeight());
    g.setColour (colour::yellow);
    g.fillRect (xAt (startF) + 4.0f, well.getY() + 3.0f, 1.0f, 8.0f);
    g.setColour (Colour (0x66e9e7e2));
    g.fillRect (xAt (0.31f), well.getY(), 1.0f, well.getHeight());
}

// ===================== PREP =====================
void PrepPanel::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    drawPanel (g, b, colour::panel, colour::border);
    auto inner = getLocalBounds().reduced (13, 9);

    // left: PREP label + horizontal TRIM | SHAPE | PITCH (compact, saves vertical space)
    auto left = inner.removeFromLeft (188);
    sectionLabel (g, "PREP", left.removeFromTop (12), colour::faint, 9.0f);
    left.removeFromTop (6);
    auto tabsRow = left.removeFromTop (24);
    const char* tabs[] = { "TRIM", "SHAPE", "PITCH" };
    for (int i = 0; i < 3; ++i)
    {
        auto t = tabsRow.removeFromLeft (58); tabsRow.removeFromLeft (4);
        const bool active = (i == 0);
        pseudoButton (g, t.toFloat(), tabs[i],
                      active ? colour::accent : colour::buttonNeutral2,
                      active ? colour::accentLight : Colour (0xff38352f),
                      active ? Colour (0xff1a1410) : colour::faint, 9.0f);
    }

    inner.removeFromLeft (12);
    g.setColour (colour::borderSubtle2);
    g.fillRect (inner.removeFromLeft (1));
    inner.removeFromLeft (12);

    // TRIM controls (knobs) on the right
    struct K { const char* label; float v; bool core; };
    const K knobs[] = { { "Start", 0.06f, true }, { "End", 0.92f, true },
                        { "Transient", 0.62f, false }, { "Fade In", 0.08f, false },
                        { "Fade Out", 0.18f, false } };
    auto row = inner.removeFromTop (58);
    for (auto& k : knobs)
    {
        auto cell = row.removeFromLeft (52); row.removeFromLeft (6);
        knobCell (g, cell, dim::prepKnob, k.v, k.label, k.core);
    }
    row.removeFromLeft (4);
    pseudoButton (g, row.removeFromLeft (78).withHeight (38).toFloat(), "Auto-Detect",
                  colour::accentTint, colour::accent, colour::accent, 9.5f);
}

// ===================== FX RACK =====================
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

    struct Slot { const char* name; const char* sub; bool on; };
    const Slot slots[] = {
        { "Distortion", "drive 55 - tone 50", true },
        { "Bitcrush",   "bits 45 - rate 50",  true },
        { "Compression","thresh 45 - ratio 55", false },
        { "Delay",      "time 40 - feedbk 45", true },
        { "Reverb",     "size 60 - decay 50",  true },
        { "Filter",     "cutoff 66 - reso 42", true },
        { "Limiter",    "ceiling 85 - release 40", true },
        { "Autopan",    "rate 50 - depth 45",  false },
    };
    const int selected = 5; // Filter

    int y = 39;
    for (int i = 0; i < 8; ++i)
    {
        auto rowR = Rectangle<int> (0, y, getWidth(), dim::fxRowH - 1);
        const bool sel = (i == selected);
        if (sel)
        {
            g.setColour (colour::accentTint);
            g.fillRect (rowR);
            g.setColour (colour::accent);
            g.fillRect (rowR.removeFromLeft (3));
        }
        else
        {
            rowR.removeFromLeft (3);
        }
        g.setColour (colour::hairline);
        g.fillRect (0, y + dim::fxRowH - 1, getWidth(), 1);

        auto row = rowR.reduced (9, 0);
        // drag handle
        g.setColour (colour::faint2); g.setFont (monoFont (12.0f));
        g.drawText ("::", row.removeFromLeft (14), Justification::centred);
        row.removeFromLeft (4);
        // LED
        auto led = row.removeFromLeft (13).withSizeKeepingCentre (13, 13).toFloat();
        g.setColour (slots[i].on ? colour::accent : Colour (0xff1a1916));
        g.fillEllipse (led);
        g.setColour (slots[i].on ? colour::accentLight : Colour (0xff3a3833));
        g.drawEllipse (led.reduced (0.5f), 1.0f);
        row.removeFromLeft (8);
        // index
        g.setColour (sel ? colour::accent : (slots[i].on ? colour::dim : Colour (0xff4f4c47)));
        g.setFont (monoFont (10.0f, sel));
        g.drawText (String (i + 1), row.removeFromLeft (14), Justification::centred);
        row.removeFromLeft (4);
        // swap + badge on the right
        pseudoButton (g, row.removeFromRight (20).withSizeKeepingCentre (20, 20).toFloat(), "<>",
                      colour::buttonNeutral2, Colour (0xff38352f), colour::faint, 9.0f);
        row.removeFromRight (6);
        auto badge = row.removeFromRight (40).withSizeKeepingCentre (40, 16).toFloat();
        drawPanel (g, badge, colour::buttonNeutral2, Colour (0xff34322e), 2.0f);
        g.setColour (colour::faint); g.setFont (monoFont (7.5f, true));
        g.drawText ("CORE", badge, Justification::centred);
        // name + summary
        auto txt = row;
        g.setColour (slots[i].on ? (sel ? colour::ink : Colour (0xffcfccc4)) : colour::faint2);
        g.setFont (uiFont (11.5f, true));
        g.drawText (slots[i].name, txt.removeFromTop (txt.getHeight() / 2), Justification::bottomLeft);
        g.setColour (colour::faint); g.setFont (monoFont (8.0f));
        g.drawText (slots[i].sub, txt, Justification::topLeft);

        y += dim::fxRowH;
    }
}

// ===================== EFFECT DETAIL =====================
void DetailPanel::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    drawPanel (g, b, colour::panel, colour::border);

    auto header = getLocalBounds().removeFromTop (38).reduced (13, 0);
    g.setColour (colour::accent);
    g.fillEllipse ((float) header.getX(), (float) header.getCentreY() - 3.5f, 7.0f, 7.0f);
    header.removeFromLeft (14);
    g.setColour (colour::ink); g.setFont (monoFont (11.0f, true));
    g.drawText ("Filter", header.removeFromLeft (60), Justification::centredLeft);
    auto badge = header.removeFromLeft (64).withSizeKeepingCentre (60, 16).toFloat();
    drawPanel (g, badge, colour::buttonNeutral2, Colour (0xff34322e), 2.0f);
    g.setColour (colour::faint); g.setFont (monoFont (7.5f, true));
    g.drawText ("BUILT-IN", badge, Justification::centred);
    g.setColour (colour::faint); g.setFont (monoFont (8.5f));
    g.drawText ("drag curve to sweep", header, Justification::centredRight);

    g.setColour (colour::borderSubtle);
    g.fillRect (0.0f, 38.0f, (float) getWidth(), 1.0f);

    auto body = getLocalBounds().withTrimmedTop (39).reduced (13, 12);
    // Cap the graph height so it doesn't balloon on tall panels; knobs sit just below it.
    const int graphH = juce::jmin (body.getHeight() - 66, 200);
    auto graph = body.removeFromTop (graphH).toFloat();
    body.removeFromTop (12);
    auto knobRow = body.removeFromTop (54);
    drawPanel (g, graph, colour::well, colour::borderSubtle, 3.0f);

    // faint grid
    g.setColour (Colour (0x0dffffff));
    g.fillRect (graph.getX(), graph.getCentreY(), graph.getWidth(), 1.0f);
    g.fillRect (graph.getCentreX(), graph.getY(), 1.0f, graph.getHeight());

    // Filter LP response curve (cutoff 0.66, reso 0.42)
    const float cut = 0.66f, res = 0.42f;
    juce::Path curve;
    const int N = 72;
    for (int i = 0; i <= N; ++i)
    {
        const float x = (float) i / (float) N;
        float m = (x < cut) ? 1.0f : juce::jmax (0.0f, 1.0f - (x - cut) * 4.5f);
        m += res * 0.5f * std::exp (-std::pow ((x - cut) / 0.045f, 2.0f));
        m = juce::jlimit (0.0f, 1.25f, m);
        const float px = graph.getX() + x * graph.getWidth();
        const float py = graph.getY() + (1.0f - m / 1.25f) * graph.getHeight();
        if (i == 0) curve.startNewSubPath (px, py); else curve.lineTo (px, py);
    }
    g.setColour (colour::accent.withAlpha (0.35f));
    g.fillRect (graph.getX() + cut * graph.getWidth(), graph.getY(), 1.0f, graph.getHeight());
    g.setColour (colour::accent);
    g.strokePath (curve, juce::PathStrokeType (2.2f));

    // knob row: Cutoff, Reso + Type segmented
    auto cutoff = knobRow.removeFromLeft (54); knobRow.removeFromLeft (9);
    knobCell (g, cutoff, dim::detailKnob, cut, "Cutoff", false);
    auto reso = knobRow.removeFromLeft (54); knobRow.removeFromLeft (12);
    knobCell (g, reso, dim::detailKnob, res, "Reso", false);

    auto typeBox = knobRow.removeFromLeft (150).toFloat();
    drawPanel (g, typeBox, colour::panelAlt, colour::borderSubtle);
    auto ti = typeBox.reduced (8, 8);
    g.setColour (colour::faint); g.setFont (monoFont (8.0f));
    g.drawText ("Type", ti.removeFromTop (12).toNearestInt(), Justification::centredLeft);
    const char* types[] = { "LP", "BP", "HP" };
    for (int i = 0; i < 3; ++i)
    {
        auto seg = ti.removeFromLeft (42).reduced (1.5f); ti.removeFromLeft (2);
        const bool active = (i == 0);
        pseudoButton (g, seg, types[i],
                      active ? colour::accent : colour::panelAlt,
                      active ? colour::accentLight : colour::borderSubtle,
                      active ? Colour (0xff1a1410) : colour::dim, 9.5f);
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
