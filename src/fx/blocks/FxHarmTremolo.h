#pragma once
#include "../FxBlock.h"
#include <cmath>

// Tremolo armonico: separa la senal en banda grave y aguda con un crossover
// (LinkwitzRiley LP/HP), modula cada banda con un LFO a 180 grados de fase y
// las recombina. El "vibe" Fender brownface clasico.
class FxHarmTremolo : public FxBlock
{
public:
    FxHarmTremolo()
    {
        add ("rate",  "Rate",      0.5f,   12.0f, 5.0f,   "Hz");
        add ("depth", "Depth",     0.0f,   1.0f,  0.6f,   "%");
        add ("xover", "Crossover", 200.0f, 2000.0f, 800.0f, "Hz");
    }
    juce::String typeId() const override { return "harmtrem"; }
    juce::String displayName() const override { return "Harm Tremolo"; }

    void prepare (double sr, int bs) override
    {
        sampleRate = sr;
        phase = 0.0f;
        juce::dsp::ProcessSpec spec { sr, (juce::uint32) bs, 1 };

        lp.prepare (spec);
        lp.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
        lp.setCutoffFrequency (lastCutoff);
        lp.reset();

        hp.prepare (spec);
        hp.setType (juce::dsp::LinkwitzRileyFilterType::highpass);
        hp.setCutoffFrequency (lastCutoff);
        hp.reset();
    }

    void reset() override
    {
        phase = 0.0f;
        lp.reset();
        hp.reset();
    }

    void processMono (float* data, int n) override
    {
        const float rate  = pv (0);
        const float depth = juce::jlimit (0.0f, 1.0f, pv (1));
        const float cutoff = juce::jlimit (20.0f, (float) (sampleRate * 0.49), pv (2));

        if (cutoff != lastCutoff)
        {
            lp.setCutoffFrequency (cutoff);
            hp.setCutoffFrequency (cutoff);
            lastCutoff = cutoff;
        }

        const float inc = juce::MathConstants<float>::twoPi * rate / (float) sampleRate;

        for (int i = 0; i < n; ++i)
        {
            const float x   = data[i];
            const float low  = lp.processSample (0, x);
            const float high = hp.processSample (0, x);

            // LFOs a 180 grados, en rango 0..1.
            const float lfoLow  = 0.5f * (1.0f + std::sin (phase));
            const float lfoHigh = 0.5f * (1.0f + std::sin (phase + juce::MathConstants<float>::pi));

            const float gLow  = 1.0f - depth * lfoLow;
            const float gHigh = 1.0f - depth * lfoHigh;

            float y = low * gLow + high * gHigh;

            if (! std::isfinite (y)) y = 0.0f;
            data[i] = juce::jlimit (-1.5f, 1.5f, y);

            phase += inc;
            if (phase >= juce::MathConstants<float>::twoPi)
                phase -= juce::MathConstants<float>::twoPi;
        }
    }

private:
    double sampleRate { 48000.0 };
    float  phase { 0.0f };
    float  lastCutoff { 800.0f };
    juce::dsp::LinkwitzRileyFilter<float> lp, hp;
};
