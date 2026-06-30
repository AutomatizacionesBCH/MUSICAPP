#pragma once
#include "../FxBlock.h"
#include <cmath>
#include <vector>
#include <algorithm>

// Delay digital: línea de retardo con feedback + low-pass one-pole en el lazo
// (tono de las repeticiones). Base de toda la familia delay (tape/BBD añaden
// saturación/modulación encima de esto).
class FxDelay : public FxBlock
{
public:
    FxDelay()
    {
        add ("time",     "Time",     20.0f,  2000.0f,  350.0f,  "ms");
        add ("feedback", "Feedback", 0.0f,   0.95f,    0.35f,   "%");
        add ("mix",      "Mix",      0.0f,   1.0f,     0.3f,    "%");
        add ("tone",     "Tone",     800.0f, 12000.0f, 6000.0f, "Hz");
    }
    juce::String typeId() const override { return "delay"; }
    juce::String displayName() const override { return "Delay"; }

    void prepare (double sr, int) override
    {
        sampleRate = sr;
        const int maxSamples = (int) (sr * 2.1) + 8;   // 2 s máx + holgura
        buf.assign ((size_t) juce::jmax (8, maxSamples), 0.0f);
        writePos = 0; lpState = 0.0f;
    }
    void reset() override { std::fill (buf.begin(), buf.end(), 0.0f); writePos = 0; lpState = 0.0f; }

    void processMono (float* data, int n) override
    {
        const int   size    = (int) buf.size();
        const float fb      = pv (1);
        const float mix     = pv (2);
        const int   delSamp = juce::jlimit (1, size - 1, (int) (pv (0) * 0.001f * (float) sampleRate));
        const float fc      = pv (3);
        const float lpCoef  = std::exp (-2.0f * juce::MathConstants<float>::pi * fc / (float) sampleRate);
        for (int i = 0; i < n; ++i)
        {
            int readPos = writePos - delSamp;
            if (readPos < 0) readPos += size;
            const float delayed = buf[(size_t) readPos];

            // tono: LP one-pole sobre la señal retardada antes de realimentar
            lpState = delayed * (1.0f - lpCoef) + lpState * lpCoef;
            buf[(size_t) writePos] = data[i] + lpState * fb;

            data[i] = data[i] * (1.0f - mix) + delayed * mix;
            if (++writePos >= size) writePos = 0;
        }
    }
private:
    double sampleRate { 48000.0 };
    std::vector<float> buf;
    int   writePos { 0 };
    float lpState { 0.0f };
};
