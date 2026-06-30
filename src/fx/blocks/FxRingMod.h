#pragma once
#include "../FxBlock.h"
#include <cmath>

// Ring modulator: multiplica la senal por una portadora sinusoidal. Genera
// tonos metalicos/inarmonicos. La via wet pasa por un LP one-pole para domar
// el aliasing de los productos de modulacion mas agudos.
class FxRingMod : public FxBlock
{
public:
    FxRingMod()
    {
        add ("freq", "Freq", 40.0f, 2000.0f, 440.0f, "Hz");
        add ("mix",  "Mix",  0.0f,  1.0f,    0.5f,   "%");
    }
    juce::String typeId() const override { return "ringmod"; }
    juce::String displayName() const override { return "Ring Mod"; }

    void prepare (double sr, int) override
    {
        sampleRate = sr;
        phase = 0.0f;
        lpState = 0.0f;
        const float fc = juce::jmin ((float) (sr * 0.45), 6000.0f);
        lpCoef = std::exp (-2.0f * juce::MathConstants<float>::pi * fc / (float) sr);
    }
    void reset() override { phase = 0.0f; lpState = 0.0f; }

    void processMono (float* data, int n) override
    {
        const float freq = pv (0);
        const float mix  = juce::jlimit (0.0f, 1.0f, pv (1));
        const float inc  = juce::MathConstants<float>::twoPi * freq / (float) sampleRate;
        for (int i = 0; i < n; ++i)
        {
            const float in  = data[i];
            float wet = in * std::sin (phase);

            // LP one-pole sobre la via wet (anti-aliasing suave)
            lpState = wet * (1.0f - lpCoef) + lpState * lpCoef;
            wet = lpState;

            float out = (1.0f - mix) * in + mix * wet;
            if (! std::isfinite (out)) out = 0.0f;
            data[i] = juce::jlimit (-1.0f, 1.0f, out);

            phase += inc;
            if (phase >= juce::MathConstants<float>::twoPi)
                phase -= juce::MathConstants<float>::twoPi;
        }
    }
private:
    double sampleRate { 48000.0 };
    float  phase { 0.0f };
    float  lpState { 0.0f };
    float  lpCoef { 0.0f };
};
