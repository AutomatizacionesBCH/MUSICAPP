#pragma once
#include "../FxBlock.h"
#include "../FxHost.h"

// Ancla CAB: posiciona la convolución del Cabinet IR en la cadena. No posee la
// convolución (la gestiona el processor); invoca host->hostProcessCab en su
// turno. El bypass del bloque ES el on/off del IR. UI especial: foto + cargar IR.
// Arranca bypasseado (el modelo por defecto suele ser amp+cab -> sin doble cab).
class FxCabRef : public FxBlock
{
public:
    explicit FxCabRef (FxHost* h) : host (h) { bypassed.store (true); }

    juce::String typeId() const override { return "cab"; }
    juce::String displayName() const override { return "Cab"; }
    juce::String kind() const override { return "cab"; }
    bool removable() const override { return false; }

    juce::var extra() const override
    {
        juce::DynamicObject::Ptr o = new juce::DynamicObject();
        o->setProperty ("loaded", host != nullptr ? host->hostCabName() : juce::String());
        return juce::var (o.get());
    }

    void prepare (double, int) override {}     // el processor prepara la convolución
    void reset() override {}
    void processMono (float* data, int n) override { if (host) host->hostProcessCab (data, n); }

private:
    FxHost* host { nullptr };
};
