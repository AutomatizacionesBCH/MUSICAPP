#pragma once
#include "../FxBlock.h"
#include <cmath>

// Octaver analogico (octava ABAJO, NO pitch shifter): low-pass de la entrada para
// ayudar al tracking, detecta cruces por cero ascendentes y togglea un flip-flop
// (+1/-1) en cada uno -> onda cuadrada a la mitad de la frecuencia (una octava
// abajo). Esa cuadrada se low-pasa y se mezcla con la senal seca. Glitchy en
// acordes es el comportamiento autentico de un sub octaver analogico.
class FxOctaver : public FxBlock
{
public:
    FxOctaver()
    {
        add ("octave", "Octave", 0.0f,   1.0f,    0.6f,   "%");   // nivel del sub
        add ("dry",    "Dry",    0.0f,   1.0f,    0.7f,   "%");   // nivel de la senal seca
        add ("tone",   "Tone",   200.0f, 2000.0f, 800.0f, "Hz");  // LP de tracking + sub
    }
    juce::String typeId() const override { return "octaver"; }
    juce::String displayName() const override { return "Octaver"; }

    void prepare (double sr, int) override
    {
        sampleRate = sr;
        reset();
    }
    void reset() override
    {
        trackLp = 0.0f;
        subLp   = 0.0f;
        prevTrack = 0.0f;
        flip = 1.0f;
    }

    void processMono (float* data, int n) override
    {
        const float octLevel = pv (0);
        const float dryLevel = pv (1);
        const float fc       = juce::jlimit (20.0f, 0.45f * (float) sampleRate, pv (2));
        const float lpCoef   = std::exp (-2.0f * juce::MathConstants<float>::pi * fc / (float) sampleRate);

        for (int i = 0; i < n; ++i)
        {
            const float in = data[i];

            // 1) low-pass de la entrada para limpiar armonicos y ayudar al tracking
            trackLp = in * (1.0f - lpCoef) + trackLp * lpCoef;

            // 2) deteccion de cruce por cero ASCENDENTE -> togglea el flip-flop
            if (prevTrack <= 0.0f && trackLp > 0.0f)
                flip = -flip;
            prevTrack = trackLp;

            // 3) cuadrada a media frecuencia, escalada por la envolvente del track
            //    (para que el sub siga la dinamica y no suene a gate duro)
            const float env  = std::fabs (trackLp);
            const float sub  = flip * env;

            // 4) low-pass del sub para suavizar la cuadrada
            subLp = sub * (1.0f - lpCoef) + subLp * lpCoef;

            // 5) mezcla seca + sub
            float out = dryLevel * in + octLevel * 2.0f * subLp;

            if (! std::isfinite (out)) out = 0.0f;
            data[i] = juce::jlimit (-1.0f, 1.0f, out);
        }
    }
private:
    double sampleRate { 48000.0 };
    float  trackLp   { 0.0f };
    float  subLp     { 0.0f };
    float  prevTrack { 0.0f };
    float  flip      { 1.0f };   // +1 / -1 flip-flop
};
