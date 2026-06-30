#pragma once
#include "../FxBlock.h"

// Phaser. juce::dsp::Phaser (cadena de all-pass modulada por LFO).
class FxPhaser : public FxBlock
{
public:
    FxPhaser()
    {
        add ("rate",     "Rate",     0.05f, 5.0f,  0.5f, "Hz");
        add ("depth",    "Depth",    0.0f,  1.0f,  0.6f, "%");
        add ("feedback", "Feedback", 0.0f,  0.95f, 0.3f, "%");
        add ("mix",      "Mix",      0.0f,  1.0f,  0.5f, "%");
    }
    juce::String typeId() const override { return "phaser"; }
    juce::String displayName() const override { return "Phaser"; }

    void prepare (double sr, int bs) override
    {
        juce::dsp::ProcessSpec spec { sr, (juce::uint32) bs, 1 };
        ph.prepare (spec); ph.reset();
    }
    void reset() override { ph.reset(); }

    void processMono (float* data, int n) override
    {
        ph.setRate (pv (0)); ph.setDepth (pv (1)); ph.setFeedback (pv (2)); ph.setMix (pv (3));
        ph.setCentreFrequency (600.0f);
        float* chans[1] = { data };
        juce::dsp::AudioBlock<float> b (chans, 1, (size_t) n);
        juce::dsp::ProcessContextReplacing<float> ctx (b);
        ph.process (ctx);
    }
private:
    juce::dsp::Phaser<float> ph;
};
