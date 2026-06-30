#pragma once
#include "../FxBlock.h"
#include <cmath>
#include <vector>
#include <algorithm>

// Reverse delay: graba la entrada en segmentos de longitud fija y reproduce cada
// segmento HACIA ATRAS. En el borde de cada segmento (cuando la lectura inversa
// llega al inicio) se reinicia con un crossfade raised-cosine para evitar clicks.
// Pequeño feedback re-inyecta el audio invertido. Mezcla wet/dry.
class FxReverse : public FxBlock
{
public:
    FxReverse()
    {
        add ("time",     "Time",     100.0f, 1500.0f, 400.0f, "ms");
        add ("feedback", "Feedback", 0.0f,   0.8f,    0.3f,   "%");
        add ("mix",      "Mix",      0.0f,   1.0f,    0.4f,   "%");
    }
    juce::String typeId() const override { return "reverse"; }
    juce::String displayName() const override { return "Reverse Delay"; }

    void prepare (double sr, int) override
    {
        sampleRate = sr;
        // Buffer dimensionado para el time maximo (1500 ms) + holgura.
        const int maxSamples = (int) (sr * 1.6) + 8;
        buf.assign ((size_t) juce::jmax (8, maxSamples), 0.0f);
        // Crossfade fijo de ~10 ms (raised-cosine), acotado.
        xfadeLen = juce::jlimit (1, maxSamples / 2, (int) (sr * 0.010));
        writePos = 0;
        readPos  = 0.0f;
        segLen   = 0;
        fadePos  = xfadeLen; // empezar sin crossfade activo
    }

    void reset() override
    {
        std::fill (buf.begin(), buf.end(), 0.0f);
        writePos = 0;
        readPos  = 0.0f;
        segLen   = 0;
        fadePos  = xfadeLen;
    }

    void processMono (float* data, int n) override
    {
        const int   size = (int) buf.size();
        const float fb   = juce::jlimit (0.0f, 0.8f, pv (1));
        const float mix  = juce::jlimit (0.0f, 1.0f, pv (2));

        // Longitud del segmento desde el parametro time (en samples), acotada.
        int wantSeg = (int) (pv (0) * 0.001f * (float) sampleRate);
        wantSeg = juce::jlimit (xfadeLen + 1, size - 1, wantSeg);
        if (segLen == 0) { segLen = wantSeg; readPos = (float) (segLen - 1); }

        for (int i = 0; i < n; ++i)
        {
            // Lectura inversa con interpolacion lineal del segmento actual.
            const float wet = readReverse (readPos, size);

            // Crossfade raised-cosine en el borde del segmento: mezclamos la cola
            // del segmento que termina con la cabeza (final fisico) del siguiente.
            float out = wet;
            if (fadePos < xfadeLen)
            {
                // tail = inicio del segmento (readPos cerca de 0)
                // head = arranque del nuevo segmento leido desde el final
                const float headPos = (float) (newSegLen - 1) - (float) fadePos;
                const float head    = readReverse (headPos, size);
                const float t = (float) fadePos / (float) xfadeLen;     // 0..1
                const float g = 0.5f * (1.0f - std::cos (juce::MathConstants<float>::pi * t)); // raised-cos
                out = wet * (1.0f - g) + head * g;
                ++fadePos;
            }

            // Guardar el nuevo input (mas feedback del wet invertido) en el buffer.
            float w = data[i] + out * fb;
            if (! std::isfinite (w)) w = 0.0f;
            w = juce::jlimit (-2.0f, 2.0f, w);
            buf[(size_t) writePos] = w;

            // Mezcla wet/dry.
            float y = data[i] * (1.0f - mix) + out * mix;
            if (! std::isfinite (y)) y = 0.0f;
            data[i] = juce::jlimit (-1.0f, 1.0f, y);

            // Avanzar escritura.
            if (++writePos >= size) writePos = 0;

            // Avanzar lectura inversa.
            readPos -= 1.0f;
            if (readPos < 0.0f)
            {
                // Fin del segmento: reiniciar y disparar crossfade.
                segLen   = wantSeg;     // adoptar el time actual al reiniciar
                newSegLen = segLen;
                readPos  = (float) (segLen - 1);
                fadePos  = 0;
            }
        }
    }

private:
    // Lee el buffer en posicion fraccional contada hacia atras desde writePos.
    // pos es la distancia (en samples) "hacia el pasado" desde la escritura.
    float readReverse (float pos, int size) const
    {
        float rp = (float) writePos - 1.0f - pos;
        while (rp < 0.0f)            rp += (float) size;
        while (rp >= (float) size)   rp -= (float) size;
        const int   i0   = (int) rp;
        const int   i1   = (i0 + 1) % size;
        const float frac = rp - (float) i0;
        return buf[(size_t) i0] * (1.0f - frac) + buf[(size_t) i1] * frac;
    }

    double sampleRate { 48000.0 };
    std::vector<float> buf;
    int   writePos  { 0 };
    float readPos   { 0.0f };
    int   segLen    { 0 };
    int   newSegLen { 0 };
    int   xfadeLen  { 1 };
    int   fadePos   { 1 };
};
