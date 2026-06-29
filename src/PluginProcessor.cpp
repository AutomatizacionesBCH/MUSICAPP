#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>
#include <filesystem>

//==============================================================================
namespace
{
    // TEMPORAL (spike): modelo REAL del usuario (captura de tone3000) para que la
    // app suene desde el arranque. OJO: los example_models de NAM Core son fixtures
    // de prueba con salida no calibrada (wavenet_a2_max da ~10x) — no usarlos.
    // En Fase 2 esto lo reemplaza el buscador de tone3000.
    const juce::File kDefaultModel {
        "/Users/automatizacionesbch/Desktop/AGENTES IA/Music App/"
        "Dunlop Eric Johnson Fuzz Face/Dunlop Eric Johnson Fuzz 01.nam"
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
    mReverbMix    = apvts.getRawParameterValue ("reverbMix");

    // Carga inicial del modelo de ejemplo (el audio aún no corre, así que es
    // seguro asignar mModel directamente; el Reset se hará en prepareToPlay).
    if (kDefaultModel.existsAsFile())
    {
        try
        {
            mModel = nam::get_dsp (std::filesystem::path (kDefaultModel.getFullPathName().toStdString()));
            if (mModel != nullptr)
            {
                mLoadedModelName = kDefaultModel.getFileName();
                mNormGain.store (computeNormGain (*mModel));
            }
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

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "reverbMix", 1 }, "Reverb",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));

    return layout;
}

void MusicAppAudioProcessor::prepareModel (nam::DSP& model) const
{
    model.Reset (mSampleRate, mBlockSize);
    model.prewarm();
}

float MusicAppAudioProcessor::computeNormGain (const nam::DSP& model)
{
    // Normaliza la salida del modelo a un loudness objetivo (-18 dB), como hace el
    // plugin oficial de NAM, para que todos los modelos queden a un nivel parejo.
    if (model.HasLoudness())
    {
        const double targetDb = -18.0;
        return (float) std::pow (10.0, (targetDb - model.GetLoudness()) / 20.0);
    }
    return 1.0f;
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
    mWork.assign       (cap, 0.0f);

    mReverb.setSampleRate (sampleRate);
    mReverb.reset();

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

    // 1) Suma de TODAS las entradas a mono (la guitarra puede estar en input 1/L o
    //    2/R) + medición de pico de entrada. Copiamos ANTES de escribir salidas.
    const int numIn = juce::jmin (getTotalNumInputChannels(), numCh);
    float inPeak = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        double s = 0.0;
        for (int ch = 0; ch < numIn; ++ch)
            s += (double) buffer.getReadPointer (ch)[i];
        inPeak = juce::jmax (inPeak, std::abs ((float) s));
        mInScratch[(size_t) i] = s * inGain;
    }
    mInPeak.store (inPeak);

    // 2) Amplificador NAM.
    if (mModel != nullptr)
    {
        double* inPtr[1]  = { mInScratch.data() };
        double* outPtr[1] = { mOutScratch.data() };
        mModel->process (inPtr, outPtr, n);
    }
    else
    {
        for (int i = 0; i < n; ++i)   // sin modelo: passthrough
            mOutScratch[(size_t) i] = mInScratch[(size_t) i];
    }

    // 3) Normalización por loudness -> buffer float de trabajo.
    const float normGain = mNormGain.load();
    for (int i = 0; i < n; ++i)
        mWork[(size_t) i] = (float) (mOutScratch[(size_t) i] * normGain);

    // 4) Reverb (post-FX, mono).
    const float mix = mReverbMix != nullptr ? mReverbMix->load() : 0.0f;
    juce::Reverb::Parameters rp;
    rp.roomSize   = 0.6f;
    rp.damping    = 0.5f;
    rp.width      = 1.0f;
    rp.freezeMode = 0.0f;
    rp.wetLevel   = mix;
    rp.dryLevel   = 1.0f - mix;
    mReverb.setParameters (rp);
    mReverb.processMono (mWork.data(), n);

    // 5) Output gain + SEGURIDAD (NaN/Inf->0, recorte +-1 para no mandar nunca una
    //    señal descontrolada a la interfaz) + medición de pico de salida.
    float outPeak = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        float v = mWork[(size_t) i] * outGain;
        if (! std::isfinite (v)) v = 0.0f;
        v = juce::jlimit (-1.0f, 1.0f, v);
        mWork[(size_t) i] = v;
        outPeak = juce::jmax (outPeak, std::abs (v));
    }
    mOutPeak.store (outPeak);

    // 6) Volcamos el mono procesado a todas las salidas.
    for (int ch = 0; ch < numCh; ++ch)
    {
        float* out = buffer.getWritePointer (ch);
        for (int i = 0; i < n; ++i)
            out[i] = mWork[(size_t) i];
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
        const float ng = computeNormGain (*dsp);

        // Entregar al audio thread; si había uno en cola sin adoptar, liberarlo.
        nam::DSP* previouslyStaged = mStagedModel.exchange (dsp.release());
        delete previouslyStaged;
        mNormGain.store (ng);

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
