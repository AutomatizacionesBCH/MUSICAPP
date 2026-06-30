#pragma once
#include "../FxBlock.h"
#include <cmath>
#include <vector>
#include <algorithm>

// Gated reverb (80s): juce::Reverb con la cola wet pasada por un gate duro.
// Se sigue el pico de la senal dry; al cruzar el threshold el gate ABRE (el wet
// pasa) y reinicia un hold timer. Mientras el hold esta activo el wet pasa a
// full; al expirar el hold con la entrada por debajo del threshold el gate
// CIERRA (wet -> 0) con un fade corto para evitar clicks.
class FxGatedReverb : public FxBlock
{
public:
    FxGatedReverb()
    {
        add ("mix",       "Mix",       0.0f,  1.0f,   0.4f,   "%");
        add ("decay",     "Decay",     0.0f,  1.0f,   0.6f,   "%");
        add ("hold",      "Hold",      50.0f, 500.0f, 200.0f, "ms");
        add ("threshold", "Threshold", 0.0f,  0.5f,   0.08f,  "%");
    }
    juce::String typeId() const override { return "gated"; }
    juce::String displayName() const override { return "Gated Reverb"; }

    void prepare (double sr, int blockSize) override
    {
        sampleRate = sr;
        rev.setSampleRate (sr);
        rev.reset();
        wet.assign ((size_t) juce::jmax (1, blockSize), 0.0f);
        gateGain  = 0.0f;
        holdSamps = 0;
    }
    void reset() override
    {
        rev.reset();
        std::fill (wet.begin(), wet.end(), 0.0f);
        gateGain  = 0.0f;
        holdSamps = 0;
    }

    void processMono (float* data, int n) override
    {
        if ((int) wet.size() < n)
            return;   // salvaguarda: nunca deberia pasar (prepare dimensiona al blockSize)

        const float mix    = pv (0);
        const float thresh = pv (3);

        // Reverb 100% wet en buffer temporal.
        juce::Reverb::Parameters p;
        p.roomSize   = pv (1);
        p.damping    = 0.5f;
        p.width      = 1.0f;
        p.freezeMode = 0.0f;
        p.wetLevel   = 1.0f;
        p.dryLevel   = 0.0f;
        rev.setParameters (p);

        std::copy (data, data + n, wet.begin());
        rev.processMono (wet.data(), n);

        // hold en samples y paso de fade (~3 ms) para cerrar/abrir sin clicks.
        const int   holdTotal = (int) (pv (2) * 0.001f * (float) sampleRate);
        const float fadeStep  = 1.0f / juce::jmax (1.0f, 0.003f * (float) sampleRate);

        for (int i = 0; i < n; ++i)
        {
            const float dry = data[i];

            // detector de pico sobre la entrada dry -> abre el gate y reinicia hold.
            if (std::abs (dry) > thresh)
                holdSamps = holdTotal;

            // gate target: 1 mientras quede hold, 0 cuando expira.
            const float target = (holdSamps > 0) ? 1.0f : 0.0f;
            if (holdSamps > 0) --holdSamps;

            // fade suave hacia el target.
            if      (gateGain < target) gateGain = juce::jmin (target, gateGain + fadeStep);
            else if (gateGain > target) gateGain = juce::jmax (target, gateGain - fadeStep);

            float w = wet[(size_t) i];
            if (! std::isfinite (w)) w = 0.0f;

            const float out = dry * (1.0f - mix) + (w * gateGain) * mix;
            data[i] = juce::jlimit (-1.5f, 1.5f, out);
        }
    }

private:
    double sampleRate { 48000.0 };
    juce::Reverb rev;
    std::vector<float> wet;
    float gateGain  { 0.0f };
    int   holdSamps { 0 };
};
