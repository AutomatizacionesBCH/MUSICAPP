#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
// UI mínima del spike: gains + cargar .nam + preferencias de audio (interfaz
// externa, sample rate, buffer). Estilo "dark pro" alineado con el mockup.
// Nota: esta UI es provisional; la definitiva irá en WebView (ver CLAUDE.md).
//==============================================================================
class MusicAppAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit MusicAppAudioProcessorEditor (MusicAppAudioProcessor&);
    ~MusicAppAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void chooseModel();
    void openAudioSettings();
    void styleKnob (juce::Slider&);

    MusicAppAudioProcessor& processorRef;

    juce::Label   titleLabel, subtitleLabel, modelLabel;
    juce::Slider  inputGain, outputGain;
    juce::Label   inputLabel, outputLabel;
    juce::TextButton loadButton  { "Cargar .nam…" };
    juce::TextButton audioButton { "Preferencias de audio…" };

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> inAtt, outAtt;
    std::unique_ptr<juce::FileChooser> chooser;

    // Paleta (tokens del mockup)
    const juce::Colour cBg      { 0xff0E0F11 };
    const juce::Colour cPanel   { 0xff17191C };
    const juce::Colour cModule  { 0xff1E2125 };
    const juce::Colour cBorder  { 0xff2A2E33 };
    const juce::Colour cTrack   { 0xff33373D };
    const juce::Colour cText    { 0xffE6E8EA };
    const juce::Colour cText2   { 0xff9AA0A6 };
    const juce::Colour cAccent  { 0xffFF7A29 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MusicAppAudioProcessorEditor)
};
