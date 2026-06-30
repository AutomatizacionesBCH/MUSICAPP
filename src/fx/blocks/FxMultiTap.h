#pragma once
#include "../FxBlock.h"
#include <cmath>
#include <vector>
#include <algorithm>

// Multi-tap delay: una sola linea de retardo leida en 3 taps (time, time*0.66,
// time*0.33) con niveles decrecientes (1.0, 0.7, 0.45) sumados al wet. Feedback
// unico desde el tap mas largo. Buffer max 1.6 s.
class FxMultiTap : public FxBlock
{
public:
    FxMultiTap()
    {
        add ("time",     "Time",     50.0f, 1500.0f, 400.0f, "ms");
        add ("feedback", "Feedback", 0.0f,  0.85f,   0.3f,   "%");
        add ("mix",      "Mix",      0.0f,  1.0f,    0.35f,  "%");
    }
    juce::String typeId() const override { return "multitap"; }
    juce::String displayName() const override { return "Multi-Tap"; }

    void prepare (double sr, int) override
    {
        sampleRate = sr;
        const int maxSamples = (int) (sr * 1.6) + 8;   // 1.6 s max + holgura
        buf.assign ((size_t) juce::jmax (8, maxSamples), 0.0f);
        writePos = 0;
    }
    void reset() override { std::fill (buf.begin(), buf.end(), 0.0f); writePos = 0; }

    void processMono (float* data, int n) override
    {
        const int   size = (int) buf.size();
        const float fb   = juce::jlimit (0.0f, 0.85f, pv (1));
        const float mix  = pv (2);

        // tap mas largo = time; los otros a 0.66 y 0.33 del tiempo
        const int d0 = juce::jlimit (1, size - 1, (int) (pv (0) * 0.001f * (float) sampleRate));
        const int d1 = juce::jlimit (1, size - 1, (int) (d0 * 0.66f));
        const int d2 = juce::jlimit (1, size - 1, (int) (d0 * 0.33f));

        for (int i = 0; i < n; ++i)
        {
            const float t0 = buf[(size_t) tapAt (d0, size)];   // mas largo (feedback)
            const float t1 = buf[(size_t) tapAt (d1, size)];
            const float t2 = buf[(size_t) tapAt (d2, size)];

            const float wet = t0 * 1.0f + t1 * 0.7f + t2 * 0.45f;

            // feedback solo desde el tap mas largo
            buf[(size_t) writePos] = data[i] + t0 * fb;

            float out = data[i] * (1.0f - mix) + wet * mix;
            if (! std::isfinite (out)) out = 0.0f;
            data[i] = juce::jlimit (-1.5f, 1.5f, out);

            if (++writePos >= size) writePos = 0;
        }
    }
private:
    int tapAt (int delay, int size) const noexcept
    {
        int rp = writePos - delay;
        if (rp < 0) rp += size;
        return rp;
    }

    double sampleRate { 48000.0 };
    std::vector<float> buf;
    int writePos { 0 };
};
