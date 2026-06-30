#pragma once
#include "../FxBlock.h"
#include <cmath>
#include <vector>

// Flanger: delay corto (1..6 ms) modulado por LFO + feedback. Comb móvil.
// Lectura fraccional (interpolación lineal) para barrer sin clicks.
class FxFlanger : public FxBlock
{
public:
    FxFlanger()
    {
        add ("rate",     "Rate",     0.05f, 5.0f,  0.4f, "Hz");
        add ("depth",    "Depth",    0.0f,  1.0f,  0.6f, "%");
        add ("feedback", "Feedback", 0.0f,  0.95f, 0.5f, "%");
        add ("mix",      "Mix",      0.0f,  1.0f,  0.5f, "%");
    }
    juce::String typeId() const override { return "flanger"; }
    juce::String displayName() const override { return "Flanger"; }

    void prepare (double sr, int) override
    {
        sampleRate = sr;
        const int maxS = (int) (sr * 0.02) + 8;   // hasta ~20 ms + holgura
        buf.assign ((size_t) juce::jmax (8, maxS), 0.0f);
        writePos = 0; phase = 0.0f;
    }
    void reset() override { std::fill (buf.begin(), buf.end(), 0.0f); writePos = 0; phase = 0.0f; }

    void processMono (float* data, int n) override
    {
        const int   size  = (int) buf.size();
        const float rate  = pv (0), depth = pv (1), fb = pv (2), mix = pv (3);
        const float inc   = juce::MathConstants<float>::twoPi * rate / (float) sampleRate;
        const float baseMs = 1.0f, sweepMs = 5.0f;     // 1..6 ms
        for (int i = 0; i < n; ++i)
        {
            const float lfo    = 0.5f * (1.0f + std::sin (phase));
            const float delMs  = baseMs + depth * sweepMs * lfo;
            const float delS   = delMs * 0.001f * (float) sampleRate;
            float rp = (float) writePos - delS;
            while (rp < 0.0f) rp += (float) size;
            const int   i0   = (int) rp;
            const int   i1   = (i0 + 1) % size;
            const float frac = rp - (float) i0;
            const float delayed = buf[(size_t) i0] * (1.0f - frac) + buf[(size_t) i1] * frac;

            buf[(size_t) writePos] = data[i] + delayed * fb;
            data[i] = data[i] * (1.0f - 0.5f * mix) + delayed * mix;
            if (++writePos >= size) writePos = 0;
            phase += inc;
            if (phase >= juce::MathConstants<float>::twoPi) phase -= juce::MathConstants<float>::twoPi;
        }
    }
private:
    double sampleRate { 48000.0 };
    std::vector<float> buf;
    int   writePos { 0 };
    float phase { 0.0f };
};
