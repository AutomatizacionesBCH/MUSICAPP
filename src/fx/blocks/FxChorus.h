#pragma once
#include "../FxBlock.h"

// Chorus (mod). juce::dsp::Chorus voiced 80s (delay ~7 ms tipo BBD, sin feedback).
class FxChorus : public FxBlock
{
public:
    FxChorus()
    {
        add ("rate",  "Rate",  0.1f, 5.0f, 0.8f,  "Hz");
        add ("depth", "Depth", 0.0f, 1.0f, 0.35f, "%");
        add ("mix",   "Mix",   0.0f, 1.0f, 0.4f,  "%");
    }
    juce::String typeId() const override { return "chorus"; }
    juce::String displayName() const override { return "Chorus"; }

    void prepare (double sr, int bs) override
    {
        juce::dsp::ProcessSpec spec { sr, (juce::uint32) bs, 1 };
        ch.prepare (spec); ch.reset();
    }
    void reset() override { ch.reset(); }

    void processMono (float* data, int n) override
    {
        ch.setRate (pv (0)); ch.setDepth (pv (1)); ch.setCentreDelay (7.0f);
        ch.setFeedback (0.0f); ch.setMix (pv (2));
        float* chans[1] = { data };
        juce::dsp::AudioBlock<float> b (chans, 1, (size_t) n);
        juce::dsp::ProcessContextReplacing<float> ctx (b);
        ch.process (ctx);
    }
private:
    juce::dsp::Chorus<float> ch;
};
