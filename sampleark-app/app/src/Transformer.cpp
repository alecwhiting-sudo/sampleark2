#include "Transformer.h"
#include <juce_core/juce_core.h>
#include <cmath>

namespace sa
{
const char* shapeName (int preset)
{
    static const char* names[kNumShapes] = { "Ramp Up", "Ramp Dn", "Triangle", "Sine", "Square", "Exp Dn", "Random" };
    return (preset >= 0 && preset < kNumShapes) ? names[preset] : "Drawn";
}

void Transformer::applyShape (int preset)
{
    shapeType = preset;
    curve.assign (kCurvePoints, 0.5f);
    for (int i = 0; i < kCurvePoints; ++i)
    {
        const float x = (float) i / (float) (kCurvePoints - 1);
        float v = 0.5f;
        switch (preset)
        {
            case 0: v = x; break;                                   // ramp up
            case 1: v = 1.0f - x; break;                            // ramp down
            case 2: v = (x < 0.5f) ? x * 2.0f : (1.0f - x) * 2.0f; break;   // triangle
            case 3: v = 0.5f - 0.5f * std::cos (x * juce::MathConstants<float>::twoPi); break; // sine
            case 4: v = (x < 0.5f) ? 1.0f : 0.0f; break;            // square
            case 5: v = std::exp (-x * 4.0f); break;                // exp down
            case 6:                                                 // random sample & hold (8 steps, deterministic)
            {
                const int step = (int) (x * 8.0f);
                const float h = std::sin ((float) step * 12.9898f) * 43758.5453f;
                v = h - std::floor (h);
                break;
            }
            default: v = 0.5f; break;
        }
        curve[(size_t) i] = juce::jlimit (0.0f, 1.0f, v);
    }
}

float shapeEval (const Transformer& t, float phase01)
{
    if (t.curve.empty()) return 0.5f;
    const float p = juce::jlimit (0.0f, 1.0f, phase01) * (float) (kCurvePoints - 1);
    const int i0 = (int) p;
    const int i1 = juce::jmin (i0 + 1, kCurvePoints - 1);
    const float f = p - (float) i0;
    return t.curve[(size_t) i0] * (1.0f - f) + t.curve[(size_t) i1] * f;
}

double transRateSeconds (int rateDiv, double tempoBpm)
{
    const double beat = 60.0 / juce::jmax (1.0, tempoBpm);
    const double beats = (rateDiv == 1) ? 4.0 : (rateDiv == 2) ? 2.0 : (rateDiv == 3) ? 1.0 : (rateDiv == 4) ? 0.5 : 0.25;
    return beats * beat;   // 1=1/1, 2=1/2, 3=1/4, 4=1/8, 5=1/16
}
}
