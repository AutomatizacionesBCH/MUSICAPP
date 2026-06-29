#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <filesystem>

//==============================================================================
namespace
{
    // TEMPORAL (spike): modelo de ejemplo que viene con NeuralAmpModelerCore,
    // para que la app suene desde el primer arranque sin tener que cargar nada.
    // En Fase 2 esto lo reemplaza el buscador de tone3000.
    const juce::File kDefaultModel {
        "/Users/automatizacionesbch/Desktop/AGENTES IA/Music App/libs/"
        "NeuralAmpModelerCore/example_models/wavenet_a2_max.nam"
    };
}

//==============================================================================
MusicAppAudioProcessor::MusicAppAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    mInputGainDb  = apvts.getRawParameterValue ("inputGain");
    mOutputGainDb = apvts.getRawParameterValue ("outputGain");

    // Carga inicial del modelo de ejemplo (el audio aún no corre, así que es
    // seguro asignar mModel directamente; el Reset se hará en prepareToPlay).
    if (kDefaultModel.existsAsFile())
    {
        try
        {
            mModel = nam::get_dsp (std::filesystem::path (kDefaultModel.getFullPathName().toStdString()));
            if (mModel != nullptr)
                mLoadedModelName = kDefaultModel.getFileName();
        }
        catch (const std::exception& e)
        {
            juce::Logger::writeToLog (juce::String ("NAM load (default) falló: ") + e.what());
        }
    }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout MusicAppAudioProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "inputGain", 1 }, "Input Gain",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "outputGain", 1 }, "Output Gain",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));

    return layout;
}

void MusicAppAudioProcessor::prepareModel (nam::DSP& model) const
{
    model.Reset (mSampleRate, mBlockSize);
    model.prewarm();
}

//==============================================================================
void MusicAppAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    mSampleRate = sampleRate;
    mBlockSize  = samplesPerBlock;

    // Holgura para que el callback nunca tenga que reasignar.
    const size_t cap = (size_t) juce::jmax (samplesPerBlock, 8192);
    mInScratch.assign  (cap, 0.0);
    mOutScratch.assign (cap, 0.0);

    if (mModel != nullptr)
        prepareModel (*mModel);
}

bool MusicAppAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono()
        || out == juce::AudioChannelSet::stereo();
}

//==============================================================================
void MusicAppAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numCh      = buffer.getNumChannels();

    // Adoptar un modelo recién preparado (lock-free). Liberar el anterior aquí
    // sólo ocurre en una carga manual (raro), no en cada buffer. Spike OK;
    // en Fase 3 pasamos a un staging que libera fuera del audio thread.
    if (auto* staged = mStagedModel.exchange (nullptr))
        mModel.reset (staged);

    if (numCh == 0 || numSamples <= 0)
        return;

    const float inGain  = juce::Decibels::decibelsToGain (mInputGainDb->load());
    const float outGain = juce::Decibels::decibelsToGain (mOutputGainDb->load());

    const int n = juce::jmin (numSamples, (int) mInScratch.size());

    // Guitarra = canal de entrada 0. Copiamos a scratch (con input gain) ANTES
    // de escribir salidas para no corromper la lectura.
    const float* in = buffer.getReadPointer (0);
    for (int i = 0; i < n; ++i)
        mInScratch[(size_t) i] = (double) (in[i] * inGain);

    if (mModel != nullptr)
    {
        double* inPtr[1]  = { mInScratch.data() };
        double* outPtr[1] = { mOutScratch.data() };
        mModel->process (inPtr, outPtr, n);
    }
    else
    {
        // Sin modelo: passthrough.
        for (int i = 0; i < n; ++i)
            mOutScratch[(size_t) i] = mInScratch[(size_t) i];
    }

    // Volcamos el resultado (mono) a todas las salidas, con output gain.
    for (int ch = 0; ch < numCh; ++ch)
    {
        float* out = buffer.getWritePointer (ch);
        for (int i = 0; i < n; ++i)
            out[i] = (float) (mOutScratch[(size_t) i] * outGain);
        // Silencio si el buffer fuese mayor que el scratch (no debería pasar).
        for (int i = n; i < numSamples; ++i)
            out[i] = 0.0f;
    }
}

//==============================================================================
bool MusicAppAudioProcessor::loadNamModel (const juce::File& file)
{
    if (! file.existsAsFile())
        return false;

    try
    {
        auto dsp = nam::get_dsp (std::filesystem::path (file.getFullPathName().toStdString()));
        if (dsp == nullptr)
            return false;

        prepareModel (*dsp);                          // Reset + prewarm fuera del audio thread

        // Entregar al audio thread; si había uno en cola sin adoptar, liberarlo.
        nam::DSP* previouslyStaged = mStagedModel.exchange (dsp.release());
        delete previouslyStaged;

        mLoadedModelName = file.getFileName();
        return true;
    }
    catch (const std::exception& e)
    {
        juce::Logger::writeToLog (juce::String ("NAM load falló: ") + e.what());
        return false;
    }
}

//==============================================================================
void MusicAppAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void MusicAppAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* MusicAppAudioProcessor::createEditor()
{
    return new MusicAppAudioProcessorEditor (*this);
}

//==============================================================================
// Punto de entrada del plugin (requerido por JUCE)
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MusicAppAudioProcessor();
}
