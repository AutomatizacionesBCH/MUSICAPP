#pragma once
#include "../FxBlock.h"
#include <cmath>

// Uni-Vibe: 4 filtros all-pass de primer orden en serie con frecuencias centro
// ESCALONADAS (base * [0.5, 1.0, 1.7, 2.6]) barridas juntas por un solo LFO.
// El espaciado desigual es lo que le da el caracter de uni-vibe (no un phaser
// normal). Se mezcla wet + dry.
//
// All-pass de primer orden:  y = -g*x + xprev + g*yprev
// con g = (tan(pi*fc/sr) - 1) / (tan(pi*fc/sr) + 1).
class FxUniVibe : public FxBlock
{
public:
    FxUniVibe()
    {
        add ("rate",  "Rate",  0.1f, 8.0f, 2.0f, "Hz");
        add ("depth", "Depth", 0.0f, 1.0f, 0.7f, "%");
        add ("mix",   "Mix",   0.0f, 1.0f, 0.5f, "%");
    }
    juce::String typeId() const override { return "univibe"; }
    juce::String displayName() const override { return "Uni-Vibe"; }

    void prepare (double sr, int) override
    {
        sampleRate = sr;
        reset();
    }
    void reset() override
    {
        phase = 0.0f;
        for (int s = 0; s < kStages; ++s) { xprev[s] = 0.0f; yprev[s] = 0.0f; }
    }

    void processMono (float* data, int n) override
    {
        const float rate  = pv (0);
        const float depth = pv (1);
        const float mix   = pv (2);
        const float inc   = juce::MathConstants<float>::twoPi * rate / (float) sampleRate;

        // Frecuencia base barrida por el LFO. Rango ~200..1500 Hz segun depth.
        const float fMin = 200.0f, fMax = 1500.0f;
        const float nyq  = 0.45f * (float) sampleRate;   // techo seguro

        for (int i = 0; i < n; ++i)
        {
            const float lfo  = 0.5f * (1.0f + std::sin (phase));   // 0..1
            const float base = fMin + depth * (fMax - fMin) * lfo;

            const float dry = data[i];
            float x = dry;
            for (int s = 0; s < kStages; ++s)
            {
                float fc = base * stageMul[s];
                fc = juce::jlimit (20.0f, nyq, fc);

                const float t = std::tan (juce::MathConstants<float>::pi * fc / (float) sampleRate);
                const float g = (t - 1.0f) / (t + 1.0f);

                const float y = -g * x + xprev[s] + g * yprev[s];
                xprev[s] = x;
                yprev[s] = y;
                x = y;
            }

            float out = dry * (1.0f - mix) + x * mix;
            if (! std::isfinite (out)) out = 0.0f;
            data[i] = juce::jlimit (-1.5f, 1.5f, out);

            phase += inc;
            if (phase >= juce::MathConstants<float>::twoPi) phase -= juce::MathConstants<float>::twoPi;
        }
    }

private:
    static constexpr int kStages = 4;
    const float stageMul[kStages] = { 0.5f, 1.0f, 1.7f, 2.6f };   // espaciado desigual

    double sampleRate { 48000.0 };
    float  phase { 0.0f };
    float  xprev[kStages] { 0.0f, 0.0f, 0.0f, 0.0f };
    float  yprev[kStages] { 0.0f, 0.0f, 0.0f, 0.0f };
};
