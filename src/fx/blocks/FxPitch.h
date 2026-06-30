#pragma once
#include "../FxBlock.h"
#include <cmath>
#include <vector>
#include <algorithm>

// Pitch shifter de intervalo fijo (algoritmo canonico de delay con ventana):
// una linea de retardo y DOS punteros de lectura separados medio buffer. Ambos
// avanzan a velocidad (1 - 2^(semis/12)) respecto al puntero de escritura, asi
// que "derivan" y producen el cambio de tono. Cada tap se pondera con una
// ventana triangular (Hann-like) segun su posicion en el buffer: la ventana es
// maxima cuando el puntero esta lejos de la cabeza de escritura y se desvanece
// en la costura, evitando clicks. Lectura fraccional (lineal).
// out = (1-mix)*dry + mix*shifted. Artifactoso por naturaleza (aceptable).
class FxPitch : public FxBlock
{
public:
    FxPitch()
    {
        add ("pitch", "Pitch", -12.0f, 12.0f, 7.0f, "");
        add ("mix",   "Mix",    0.0f,   1.0f, 0.5f, "%");
    }
    juce::String typeId() const override { return "pitch"; }
    juce::String displayName() const override { return "Pitch"; }

    void prepare (double sr, int) override
    {
        sampleRate = sr;
        const int sz = (int) (sr * 0.080) + 8;          // ~80 ms + holgura
        buf.assign ((size_t) juce::jmax (16, sz), 0.0f);
        writePos = 0;
        const float half = (float) buf.size() * 0.5f;
        read0 = 0.0f;
        read1 = half;                                    // separados medio buffer
    }
    void reset() override
    {
        std::fill (buf.begin(), buf.end(), 0.0f);
        writePos = 0;
        const float half = (float) buf.size() * 0.5f;
        read0 = 0.0f;
        read1 = half;
    }

    void processMono (float* data, int n) override
    {
        const int   size = (int) buf.size();
        const float fsz  = (float) size;
        const float semis = pv (0);
        const float mix   = juce::jlimit (0.0f, 1.0f, pv (1));

        // velocidad de deriva de los punteros respecto a la escritura.
        // speed = 1 - 2^(semis/12): >0 para bajar, <0 para subir tono.
        const float speed = 1.0f - std::pow (2.0f, semis / 12.0f);

        for (int i = 0; i < n; ++i)
        {
            buf[(size_t) writePos] = data[i];

            const float s0 = readTap (read0, size);
            const float s1 = readTap (read1, size);

            // distancia de cada puntero a la cabeza de escritura (0..size),
            // envuelta hacia atras. Ventana triangular: 0 en la costura
            // (puntero junto a la escritura) y 1 en el centro del buffer.
            const float w0 = window (read0, fsz);
            const float w1 = window (read1, fsz);
            float wsum = w0 + w1;
            if (wsum < 1.0e-6f) wsum = 1.0e-6f;
            float shifted = (s0 * w0 + s1 * w1) / wsum;

            if (! std::isfinite (shifted)) shifted = 0.0f;

            data[i] = data[i] * (1.0f - mix) + shifted * mix;

            // avanzar punteros: el de escritura +1, los de lectura +1+speed
            // (derivan a "speed" por muestra). Mantenerlos envueltos en [0,size).
            read0 = wrap (read0 + 1.0f + speed, fsz);
            read1 = wrap (read1 + 1.0f + speed, fsz);
            if (++writePos >= size) writePos = 0;
        }
    }

private:
    // lectura fraccional (lineal) en posicion absoluta del buffer.
    float readTap (float rp, int size) const
    {
        const int   i0   = (int) rp;
        const int   i1   = (i0 + 1) % size;
        const float frac = rp - (float) i0;
        return buf[(size_t) i0] * (1.0f - frac) + buf[(size_t) i1] * frac;
    }

    // distancia "hacia atras" del puntero de lectura a la cabeza de escritura,
    // normalizada 0..1, convertida en ventana triangular que se desvanece en la
    // costura (cuando el tap esta justo en/antes de la escritura).
    float window (float rp, float fsz) const
    {
        float d = (float) writePos - rp;          // muestras de retardo
        while (d < 0.0f)   d += fsz;
        while (d >= fsz)   d -= fsz;
        const float frac = d / fsz;               // 0..1 posicion en el buffer
        // triangular: 0 en los extremos (costura), 1 en el centro.
        return 1.0f - std::fabs (2.0f * frac - 1.0f);
    }

    static float wrap (float x, float fsz)
    {
        while (x >= fsz) x -= fsz;
        while (x < 0.0f) x += fsz;
        return x;
    }

    double sampleRate { 48000.0 };
    std::vector<float> buf;
    int   writePos { 0 };
    float read0 { 0.0f };
    float read1 { 0.0f };
};
