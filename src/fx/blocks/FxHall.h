#pragma once
#include "../FxBlock.h"
#include <cmath>
#include <vector>
#include <algorithm>

// Hall reverb: juce::Reverb (Freeverb) con room size grande + un pre-delay
// (linea de retardo corta al frente) para dar la sensacion de sala grande.
// El pre-delay separa el sonido directo de la cola reverberante.
class FxHall : public FxBlock
{
public:
    FxHall()
    {
        add ("mix",      "Mix",      0.0f,   1.0f,   0.3f,  "%");
        add ("decay",    "Decay",    0.0f,   1.0f,   0.75f, "%");
        add ("damping",  "Damping",  0.0f,   1.0f,   0.4f,  "%");
        add ("predelay", "Predelay", 0.0f, 120.0f,  25.0f,  "ms");
    }
    juce::String typeId() const override { return "hall"; }
    juce::String displayName() const override { return "Hall"; }

    void prepare (double sr, int) override
    {
        sampleRate = sr;
        // buffer de pre-delay: max 120 ms + holgura
        const int maxSamples = (int) (sr * 0.121) + 8;
        pre.assign ((size_t) juce::jmax (8, maxSamples), 0.0f);
        writePos = 0;
        rev.setSampleRate (sr);
        rev.reset();
    }
    void reset() override
    {
        std::fill (pre.begin(), pre.end(), 0.0f);
        writePos = 0;
        rev.reset();
    }

    void processMono (float* data, int n) override
    {
        const int   size    = (int) pre.size();
        const float mix     = pv (0);
        const int   preSamp = juce::jlimit (0, size - 1,
                                            (int) (pv (3) * 0.001f * (float) sampleRate));

        // Hall: room size grande derivada del decay (sesgada hacia arriba).
        juce::Reverb::Parameters p;
        p.roomSize   = juce::jlimit (0.0f, 1.0f, 0.5f + 0.5f * pv (1));
        p.damping    = pv (2);
        p.width      = 1.0f;
        p.freezeMode = 0.0f;
        p.wetLevel   = mix;
        p.dryLevel   = 1.0f - mix;
        rev.setParameters (p);

        for (int i = 0; i < n; ++i)
        {
            const float dry = data[i];

            // pre-delay: leer muestra retardada, escribir la actual
            int readPos = writePos - preSamp;
            if (readPos < 0) readPos += size;
            const float delayed = pre[(size_t) readPos];
            pre[(size_t) writePos] = dry;
            if (++writePos >= size) writePos = 0;

            // la senal pre-retardada alimenta la reverb
            data[i] = delayed;
        }

        rev.processMono (data, n);

        // guardia anti-NaN
        for (int i = 0; i < n; ++i)
            if (! std::isfinite (data[i])) data[i] = 0.0f;
    }
private:
    double sampleRate { 48000.0 };
    juce::Reverb rev;
    std::vector<float> pre;
    int writePos { 0 };
};
