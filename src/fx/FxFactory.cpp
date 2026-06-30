#include "FxFactory.h"
#include "blocks/FxChorus.h"
#include "blocks/FxFlanger.h"
#include "blocks/FxPhaser.h"
#include "blocks/FxTremolo.h"
#include "blocks/FxDelay.h"
#include "blocks/FxReverb.h"

namespace FxFactory
{
    std::unique_ptr<FxBlock> create (const juce::String& t)
    {
        if (t == "chorus")  return std::make_unique<FxChorus>();
        if (t == "flanger") return std::make_unique<FxFlanger>();
        if (t == "phaser")  return std::make_unique<FxPhaser>();
        if (t == "tremolo") return std::make_unique<FxTremolo>();
        if (t == "delay")   return std::make_unique<FxDelay>();
        if (t == "reverb")  return std::make_unique<FxReverb>();
        return nullptr;
    }

    juce::Array<TypeInfo> available()
    {
        return {
            TypeInfo { "chorus",  "Chorus" },
            TypeInfo { "flanger", "Flanger" },
            TypeInfo { "phaser",  "Phaser" },
            TypeInfo { "tremolo", "Tremolo" },
            TypeInfo { "delay",   "Delay" },
            TypeInfo { "reverb",  "Reverb" },
        };
    }
}
