#pragma once
#include "../FxBlock.h"

// Reverb algorítmica (Freeverb vía juce::Reverb). Room/Hall genérico; los
// sabores (plate Dattorro, spring, shimmer) vendrán como bloques propios.
class FxReverb : public FxBlock
{
public:
    FxReverb()
    {
        add ("mix",     "Mix",     0.0f, 1.0f, 0.3f, "%");
        add ("size",    "Size",    0.0f, 1.0f, 0.6f, "%");
        add ("damping", "Damping", 0.0f, 1.0f, 0.5f, "%");
    }
    juce::String typeId() const override { return "reverb"; }
    juce::String displayName() const override { return "Reverb"; }

    void prepare (double sr, int) override { rev.setSampleRate (sr); rev.reset(); }
    void reset() override { rev.reset(); }

    void processMono (float* data, int n) override
    {
        const float mix = pv (0);
        juce::Reverb::Parameters p;
        p.roomSize   = pv (1);
        p.damping    = pv (2);
        p.width      = 1.0f;
        p.freezeMode = 0.0f;
        p.wetLevel   = mix;
        p.dryLevel   = 1.0f - mix;
        rev.setParameters (p);
        rev.processMono (data, n);
    }
private:
    juce::Reverb rev;
};
