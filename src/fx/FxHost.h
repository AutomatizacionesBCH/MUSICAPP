#pragma once
#include <JuceHeader.h>

// Interfaz que el processor implementa para que los bloques "ancla" (Amp/Cab)
// puedan invocar el motor pesado (NAM / convolución IR) sin poseerlo. Así toda
// la cadena (Drive, Amp, Cab y efectos) vive en una sola lista reordenable,
// pero la lógica de NAM/IR sigue en el processor (bajo riesgo).
struct FxHost
{
    virtual ~FxHost() = default;
    virtual void hostProcessAmp (float* data, int n) = 0;   // NAM
    virtual void hostProcessCab (float* data, int n) = 0;   // Cabinet IR (convolución)
    virtual juce::String hostAmpName() = 0;                 // nombre del modelo cargado
    virtual juce::String hostCabName() = 0;                 // nombre del IR cargado
};
