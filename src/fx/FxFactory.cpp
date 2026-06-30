#include "FxFactory.h"
// Modulación
#include "blocks/FxChorus.h"
#include "blocks/FxFlanger.h"
#include "blocks/FxPhaser.h"
#include "blocks/FxTremolo.h"
#include "blocks/FxHarmTremolo.h"
#include "blocks/FxVibrato.h"
#include "blocks/FxUniVibe.h"
#include "blocks/FxRingMod.h"
#include "blocks/FxSweep.h"
// Delay
#include "blocks/FxDelay.h"
#include "blocks/FxTapeEcho.h"
#include "blocks/FxBBD.h"
#include "blocks/FxMultiTap.h"
#include "blocks/FxReverse.h"
#include "blocks/FxDucking.h"
// Pitch
#include "blocks/FxOctaver.h"
#include "blocks/FxPitch.h"
// Reverb
#include "blocks/FxReverb.h"
#include "blocks/FxHall.h"
#include "blocks/FxPlate.h"
#include "blocks/FxDuckReverb.h"
#include "blocks/FxGatedReverb.h"

namespace FxFactory
{
    std::unique_ptr<FxBlock> create (const juce::String& t)
    {
        // Modulación
        if (t == "chorus")   return std::make_unique<FxChorus>();
        if (t == "flanger")  return std::make_unique<FxFlanger>();
        if (t == "phaser")   return std::make_unique<FxPhaser>();
        if (t == "tremolo")  return std::make_unique<FxTremolo>();
        if (t == "harmtrem") return std::make_unique<FxHarmTremolo>();
        if (t == "vibrato")  return std::make_unique<FxVibrato>();
        if (t == "univibe")  return std::make_unique<FxUniVibe>();
        if (t == "ringmod")  return std::make_unique<FxRingMod>();
        if (t == "sweep")    return std::make_unique<FxSweep>();
        // Delay
        if (t == "delay")    return std::make_unique<FxDelay>();
        if (t == "tapeecho") return std::make_unique<FxTapeEcho>();
        if (t == "bbd")      return std::make_unique<FxBBD>();
        if (t == "multitap") return std::make_unique<FxMultiTap>();
        if (t == "reverse")  return std::make_unique<FxReverse>();
        if (t == "ducking")  return std::make_unique<FxDucking>();
        // Pitch
        if (t == "octaver")  return std::make_unique<FxOctaver>();
        if (t == "pitch")    return std::make_unique<FxPitch>();
        // Reverb
        if (t == "reverb")   return std::make_unique<FxReverb>();
        if (t == "hall")     return std::make_unique<FxHall>();
        if (t == "plate")    return std::make_unique<FxPlate>();
        if (t == "duckrev")  return std::make_unique<FxDuckReverb>();
        if (t == "gated")    return std::make_unique<FxGatedReverb>();
        return nullptr;
    }

    juce::Array<TypeInfo> available()
    {
        return {
            TypeInfo { "chorus",   "Chorus",         "Modulacion" },
            TypeInfo { "flanger",  "Flanger",        "Modulacion" },
            TypeInfo { "phaser",   "Phaser",         "Modulacion" },
            TypeInfo { "tremolo",  "Tremolo",        "Modulacion" },
            TypeInfo { "harmtrem", "Harm Tremolo",   "Modulacion" },
            TypeInfo { "vibrato",  "Vibrato",        "Modulacion" },
            TypeInfo { "univibe",  "Uni-Vibe",       "Modulacion" },
            TypeInfo { "ringmod",  "Ring Mod",       "Modulacion" },
            TypeInfo { "sweep",    "Filter Sweep",   "Modulacion" },

            TypeInfo { "delay",    "Delay",          "Delay" },
            TypeInfo { "tapeecho", "Tape Echo",      "Delay" },
            TypeInfo { "bbd",      "Analog Delay",   "Delay" },
            TypeInfo { "multitap", "Multi-Tap",      "Delay" },
            TypeInfo { "reverse",  "Reverse Delay",  "Delay" },
            TypeInfo { "ducking",  "Ducking Delay",  "Delay" },

            TypeInfo { "octaver",  "Octaver",        "Pitch" },
            TypeInfo { "pitch",    "Pitch",          "Pitch" },

            TypeInfo { "reverb",   "Reverb",         "Reverb" },
            TypeInfo { "hall",     "Hall",           "Reverb" },
            TypeInfo { "plate",    "Plate",          "Reverb" },
            TypeInfo { "duckrev",  "Ducking Reverb", "Reverb" },
            TypeInfo { "gated",    "Gated Reverb",   "Reverb" },
        };
    }
}
