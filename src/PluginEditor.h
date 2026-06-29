#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
// UI mínima del spike: gains + cargar .nam + preferencias de audio (interfaz
// externa, sample rate, buffer). Estilo "dark pro" alineado con el mockup.
// Nota: esta UI es provisional; la definitiva irá en WebView (ver CLAUDE.md).
//==============================================================================
class MusicAppAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    explicit MusicAppAudioProcessorEditor (MusicAppAudioProcessor&);
    ~MusicAppAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;      // desmuteo + refresco de medidores
    void ensureInputUnmuted();
    void chooseModel();
    void openAudioSettings();
    void styleKnob (juce::Slider&);
    void drawMeter (juce::Graphics&, juce::Rectangle<int>, float level, const juce::String& label);

    MusicAppAudioProcessor& processorRef;

    juce::Label   titleLabel, subtitleLabel, modelLabel;
    juce::Slider  inputGain, outputGain, reverbMix;
    juce::Label   inputLabel, outputLabel, reverbLabel;
    juce::TextButton loadButton  { "Cargar .nam..." };
    juce::TextButton audioButton { "Preferencias de audio..." };

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> inAtt, outAtt, revAtt;
    std::unique_ptr<juce::FileChooser> chooser;

    // Niveles suavizados de los medidores (con caída).
    float mInLevel = 0.0f, mOutLevel = 0.0f;

    // Paleta (tokens del mockup)
    const juce::Colour cBg      { 0xff0E0F11 };
    const juce::Colour cPanel   { 0xff17191C };
    const juce::Colour cModule  { 0xff1E2125 };
    const juce::Colour cBorder  { 0xff2A2E33 };
    const juce::Colour cTrack   { 0xff33373D };
    const juce::Colour cText    { 0xffE6E8EA };
    const juce::Colour cText2   { 0xff9AA0A6 };
    const juce::Colour cAccent  { 0xffFF7A29 };
    const juce::Colour cGreen   { 0xff2BD66A };
    const juce::Colour cAmber   { 0xffF4C430 };
    const juce::Colour cRed     { 0xffFF3B30 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MusicAppAudioProcessorEditor)
};
