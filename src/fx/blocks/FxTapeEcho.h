#pragma once
#include "../FxBlock.h"
#include <cmath>
#include <vector>
#include <algorithm>

// Tape Echo: delay con lectura FRACCIONAL cuya posicion oscila por un LFO lento
// de wow/flutter (suma de un seno lento + uno mas rapido, profundidad ~ wow*2ms).
// El lazo de feedback pasa por un soft-clip tanh (drive) y un low-pass one-pole
// (perdida de agudos de la cinta). Emula un echo de cinta analogico.
class FxTapeEcho : public FxBlock
{
public:
    FxTapeEcho()
    {
        add ("time",     "Time",     20.0f, 1500.0f, 350.0f, "ms");
        add ("feedback", "Feedback", 0.0f,  0.9f,    0.4f,   "%");
        add ("mix",      "Mix",      0.0f,  1.0f,    0.3f,   "%");
        add ("wow",      "Wow",      0.0f,  1.0f,    0.3f,   "%");
        add ("drive",    "Drive",    0.0f,  1.0f,    0.3f,   "%");
    }
    juce::String typeId() const override { return "tapeecho"; }
    juce::String displayName() const override { return "Tape Echo"; }

    void prepare (double sr, int) override
    {
        sampleRate = sr;
        const int maxSamples = (int) (sr * 1.6) + 8;   // 1.6 s max + holgura
        buf.assign ((size_t) juce::jmax (8, maxSamples), 0.0f);
        writePos = 0; lpState = 0.0f; phaseWow = 0.0f; phaseFlut = 0.0f;
    }
    void reset() override
    {
        std::fill (buf.begin(), buf.end(), 0.0f);
        writePos = 0; lpState = 0.0f; phaseWow = 0.0f; phaseFlut = 0.0f;
    }

    void processMono (float* data, int n) override
    {
        const int   size  = (int) buf.size();
        const float fb    = juce::jlimit (0.0f, 0.95f, pv (1));
        const float mix   = pv (2);
        const float wow   = pv (3);
        const float drive = pv (4);

        // Retardo base en muestras (limitado al buffer con holgura para el wobble).
        const float baseDelMs = pv (0);
        float baseDelS = baseDelMs * 0.001f * (float) sampleRate;
        baseDelS = juce::jlimit (1.0f, (float) (size - 4), baseDelS);

        // LFO wow/flutter: seno lento (~0.6 Hz) + seno mas rapido (~6 Hz), tenue.
        const float wowInc  = juce::MathConstants<float>::twoPi * 0.6f / (float) sampleRate;
        const float flutInc = juce::MathConstants<float>::twoPi * 6.0f / (float) sampleRate;
        const float depthS  = wow * 0.002f * (float) sampleRate;   // ~wow*2ms

        // LP one-pole para perdida de agudos en el lazo (mas oscuro con drive).
        const float fc     = 4500.0f - drive * 2000.0f;            // 4500..2500 Hz
        const float lpCoef = std::exp (-2.0f * juce::MathConstants<float>::pi * fc / (float) sampleRate);

        // Ganancia de drive para el tanh (1..~4) compensada a la salida del clip.
        const float driveGain = 1.0f + drive * 3.0f;

        for (int i = 0; i < n; ++i)
        {
            const float wobble = depthS * (0.7f * std::sin (phaseWow)
                                         + 0.3f * std::sin (phaseFlut));
            float rp = (float) writePos - (baseDelS + wobble);
            while (rp < 0.0f)            rp += (float) size;
            while (rp >= (float) size)   rp -= (float) size;

            const int   i0   = (int) rp;
            const int   i1   = (i0 + 1) % size;
            const float frac = rp - (float) i0;
            float delayed = buf[(size_t) i0] * (1.0f - frac) + buf[(size_t) i1] * frac;

            // Lazo de feedback: soft-clip tanh (saturacion de cinta) + LP.
            float fbSig = std::tanh (delayed * driveGain) / std::tanh (driveGain);
            lpState = fbSig * (1.0f - lpCoef) + lpState * lpCoef;

            float w = data[i] + lpState * fb;
            if (! std::isfinite (w)) w = 0.0f;
            buf[(size_t) writePos] = juce::jlimit (-4.0f, 4.0f, w);

            data[i] = data[i] * (1.0f - mix) + delayed * mix;
            if (++writePos >= size) writePos = 0;

            phaseWow += wowInc;
            if (phaseWow >= juce::MathConstants<float>::twoPi) phaseWow -= juce::MathConstants<float>::twoPi;
            phaseFlut += flutInc;
            if (phaseFlut >= juce::MathConstants<float>::twoPi) phaseFlut -= juce::MathConstants<float>::twoPi;
        }
    }
private:
    double sampleRate { 48000.0 };
    std::vector<float> buf;
    int   writePos { 0 };
    float lpState { 0.0f };
    float phaseWow { 0.0f };
    float phaseFlut { 0.0f };
};
