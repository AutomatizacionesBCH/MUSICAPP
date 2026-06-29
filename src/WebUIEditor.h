#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
// UI definitiva (Ciclo 1): cadena de señal en JUCE WebView (HTML/CSS/JS),
// desacoplada del motor. Etapa 1 = visual + fotos vía resource provider.
//==============================================================================
class WebUIEditor : public juce::AudioProcessorEditor
{
public:
    explicit WebUIEditor (MusicAppAudioProcessor&);
    ~WebUIEditor() override = default;

    void resized() override;

private:
    using Resource = juce::WebBrowserComponent::Resource;

    std::optional<Resource> provide (const juce::String& url);
    juce::File   imageFor (const juce::File& mediaFile) const;  // <dir>/images/image_*
    juce::String stateJson() const;

    MusicAppAudioProcessor&   processorRef;
    juce::WebBrowserComponent webView;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WebUIEditor)
};
