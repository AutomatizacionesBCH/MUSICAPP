#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <memory>
#include <vector>

#include "NAM/dsp.h"
#include "NAM/get_dsp.h"

//==============================================================================
// Motor de audio "headless": input -> NAM -> output.
// El estado (gains) vive en APVTS; la UI sólo habla por APVTS + métodos públicos.
// (Spike Milestone 1 — sin IR ni efectos todavía.)
//==============================================================================
class MusicAppAudioProcessor : public juce::AudioProcessor
{
public:
    MusicAppAudioProcessor();
    ~MusicAppAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Music App"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    // App-specific (llamado desde el message thread / UI)
    juce::AudioProcessorValueTreeState apvts;
    bool loadNamModel (const juce::File& file);
    juce::String getLoadedModelName() const { return mLoadedModelName; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void prepareModel (nam::DSP& model) const;

    // El modelo "vivo" lo posee el audio thread; los cambios se entregan por
    // staging atómico (cargar/Reset/prewarm se hacen fuera del audio thread).
    std::unique_ptr<nam::DSP> mModel;
    std::atomic<nam::DSP*> mStagedModel { nullptr };
    juce::String mLoadedModelName;

    double mSampleRate { 48000.0 };
    int    mBlockSize  { 512 };

    // NAM trabaja en double: scratch pre-asignado para no alocar en el callback.
    std::vector<double> mInScratch, mOutScratch;

    std::atomic<float>* mInputGainDb  = nullptr;
    std::atomic<float>* mOutputGainDb = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MusicAppAudioProcessor)
};
