#pragma once
#include "../FxBlock.h"
#include "../FxHost.h"

// Ancla AMP: posiciona el modelo NAM dentro de la cadena. No posee el modelo
// (lo sigue gestionando el processor); sólo invoca host->hostProcessAmp en su
// turno. Sin perillas (NAM es caja negra). UI especial: foto + "modelo cargado".
class FxAmpRef : public FxBlock
{
public:
    explicit FxAmpRef (FxHost* h) : host (h) {}

    juce::String typeId() const override { return "amp"; }
    juce::String displayName() const override { return "Amp"; }
    juce::String kind() const override { return "amp"; }
    bool removable() const override { return false; }

    juce::var extra() const override
    {
        juce::DynamicObject::Ptr o = new juce::DynamicObject();
        o->setProperty ("loaded", host != nullptr ? host->hostAmpName() : juce::String());
        return juce::var (o.get());
    }

    void prepare (double, int) override {}     // el processor prepara el modelo
    void reset() override {}
    void processMono (float* data, int n) override { if (host) host->hostProcessAmp (data, n); }

private:
    FxHost* host { nullptr };
};
