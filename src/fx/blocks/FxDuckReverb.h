#pragma once
#include "../FxBlock.h"
#include <cmath>
#include <vector>
#include <algorithm>

// Ducking reverb: juce::Reverb cuya cola WET se atenua mientras tocas. Un
// peak-follower one-pole sobre la senal DRY de entrada (ataque rapido, release
// por parametro) controla cuanto se "agacha" la reverb. wetGain = mix * (1 - duck*env).
// Procesamos la reverb en un buffer temporal (reservado en prepare) y mezclamos
// dry + wetGain*wet muestra a muestra.
class FxDuckReverb : public FxBlock
{
public:
    FxDuckReverb()
    {
        add ("mix",     "Mix",     0.0f,  1.0f,    0.35f, "%");
        add ("decay",   "Decay",   0.0f,  1.0f,    0.7f,  "%");
        add ("damping", "Damping", 0.0f,  1.0f,    0.4f,  "%");
        add ("duck",    "Duck",    0.0f,  1.0f,    0.7f,  "%");
        add ("release", "Release", 50.0f, 1000.0f, 300.0f, "ms");
    }
    juce::String typeId() const override { return "duckrev"; }
    juce::String displayName() const override { return "Ducking Reverb"; }

    void prepare (double sr, int blockSize) override
    {
        sampleRate = sr;
        rev.setSampleRate (sr);
        rev.reset();
        wet.assign ((size_t) juce::jmax (8, blockSize), 0.0f);
        env = 0.0f;
    }
    void reset() override
    {
        rev.reset();
        std::fill (wet.begin(), wet.end(), 0.0f);
        env = 0.0f;
    }

    void processMono (float* data, int n) override
    {
        // Asegura espacio en el buffer wet (no deberia reasignar si blockSize fijo).
        if ((int) wet.size() < n)
            return;   // RT-safe: nunca asignar en el audio thread; si pasa, no procesar

        const float mix     = pv (0);
        const float decay   = pv (1);
        const float damping = pv (2);
        const float duck    = juce::jlimit (0.0f, 1.0f, pv (3));
        const float relMs   = juce::jmax (1.0f, pv (4));

        // Reverb 100% wet en el buffer temporal (el blend lo hacemos nosotros).
        juce::Reverb::Parameters p;
        p.roomSize   = juce::jlimit (0.0f, 1.0f, decay);
        p.damping    = juce::jlimit (0.0f, 1.0f, damping);
        p.width      = 1.0f;
        p.freezeMode = 0.0f;
        p.wetLevel   = 1.0f;
        p.dryLevel   = 0.0f;
        rev.setParameters (p);

        // Copia dry -> wet, luego procesa wet in-place como senal de reverb pura.
        std::copy (data, data + n, wet.begin());
        rev.processMono (wet.data(), n);

        // Peak follower one-pole sobre la senal DRY: ataque rapido, release lento.
        const float atkCoef = std::exp (-1.0f / (0.002f * (float) sampleRate));        // ~2 ms
        const float relCoef = std::exp (-1.0f / (relMs * 0.001f * (float) sampleRate)); // por parametro

        for (int i = 0; i < n; ++i)
        {
            const float dry   = data[i];
            const float level = std::fabs (dry);
            if (level > env) env = atkCoef * (env - level) + level;
            else             env = relCoef * (env - level) + level;

            const float wetGain = mix * (1.0f - duck * juce::jlimit (0.0f, 1.0f, env));
            float out = dry + wetGain * wet[(size_t) i];

            if (! std::isfinite (out)) out = 0.0f;
            data[i] = juce::jlimit (-1.0f, 1.0f, out);
        }
    }
private:
    double            sampleRate { 48000.0 };
    juce::Reverb      rev;
    std::vector<float> wet;
    float             env { 0.0f };
};
