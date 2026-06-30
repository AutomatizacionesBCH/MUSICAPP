#pragma once
#include "../FxBlock.h"
#include <cmath>
#include <vector>
#include <algorithm>

// Vibrato real (pitch vibrato, 100% wet): linea de retardo cuya posicion de
// lectura barre un LFO (efecto Doppler). Como un flanger pero solo wet y sin
// feedback. Retardo base ~5 ms, el LFO barre la lectura +-depth*4 ms.
// Lectura fraccional (interpolacion lineal) para barrer sin clicks.
class FxVibrato : public FxBlock
{
public:
    FxVibrato()
    {
        add ("rate",  "Rate",  0.1f, 8.0f, 4.0f, "Hz");
        add ("depth", "Depth", 0.0f, 1.0f, 0.5f, "%");
    }
    juce::String typeId() const override { return "vibrato"; }
    juce::String displayName() const override { return "Vibrato"; }

    void prepare (double sr, int) override
    {
        sampleRate = sr;
        // base 5 ms + barrido +-4 ms => maximo ~9 ms; reservamos holgura.
        const int maxS = (int) (sr * 0.02) + 8;
        buf.assign ((size_t) juce::jmax (8, maxS), 0.0f);
        writePos = 0; phase = 0.0f;
    }
    void reset() override { std::fill (buf.begin(), buf.end(), 0.0f); writePos = 0; phase = 0.0f; }

    void processMono (float* data, int n) override
    {
        const int   size  = (int) buf.size();
        const float rate  = pv (0), depth = pv (1);
        const float inc   = juce::MathConstants<float>::twoPi * rate / (float) sampleRate;
        const float baseMs = 5.0f, sweepMs = 4.0f;   // base 5 ms, +-depth*4 ms
        for (int i = 0; i < n; ++i)
        {
            // LFO bipolar para barrer la lectura por encima y por debajo del base.
            const float lfo   = std::sin (phase);
            const float delMs = baseMs + depth * sweepMs * lfo;
            float delS = delMs * 0.001f * (float) sampleRate;
            delS = juce::jlimit (1.0f, (float) (size - 2), delS);

            float rp = (float) writePos - delS;
            while (rp < 0.0f) rp += (float) size;
            const int   i0   = (int) rp;
            const int   i1   = (i0 + 1) % size;
            const float frac = rp - (float) i0;
            const float delayed = buf[(size_t) i0] * (1.0f - frac) + buf[(size_t) i1] * frac;

            buf[(size_t) writePos] = data[i];   // sin feedback
            data[i] = delayed;                  // 100% wet, sin dry mix

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
