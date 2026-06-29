#pragma once

#include <vector>
#include <array>

// M3a transformers: up to 8 modulation curves that move a target parameter (or an
// amplitude stage) over the course of the rendered output. Each has a shape (preset or
// user-drawn), a depth, and a time-basis (one-shot across the output, or cyclic at a rate).
namespace sa
{
constexpr int kNumTransformers = 8;
constexpr int kCurvePoints = 64;
constexpr int kNumShapes = 7;

enum class TransTarget { EffectParam, PreAmp, PostAmp };

struct Transformer
{
    bool on = false;
    TransTarget kind = TransTarget::EffectParam;
    int slot = 5;          // effect slot (Filter by default) when kind == EffectParam
    int param = 0;         // param index within that slot
    float depth = 0.6f;    // 0..1 amount
    int shapeType = 0;     // preset index, or -1 for user-drawn
    int basis = 0;         // 0 one-shot (over output), 1 cyclic (at rate)
    int rateDiv = 3;       // cyclic rate: 0 = Free (use freqHz), 1=1/1,2=1/2,3=1/4,4=1/8,5=1/16
    float freqHz = 2.0f;   // free-frequency (Hz) when rateDiv == 0
    float phase = 0.0f;    // 0..1 cycle phase offset
    std::vector<float> curve;  // kCurvePoints values in 0..1

    Transformer() { curve.assign (kCurvePoints, 0.5f); applyShape (0); }
    void applyShape (int preset);   // fills curve from a preset and sets shapeType
};

const char* shapeName (int preset);
float shapeEval (const Transformer& t, float phase01);     // interpolate the curve at phase 0..1
double transRateSeconds (int rateDiv, double tempoBpm);    // cyclic cycle length in seconds
}
