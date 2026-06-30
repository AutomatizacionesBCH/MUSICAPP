#pragma once
#include "../FxBlock.h"
#include <cmath>

// Filter Sweep: auto-wah por LFO. Band-pass resonante (StateVariableTPTFilter)
// cuyo cutoff lo barre un LFO seno alrededor de un centro, escalado por depth.
// cutoff = center * 2^(depth * sin(lfo)), clamp [80, sr*0.45]. Mezcla wet/dry.
class FxSweep : public FxBlock
{
public:
    FxSweep()
    {
        add ("rate",      "Rate",      0.1f, 8.0f,  1.0f, "Hz");
        add ("depth",     "Depth",     0.0f, 1.0f,  0.7f, "%");
        add ("resonance", "Resonance", 0.1f, 0.95f, 0.6f, "%");
        add ("center",    "Center",    200.0f, 2000.0f, 800.0f, "Hz");
        add ("mix",       "Mix",       0.0f, 1.0f,  1.0f, "%");
    }
    juce::String typeId() const override { return "sweep"; }
    juce::String displayName() const override { return "Filter Sweep"; }

    void prepare (double sr, int bs) override
    {
        sampleRate = sr;
        phase = 0.0f;
        juce::dsp::ProcessSpec spec { sr, (juce::uint32) bs, 1 };
        filter.prepare (spec);
        filter.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
        filter.reset();
    }
    void reset() override { phase = 0.0f; filter.reset(); }

    void processMono (float* data, int n) override
    {
        const float rate  = pv (0);
        const float depth = pv (1);
        const float reso  = juce::jlimit (0.1f, 0.95f, pv (2));
        const float center = pv (3);
        const float mix   = juce::jlimit (0.0f, 1.0f, pv (4));

        const float inc    = juce::MathConstants<float>::twoPi * rate / (float) sampleRate;
        const float maxCut = (float) (sampleRate * 0.45);
        // Q de resonancia: 0.5..~9 segun el parametro.
        const float q = 0.5f + reso * 9.0f;
        filter.setResonance (q);

        for (int i = 0; i < n; ++i)
        {
            const float dry = data[i];

            float cutoff = center * std::pow (2.0f, depth * std::sin (phase));
            cutoff = juce::jlimit (80.0f, maxCut, cutoff);
            filter.setCutoffFrequency (cutoff);

            float wet = filter.processSample (0, dry);
            if (! std::isfinite (wet)) wet = 0.0f;

            float out = dry * (1.0f - mix) + wet * mix;
            data[i] = juce::jlimit (-1.5f, 1.5f, out);

            phase += inc;
            if (phase >= juce::MathConstants<float>::twoPi)
                phase -= juce::MathConstants<float>::twoPi;
        }
    }
private:
    double sampleRate { 48000.0 };
    float  phase { 0.0f };
    juce::dsp::StateVariableTPTFilter<float> filter;
};
