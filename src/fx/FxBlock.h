#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <memory>
#include <vector>

//==============================================================================
// Un parámetro de un efecto del rack. Valor atómico: lo escribe el message
// thread (UI) y lo lee el audio thread sin locks.
struct FxParam
{
    FxParam (juce::String pid, juce::String plabel,
             float pmin, float pmax, float pdef, juce::String punit = {})
        : id (std::move (pid)), label (std::move (plabel)), unit (std::move (punit)),
          minV (pmin), maxV (pmax), defV (pdef), value (pdef) {}

    const juce::String id, label, unit;
    const float minV, maxV, defV;
    std::atomic<float> value;

    float get() const noexcept { return value.load (std::memory_order_relaxed); }
    void  set (float v) noexcept { value.store (juce::jlimit (minV, maxV, v), std::memory_order_relaxed); }
};

//==============================================================================
// Interfaz base de un bloque de efecto del rack flexible. Cada efecto concreto
// (chorus, delay, reverb, ...) hereda de aquí, declara sus FxParam y procesa
// MONO in-place. El audio thread sólo llama processMono(); prepare/reset y la
// construcción ocurren en el message thread.
class FxBlock
{
public:
    virtual ~FxBlock() = default;

    virtual juce::String typeId() const = 0;        // "chorus", "delay", ...
    virtual juce::String displayName() const = 0;   // "Chorus"
    virtual void prepare (double sampleRate, int blockSize) = 0;
    virtual void reset() = 0;
    virtual void processMono (float* data, int n) = 0;   // in-place, mono

    int uid = 0;                                  // id de instancia (lo asigna FxChain)
    std::atomic<bool> bypassed { false };
    std::vector<std::unique_ptr<FxParam>> params;

    FxParam* param (const juce::String& pid) const
    {
        for (auto& p : params) if (p->id == pid) return p.get();
        return nullptr;
    }
    float pv (int idx) const noexcept { return params[(size_t) idx]->get(); }

protected:
    FxParam* add (juce::String id, juce::String label,
                  float mn, float mx, float def, juce::String unit = {})
    {
        params.push_back (std::make_unique<FxParam> (std::move (id), std::move (label),
                                                     mn, mx, def, std::move (unit)));
        return params.back().get();
    }
};
