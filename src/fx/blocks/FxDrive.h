#pragma once
#include "../FxBlock.h"
#include <cmath>

// Drive (overdrive por soft-clip tanh, pre-amp). Ancla de la cadena: no se
// quita, pero se puede mover y bypassear. Arranca bypasseado (off por defecto).
class FxDrive : public FxBlock
{
public:
    FxDrive()
    {
        add ("amount", "Drive", 0.0f, 1.0f, 0.3f, "%");
        add ("level",  "Level", 0.0f, 1.0f, 0.7f, "%");
        bypassed.store (true);
    }
    juce::String typeId() const override { return "drive"; }
    juce::String displayName() const override { return "Drive"; }
    juce::String kind() const override { return "drive"; }
    bool removable() const override { return false; }

    void prepare (double, int) override {}
    void reset() override {}

    void processMono (float* data, int n) override
    {
        const double g     = 1.0 + (double) pv (0) * 30.0;   // 1..31
        const double level = (double) pv (1);
        for (int i = 0; i < n; ++i)
            data[i] = (float) (std::tanh ((double) data[i] * g) * level);
    }
};
