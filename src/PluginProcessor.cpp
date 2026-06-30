#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "WebUIEditor.h"
#include "fx/FxFactory.h"
#include "fx/blocks/FxDrive.h"
#include "fx/blocks/FxAmpRef.h"
#include "fx/blocks/FxCabRef.h"

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

    // La cadena crea Drive/Amp/Cab (necesitan el host=this) + delega los efectos
    // a FxFactory. Toda la cadena es una sola lista reordenable.
    mFxChain.setFactory ([this] (const juce::String& t) -> std::unique_ptr<FxBlock>
    {
        if (t == "drive") return std::make_unique<FxDrive>();
        if (t == "amp")   return std::make_unique<FxAmpRef> (this);
        if (t == "cab")   return std::make_unique<FxCabRef> (this);
        return FxFactory::create (t);
    });

    // Cadena por defecto: Drive(off) -> Amp -> Cab(off) -> Reverb.
    mFxChain.add ("drive");
    mFxChain.add ("amp");
    mFxChain.add ("cab");
    mFxChain.add ("reverb");

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

    // Cabinet IR: mono (1 canal). loadImpulseResponse resamplea al sampleRate aquí.
    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 1 };
    mConvolution.prepare (spec);
    mConvolution.reset();

    mFxChain.prepare (sampleRate, samplesPerBlock);   // rack flexible post-cab

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

    const int n = juce::jmin (numSamples, (int) mWork.size());

    // 1) Suma de TODAS las entradas a mono float * IN gain (la guitarra puede estar
    //    en input 1/L o 2/R) + pico de entrada + entrada cruda al ring (afinador).
    const int numIn = juce::jmin (getTotalNumInputChannels(), numCh);
    float inPeak = 0.0f;
    int ringW = mInWrite.load();
    for (int i = 0; i < n; ++i)
    {
        double s = 0.0;
        for (int ch = 0; ch < numIn; ++ch)
            s += (double) buffer.getReadPointer (ch)[i];
        inPeak = juce::jmax (inPeak, std::abs ((float) s));
        mWork[(size_t) i] = (float) (s * inGain);
        mInRing[(size_t) (ringW % kInRingSize)] = (float) s;
        ++ringW;
    }
    mInWrite.store (ringW);
    mInPeak.store (inPeak);

    // 2) Cadena flexible COMPLETA: Drive -> Amp(NAM) -> Cab(IR) -> efectos, en el
    //    orden que el usuario haya puesto (los bloques ancla llaman hostProcess*).
    //    Lock-free (ScopedTryLock) -> nunca bloquea el audio thread.
    mFxChain.processMono (mWork.data(), n);

    // 3) OUT gain + SEGURIDAD (NaN/Inf->0, recorte +-1) + pico de salida.
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

    // 4) Volcamos el mono procesado a todas las salidas.
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
// FxHost: los bloques ancla Amp/Cab invocan estos. Corren en el audio thread.
void MusicAppAudioProcessor::hostProcessAmp (float* data, int n)
{
    if (mModel == nullptr)
        return;   // sin modelo: passthrough
    const int m = juce::jmin (n, (int) mInScratch.size());
    for (int i = 0; i < m; ++i)
        mInScratch[(size_t) i] = (double) data[i];
    double* in[1]  = { mInScratch.data() };
    double* out[1] = { mOutScratch.data() };
    mModel->process (in, out, m);
    const float ng = mNormGain.load();
    for (int i = 0; i < m; ++i)
        data[i] = (float) (mOutScratch[(size_t) i] * ng);
}

void MusicAppAudioProcessor::hostProcessCab (float* data, int n)
{
    float* chans[1] = { data };
    juce::dsp::AudioBlock<float> block (chans, 1, (size_t) n);
    juce::dsp::ProcessContextReplacing<float> ctx (block);
    mConvolution.process (ctx);
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
    auto state = apvts.copyState();
    state.appendChild (mFxChain.toValueTree(), nullptr);   // hijo "fxchain"
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void MusicAppAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto tree = juce::ValueTree::fromXml (*xml);
        if (auto fxTree = tree.getChildWithName ("fxchain"); fxTree.isValid())
        {
            mFxChain.fromValueTree (fxTree);
            tree.removeChild (fxTree, nullptr);   // que el APVTS no vea el hijo extra
        }
        apvts.replaceState (tree);
    }
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
