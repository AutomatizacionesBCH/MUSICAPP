#pragma once
#include "../FxBlock.h"
#include "NAM/dsp.h"
#include "NAM/get_dsp.h"
#include <atomic>
#include <memory>
#include <vector>
#include <cmath>
#include <filesystem>

// Drive: overdrive algorítmico (soft-clip tanh) O un PEDAL NAM cargado (captura
// real de Tube Screamer / Klon / Fuzz / boost...). Si hay pedal cargado corre la
// captura; si no, el tanh. DRIVE = empuje de entrada, LEVEL = salida. Ancla de la
// cadena (no se quita, pero se mueve y bypassea). Arranca bypasseado.
class FxDrive : public FxBlock
{
public:
    FxDrive()
    {
        add ("amount", "Drive", 0.0f, 1.0f, 0.3f, "%");
        add ("level",  "Level", 0.0f, 1.0f, 0.7f, "%");
        bypassed.store (true);
    }
    ~FxDrive() override { delete mStaged.exchange (nullptr); }

    juce::String typeId() const override { return "drive"; }
    juce::String displayName() const override { return "Drive"; }
    juce::String kind() const override { return "drive"; }
    bool removable() const override { return false; }

    bool canLoadFile() const override { return true; }
    juce::String loadedFileName() const override { return mLoadedName; }
    juce::var extra() const override
    {
        juce::DynamicObject::Ptr o = new juce::DynamicObject();
        o->setProperty ("loaded", mLoadedName);   // "" => modo algorítmico (tanh)
        return juce::var (o.get());
    }

    void prepare (double sampleRate, int blockSize) override
    {
        mSampleRate = sampleRate;
        mBlockSize  = blockSize;
        const size_t cap = (size_t) juce::jmax (blockSize, 8192);
        mIn.assign  (cap, 0.0);
        mOut.assign (cap, 0.0);
        if (mPedal != nullptr) { mPedal->Reset (sampleRate, blockSize); mPedal->prewarm(); }
    }
    void reset() override {}

    // MESSAGE thread: carga una captura de pedal .nam. Prepara fuera del audio
    // thread y la entrega por staging atómico (igual que el modelo del amp).
    void loadFile (const juce::File& file) override
    {
        if (! file.existsAsFile())
            return;
        try
        {
            auto dsp = nam::get_dsp (std::filesystem::path (file.getFullPathName().toStdString()));
            if (dsp == nullptr)
                return;
            dsp->Reset (mSampleRate, mBlockSize);
            dsp->prewarm();
            float ng = 1.0f;
            if (dsp->HasLoudness())
                ng = (float) std::pow (10.0, (-18.0 - dsp->GetLoudness()) / 20.0);
            mNormGain.store (ng);
            delete mStaged.exchange (dsp.release());   // entrega al audio thread
            mLoadedName = file.getFileNameWithoutExtension();
        }
        catch (...) {}
    }

    void processMono (float* data, int n) override
    {
        if (auto* st = mStaged.exchange (nullptr))   // adoptar el pedal recién cargado
            mPedal.reset (st);

        const double level = (double) pv (1);

        if (mPedal != nullptr)
        {
            const int    m   = juce::jmin (n, (int) mIn.size());
            const double pre = 1.0 + (double) pv (0) * 4.0;   // DRIVE empuja la entrada del pedal
            for (int i = 0; i < m; ++i)
                mIn[(size_t) i] = (double) data[i] * pre;
            double* in[1]  = { mIn.data() };
            double* out[1] = { mOut.data() };
            mPedal->process (in, out, m);
            const double ng = (double) mNormGain.load();
            for (int i = 0; i < m; ++i)
                data[i] = (float) (mOut[(size_t) i] * ng * level);
        }
        else
        {
            const double drive = 1.0 + (double) pv (0) * 30.0;   // 1..31 (tanh)
            for (int i = 0; i < n; ++i)
                data[i] = (float) (std::tanh ((double) data[i] * drive) * level);
        }
    }

private:
    double mSampleRate { 48000.0 };
    int    mBlockSize  { 512 };
    std::unique_ptr<nam::DSP> mPedal;
    std::atomic<nam::DSP*>    mStaged { nullptr };
    std::atomic<float>        mNormGain { 1.0f };
    juce::String              mLoadedName;
    std::vector<double>       mIn, mOut;
};
