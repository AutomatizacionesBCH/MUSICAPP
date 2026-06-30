#pragma once
#include "../FxBlock.h"
#include <cmath>

// Tremolo óptico: LFO (seno) modulando la ganancia. El efecto de modulación más
// simple — la base para bias/harmonic tremolo después.
class FxTremolo : public FxBlock
{
public:
    FxTremolo()
    {
        add ("rate",  "Rate",  0.5f, 12.0f, 5.0f, "Hz");
        add ("depth", "Depth", 0.0f, 1.0f,  0.6f, "%");
    }
    juce::String typeId() const override { return "tremolo"; }
    juce::String displayName() const override { return "Tremolo"; }

    void prepare (double sr, int) override { sampleRate = sr; phase = 0.0f; }
    void reset() override { phase = 0.0f; }

    void processMono (float* data, int n) override
    {
        const float rate  = pv (0);
        const float depth = pv (1);
        const float inc   = juce::MathConstants<float>::twoPi * rate / (float) sampleRate;
        for (int i = 0; i < n; ++i)
        {
            const float lfo  = 0.5f * (1.0f + std::sin (phase));   // 0..1
            data[i] *= 1.0f - depth * lfo;
            phase += inc;
            if (phase >= juce::MathConstants<float>::twoPi) phase -= juce::MathConstants<float>::twoPi;
        }
    }
private:
    double sampleRate { 48000.0 };
    float  phase { 0.0f };
};
