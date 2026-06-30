#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "WebUIEditor.h"

#include <cmath>
#include <filesystem>

//==============================================================================
namespace
{
    // Carpeta de modelos por defecto, MULTIPLATAFORMA (Windows y macOS):
    //   ~/Documents/Music App/models
    // Coloca ahí uno o varios .nam para que la app cargue uno al arrancar.
    juce::File defaultModelsDir()
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                 .getChildFile ("Music App")
                 .getChildFile ("models");
    }

    // Resuelve el modelo por defecto SIN rutas específicas de usuario/SO:
    //   1) variable de entorno MUSICAPP_DEFAULT_MODEL (ruta a un .nam), o
    //   2) el primer .nam dentro de defaultModelsDir(), o
    //   3) ninguno -> la app arranca en passthrough hasta cargar uno desde la UI.
    // OJO: los example_models de NAM Core son fixtures sin calibrar (no usarlos).
    // En Fase 2 esto lo reemplaza el buscador de tone3000.
    juce::File resolveDefaultModel()
    {
        const auto env = juce::SystemStats::getEnvironmentVariable ("MUSICAPP_DEFAULT_MODEL", {});
        if (env.isNotEmpty())
        {
            const juce::File f (env);
            if (f.existsAsFile())
                return f;
        }

        const auto dir = defaultModelsDir();
        if (dir.isDirectory())
        {
            const auto nams = dir.findChildFiles (juce::File::findFiles, false, "*.nam");
            if (! nams.isEmpty())
                return nams.getReference (0);
        }
        return {};
    }

    // Carpeta de IRs por defecto, multiplataforma: ~/Documents/Music App/irs
    juce::File defaultIRsDir()
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                 .getChildFile ("Music App")
                 .getChildFile ("irs");
    }

    // Resuelve un IR por defecto sin rutas de usuario/SO:
    //   1) env var MUSICAPP_DEFAULT_IR (ruta a un .wav), o
    //   2) el primer .wav en defaultIRsDir(), o
    //   3) ninguno. (Se carga pero arranca BYPASSEADO: el modelo por defecto es
    //      amp-cab y un IR encima sería doble-cab.)
    juce::File resolveDefaultIR()
    {
        const auto env = juce::SystemStats::getEnvironmentVariable ("MUSICAPP_DEFAULT_IR", {});
        if (env.isNotEmpty())
        {
            const juce::File f (env);
            if (f.existsAsFile())
                return f;
        }

        const auto dir = defaultIRsDir();
        if (dir.isDirectory())
        {
            const auto irs = dir.findChildFiles (juce::File::findFiles, false, "*.wav");
            if (! irs.isEmpty())
                return irs.getReference (0);
        }
        return {};
    }
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
    mIrOn         = apvts.getRawParameterValue ("irOn");
    mDriveOn      = apvts.getRawParameterValue ("driveOn");
    mDriveAmount  = apvts.getRawParameterValue ("driveAmount");
    mDriveLevel   = apvts.getRawParameterValue ("driveLevel");

    // Carga inicial del modelo por defecto (el audio aún no corre, así que es
    // seguro asignar mModel directamente; el Reset se hará en prepareToPlay).
    const juce::File defaultModel = resolveDefaultModel();
    if (defaultModel.existsAsFile())
    {
        try
        {
            mModel = nam::get_dsp (std::filesystem::path (defaultModel.getFullPathName().toStdString()));
            if (mModel != nullptr)
            {
                mLoadedModelName = defaultModel.getFileName();
                mLoadedModelFile = defaultModel;
                mNormGain.store (computeNormGain (*mModel));
            }
        }
        catch (const std::exception& e)
        {
            juce::Logger::writeToLog (juce::String ("NAM load (default) falló: ") + e.what());
        }
    }

    // IR por defecto (si hay uno en ~/Documents/Music App/irs). Arranca BYPASSEADO
    // (irOn = false) para no doble-cab con el modelo amp-cab por defecto.
    const juce::File defaultIR = resolveDefaultIR();
    if (defaultIR.existsAsFile())
        loadIR (defaultIR);
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

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "irOn", 1 }, "Cabinet IR", false));

    // Drive (pre-FX, overdrive antes del NAM)
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "driveOn", 1 }, "Drive", false));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "driveAmount", 1 }, "Drive Amount",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.3f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "driveLevel", 1 }, "Drive Level",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.7f));

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

    // Cabinet IR: mono (1 canal). loadImpulseResponse resamplea al sampleRate aquí.
    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 1 };
    mConvolution.prepare (spec);
    mConvolution.reset();

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
    int ringW = mInWrite.load();
    for (int i = 0; i < n; ++i)
    {
        double s = 0.0;
        for (int ch = 0; ch < numIn; ++ch)
            s += (double) buffer.getReadPointer (ch)[i];
        inPeak = juce::jmax (inPeak, std::abs ((float) s));
        mInScratch[(size_t) i] = s * inGain;
        mInRing[(size_t) (ringW % kInRingSize)] = (float) s;   // entrada cruda para el afinador
        ++ringW;
    }
    mInWrite.store (ringW);
    mInPeak.store (inPeak);

    // 1.5) Drive (pre-FX): overdrive por soft-clip (tanh) antes del NAM.
    if (mDriveOn != nullptr && mDriveOn->load() > 0.5f)
    {
        const double driveGain = 1.0 + (double) mDriveAmount->load() * 30.0;   // 1..31
        const double level     = (double) mDriveLevel->load();
        for (int i = 0; i < n; ++i)
            mInScratch[(size_t) i] = std::tanh (mInScratch[(size_t) i] * driveGain) * level;
    }

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

    // 3.5) Cabinet IR (convolución, post-NAM). Bypass real: si está OFF no se procesa.
    if (mIrOn != nullptr && mIrOn->load() > 0.5f)
    {
        float* irChans[1] = { mWork.data() };
        juce::dsp::AudioBlock<float> irBlock (irChans, 1, (size_t) n);
        juce::dsp::ProcessContextReplacing<float> irCtx (irBlock);
        mConvolution.process (irCtx);
    }

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
        mLoadedModelFile = file;
        return true;
    }
    catch (const std::exception& e)
    {
        juce::Logger::writeToLog (juce::String ("NAM load falló: ") + e.what());
        return false;
    }
}

//==============================================================================
bool MusicAppAudioProcessor::loadIR (const juce::File& file)
{
    if (! file.existsAsFile())
        return false;

    // loadImpulseResponse carga en un hilo de fondo y hace el swap del IR de forma
    // thread-safe (mono, normalizado, resampleado al sampleRate preparado).
    mConvolution.loadImpulseResponse (file,
                                      juce::dsp::Convolution::Stereo::no,
                                      juce::dsp::Convolution::Trim::no,
                                      (size_t) 0,
                                      juce::dsp::Convolution::Normalise::yes);
    mIrLoadedName = file.getFileName();
    mIrLoadedFile = file;
    return true;
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

void MusicAppAudioProcessor::getRecentInput (float* dest, int numSamples) const
{
    const int w = mInWrite.load();
    for (int i = 0; i < numSamples; ++i)
    {
        int idx = ((w - numSamples + i) % kInRingSize + kInRingSize) % kInRingSize;
        dest[i] = mInRing[(size_t) idx];
    }
}

juce::AudioProcessorEditor* MusicAppAudioProcessor::createEditor()
{
    return new WebUIEditor (*this);   // UI definitiva en WebView (Ciclo 1)
}

//==============================================================================
// Punto de entrada del plugin (requerido por JUCE)
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MusicAppAudioProcessor();
}
