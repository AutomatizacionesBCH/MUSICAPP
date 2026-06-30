#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <memory>
#include <vector>

#include "NAM/dsp.h"
#include "NAM/get_dsp.h"
#include "fx/FxChain.h"

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
    bool loadIR (const juce::File& file);                          // Cabinet IR (.wav)
    juce::String getLoadedIRName() const { return mIrLoadedName; }
    // Rutas completas (para que la UI resuelva la foto vecina del tono: <dir>/images).
    juce::File getLoadedModelFile() const { return mLoadedModelFile; }
    juce::File getLoadedIRFile()    const { return mIrLoadedFile; }

    // Rack flexible de efectos (mod/delay/pitch/reverb). La UI lo edita por
    // funciones nativas; el audio thread lo procesa post-cab.
    FxChain& fx() { return mFxChain; }

    // Medidores (read-only para la UI): pico [0..1] del último bloque.
    float getInPeak()  const { return mInPeak.load(); }
    float getOutPeak() const { return mOutPeak.load(); }

    // Copia las últimas numSamples muestras de la entrada (mono, pre-NAM) en dest
    // para el afinador. Lectura best-effort fuera del audio thread.
    void getRecentInput (float* dest, int numSamples) const;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void prepareModel (nam::DSP& model) const;
    static float computeNormGain (const nam::DSP& model);   // normalización por loudness

    // El modelo "vivo" lo posee el audio thread; los cambios se entregan por
    // staging atómico (cargar/Reset/prewarm se hacen fuera del audio thread).
    std::unique_ptr<nam::DSP> mModel;
    std::atomic<nam::DSP*> mStagedModel { nullptr };
    std::atomic<float> mNormGain { 1.0f };   // ganancia de normalización por loudness del modelo
    juce::String mLoadedModelName;
    juce::File   mLoadedModelFile;

    double mSampleRate { 48000.0 };
    int    mBlockSize  { 512 };

    // NAM trabaja en double: scratch pre-asignado para no alocar en el callback.
    std::vector<double> mInScratch, mOutScratch;
    std::vector<float>  mWork;            // buffer float de trabajo (post-NAM: reverb/output)

    // Cabinet IR (post-NAM, pre-reverb). loadImpulseResponse hace el swap
    // thread-safe; la convolución no aloca en el callback (buffers de prepare()).
    juce::dsp::Convolution mConvolution;
    juce::String mIrLoadedName;
    juce::File   mIrLoadedFile;

    // Rack flexible de efectos (mod/delay/pitch/reverb), post-cab. Reemplaza a
    // los antiguos chorus/reverb fijos: ahora son bloques que se agregan/mueven.
    FxChain mFxChain;

    std::atomic<float> mInPeak  { 0.0f };
    std::atomic<float> mOutPeak { 0.0f };

    // Ring buffer de la entrada mono (para el afinador; lo lee la UI).
    static constexpr int kInRingSize = 8192;
    std::array<float, kInRingSize> mInRing {};
    std::atomic<int> mInWrite { 0 };

    std::atomic<float>* mInputGainDb  = nullptr;
    std::atomic<float>* mOutputGainDb = nullptr;
    std::atomic<float>* mIrOn          = nullptr;   // bypass del Cabinet IR
    std::atomic<float>* mDriveOn       = nullptr;   // bypass del drive (pre-FX)
    std::atomic<float>* mDriveAmount   = nullptr;
    std::atomic<float>* mDriveLevel    = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MusicAppAudioProcessor)
};
