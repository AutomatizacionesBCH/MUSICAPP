#pragma once
#include "../FxBlock.h"
#include <cmath>
#include <vector>
#include <algorithm>

// Plate reverb: cadena de all-pass diffusers cortos (5/7/10/13 ms, g ~0.7) que
// densifican la entrada, seguidos de una juce::Reverb afinada brillante (width
// alto, damping bajo). Un predelay precede a todo. La densidad rapida + el brillo
// dan el caracter "plate". out mezcla wet/dry.
class FxPlate : public FxBlock
{
public:
    FxPlate()
    {
        add ("mix",      "Mix",      0.0f, 1.0f,  0.3f, "%");
        add ("decay",    "Decay",    0.0f, 1.0f,  0.6f, "%");   // -> roomSize
        add ("tone",     "Tone",     0.0f, 1.0f,  0.5f, "%");   // -> damping
        add ("predelay", "Predelay", 0.0f, 80.0f, 10.0f, "ms");
    }
    juce::String typeId() const override { return "plate"; }
    juce::String displayName() const override { return "Plate"; }

    void prepare (double sr, int) override
    {
        sampleRate = sr;

        // Predelay: hasta 80 ms + holgura.
        const int pdMax = (int) (sr * 0.085) + 8;
        pdBuf.assign ((size_t) juce::jmax (8, pdMax), 0.0f);
        pdWrite = 0;

        // All-pass diffusers: longitudes fijas en ms.
        const float apMs[kNumAP] = { 5.0f, 7.0f, 10.0f, 13.0f };
        for (int a = 0; a < kNumAP; ++a)
        {
            int len = juce::jmax (4, (int) (apMs[a] * 0.001f * (float) sr) + 1);
            ap[a].buf.assign ((size_t) len, 0.0f);
            ap[a].write = 0;
        }

        rev.setSampleRate (sr);
        rev.reset();
    }

    void reset() override
    {
        std::fill (pdBuf.begin(), pdBuf.end(), 0.0f);
        pdWrite = 0;
        for (int a = 0; a < kNumAP; ++a)
        {
            std::fill (ap[a].buf.begin(), ap[a].buf.end(), 0.0f);
            ap[a].write = 0;
        }
        rev.reset();
    }

    void processMono (float* data, int n) override
    {
        const float mix      = pv (0);
        const float decay    = pv (1);
        const float tone     = pv (2);
        const int   pdSize   = (int) pdBuf.size();
        const int   pdSamp   = juce::jlimit (0, pdSize - 1,
                                             (int) (pv (3) * 0.001f * (float) sampleRate));

        // Reverb brillante estilo plate: width alto, damping bajo. tone sube el
        // damping (mas oscuro) cuando crece.
        juce::Reverb::Parameters p;
        p.roomSize   = 0.45f + 0.5f * decay;     // denso pero acotado
        p.damping    = 0.15f + 0.55f * tone;     // base baja (brillante)
        p.width      = 1.0f;
        p.freezeMode = 0.0f;
        p.wetLevel   = 1.0f;                      // mezcla wet/dry la hacemos aqui
        p.dryLevel   = 0.0f;
        rev.setParameters (p);

        const float g = 0.7f;                     // coef all-pass

        for (int i = 0; i < n; ++i)
        {
            const float dry = data[i];

            // --- Predelay ---
            int rp = pdWrite - pdSamp;
            if (rp < 0) rp += pdSize;
            float x = pdBuf[(size_t) rp];
            pdBuf[(size_t) pdWrite] = dry;
            if (++pdWrite >= pdSize) pdWrite = 0;

            // --- Cadena de all-pass diffusers ---
            // y = -g*x + buf_delayed ; buf_in = x + g*y
            for (int a = 0; a < kNumAP; ++a)
            {
                AP& s = ap[a];
                const int sz = (int) s.buf.size();
                const float delayed = s.buf[(size_t) s.write];
                const float y = -g * x + delayed;
                s.buf[(size_t) s.write] = x + g * y;
                if (++s.write >= sz) s.write = 0;
                x = y;
            }

            // Guard NaN/Inf antes de la reverb.
            if (! std::isfinite (x)) x = 0.0f;
            wet[0] = x;
            rev.processMono (wet, 1);

            float w = wet[0];
            if (! std::isfinite (w)) w = 0.0f;

            data[i] = dry * (1.0f - mix) + w * mix;
        }
    }

private:
    static constexpr int kNumAP = 4;

    struct AP
    {
        std::vector<float> buf;
        int write { 0 };
    };

    double sampleRate { 48000.0 };

    std::vector<float> pdBuf;
    int pdWrite { 0 };

    AP ap[kNumAP];

    juce::Reverb rev;
    float wet[1] { 0.0f };
};
