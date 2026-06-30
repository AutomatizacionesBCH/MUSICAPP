#pragma once
#include "FxBlock.h"
#include <vector>
#include <memory>

//==============================================================================
// Rack flexible de efectos: lista ordenable de FxBlock. El audio thread procesa
// con ScopedTryLock (nunca bloquea: si hay una edición estructural en curso,
// pasa ese buffer en seco). Las ediciones estructurales (add/remove/move) toman
// el lock en el message thread. setParam/setBypass son lock-free (sólo tocan
// atómicos), así que girar perillas no produce glitches.
class FxChain
{
public:
    void prepare (double sampleRate, int blockSize);
    void processMono (float* data, int n);                 // audio thread

    // --- message thread ---
    int  add (const juce::String& typeId, int pos = -1);   // devuelve uid (0 si falla)
    void remove (int uid);
    void move (int uid, int newIndex);
    void setBypass (int uid, bool bypassed);
    void setParam (int uid, const juce::String& paramId, float value);
    void clear();

    juce::var       describe() const;                       // JSON para la UI
    juce::ValueTree toValueTree() const;                    // serialización
    void            fromValueTree (const juce::ValueTree& vt);

private:
    FxBlock* find (int uid) const;

    std::vector<std::unique_ptr<FxBlock>> blocks;
    juce::CriticalSection lock;        // sólo ediciones estructurales
    double sampleRate { 48000.0 };
    int    blockSize  { 512 };
    int    nextUid    { 1 };
};
