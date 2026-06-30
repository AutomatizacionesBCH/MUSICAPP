#pragma once
#include "../FxBlock.h"
#include <cmath>
#include <vector>
#include <algorithm>

// Ducking delay: un delay normal cuyo nivel WET baja mientras tocas. La envolvente
// de pico de la señal seca se sigue con un peak-follower one-pole (attack rapido
// ~5 ms, release por parametro). wetGain = 1 - duck * env (clamp >= 0). Asi las
// repeticiones se "esconden" debajo de las notas y aparecen en los silencios.
class FxDucking : public FxBlock
{
public:
    FxDucking()
    {
        add ("time",     "Time",     20.0f,  1500.0f, 350.0f, "ms");
        add ("feedback", "Feedback", 0.0f,   0.9f,    0.4f,   "%");
        add ("mix",      "Mix",      0.0f,   1.0f,    0.4f,   "%");
        add ("duck",     "Duck",     0.0f,   1.0f,    0.7f,   "%");
        add ("release",  "Release",  50.0f,  1000.0f, 300.0f, "ms");
    }
    juce::String typeId() const override { return "ducking"; }
    juce::String displayName() const override { return "Ducking Delay"; }

    void prepare (double sr, int) override
    {
        sampleRate = sr;
        const int maxSamples = (int) (sr * 1.6) + 8;   // 1.5 s max + holgura
        buf.assign ((size_t) juce::jmax (8, maxSamples), 0.0f);
        writePos = 0;
        env      = 0.0f;
    }
    void reset() override
    {
        std::fill (buf.begin(), buf.end(), 0.0f);
        writePos = 0;
        env      = 0.0f;
    }

    void processMono (float* data, int n) override
    {
        const int   size    = (int) buf.size();
        const float fb      = juce::jlimit (0.0f, 0.95f, pv (1));
        const float mix     = pv (2);
        const float duck    = pv (3);
        const int   delSamp = juce::jlimit (1, size - 1, (int) (pv (0) * 0.001f * (float) sampleRate));

        // Coeficientes del peak-follower: attack fijo ~5 ms, release por parametro.
        const float atkMs   = 5.0f;
        const float relMs   = pv (4);
        const float atkCoef = std::exp (-1.0f / (juce::jmax (1.0f, atkMs * 0.001f * (float) sampleRate)));
        const float relCoef = std::exp (-1.0f / (juce::jmax (1.0f, relMs * 0.001f * (float) sampleRate)));

        for (int i = 0; i < n; ++i)
        {
            const float dry = data[i];

            // Peak follower sobre la senal seca de entrada.
            const float rect = std::fabs (dry);
            if (rect > env) env = rect + atkCoef * (env - rect);
            else            env = rect + relCoef * (env - rect);
            if (! std::isfinite (env)) env = 0.0f;

            // Ducking del nivel wet: mas senal de entrada => menos wet.
            const float wetGain = juce::jlimit (0.0f, 1.0f, 1.0f - duck * env);

            int readPos = writePos - delSamp;
            if (readPos < 0) readPos += size;
            const float delayed = buf[(size_t) readPos];

            buf[(size_t) writePos] = dry + delayed * fb;
            if (++writePos >= size) writePos = 0;

            const float wet = delayed * wetGain;
            float out = dry * (1.0f - mix) + wet * mix;
            if (! std::isfinite (out)) out = 0.0f;
            data[i] = juce::jlimit (-1.5f, 1.5f, out);
        }
    }
private:
    double sampleRate { 48000.0 };
    std::vector<float> buf;
    int   writePos { 0 };
    float env      { 0.0f };
};
