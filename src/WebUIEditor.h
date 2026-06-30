#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ModelLibrary.h"

//==============================================================================
// UI definitiva en JUCE WebView (HTML/CSS/JS), desacoplada del motor.
// Etapa 1: cadena + fotos. Etapa 2: controles cableados por relays (APVTS) +
// funciones nativas para cargar modelo/IR (reusa el ModelBrowser popup).
//==============================================================================
class WebUIEditor : public juce::AudioProcessorEditor,
                    private juce::Timer
{
public:
    explicit WebUIEditor (MusicAppAudioProcessor&);
    ~WebUIEditor() override;

    void resized() override;

private:
    using Resource = juce::WebBrowserComponent::Resource;

    std::optional<Resource> provide (const juce::String& url);
    juce::File   imageFor (const juce::File& mediaFile) const;  // <dir>/images/image_*
    juce::String stateJson() const;

    void  timerCallback() override;   // empuja {in,out,hz} a la UI ~24 Hz
    float detectPitch();              // autocorrelación sobre la entrada reciente
    std::vector<float> mPitchBuf;     // buffer de análisis del afinador

    void openModelBrowser();   // función nativa loadModel
    void chooseIR();           // función nativa loadIR
    void notifyChanged();      // emite "modelChanged" -> la UI refresca nombre+foto

    MusicAppAudioProcessor& processorRef;

    // Relays (declarados ANTES que webView: las Options los referencian).
    juce::WebSliderRelay       inputRelay  { "inputGain" };
    juce::WebSliderRelay       outputRelay { "outputGain" };
    juce::WebSliderRelay       reverbRelay { "reverbMix" };
    juce::WebToggleButtonRelay irRelay     { "irOn" };

    juce::WebBrowserComponent  webView;

    std::unique_ptr<juce::WebSliderParameterAttachment>       inAtt, outAtt, revAtt;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> irAtt;

    // Buscador de modelos (popup nativo reusado) + persistencia de la carpeta.
    ModelLibrary mLibrary;
    std::unique_ptr<juce::PropertiesFile> mSettings;
    std::unique_ptr<juce::DocumentWindow> mBrowserWindow;
    std::unique_ptr<juce::FileChooser>    mChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WebUIEditor)
};
