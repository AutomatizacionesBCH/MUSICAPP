#include "PluginEditor.h"

#if JucePlugin_Build_Standalone
 #include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#endif

//==============================================================================
MusicAppAudioProcessorEditor::MusicAppAudioProcessorEditor (MusicAppAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    titleLabel.setText ("MUSIC APP", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::FontOptions (20.0f, juce::Font::bold)));
    titleLabel.setColour (juce::Label::textColourId, cAccent);
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText ("Spike - NAM en vivo", juce::dontSendNotification);
    subtitleLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    subtitleLabel.setColour (juce::Label::textColourId, cText2);
    addAndMakeVisible (subtitleLabel);

    modelLabel.setFont (juce::Font (juce::FontOptions (13.0f)));
    modelLabel.setColour (juce::Label::textColourId, cText);
    modelLabel.setColour (juce::Label::backgroundColourId, cModule);
    modelLabel.setJustificationType (juce::Justification::centredLeft);
    modelLabel.setText ("  Modelo: " + (processorRef.getLoadedModelName().isNotEmpty()
                                          ? processorRef.getLoadedModelName()
                                          : juce::String ("(ninguno - passthrough)")),
                        juce::dontSendNotification);
    addAndMakeVisible (modelLabel);

    styleKnob (inputGain);
    styleKnob (outputGain);
    addAndMakeVisible (inputGain);
    addAndMakeVisible (outputGain);

    inAtt  = std::make_unique<SliderAttachment> (processorRef.apvts, "inputGain",  inputGain);
    outAtt = std::make_unique<SliderAttachment> (processorRef.apvts, "outputGain", outputGain);

    auto setupCaption = [this] (juce::Label& l, const juce::String& t)
    {
        l.setText (t, juce::dontSendNotification);
        l.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
        l.setColour (juce::Label::textColourId, cText2);
        l.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (l);
    };
    setupCaption (inputLabel,  "INPUT");
    setupCaption (outputLabel, "OUTPUT");

    auto styleButton = [this] (juce::TextButton& b, bool accent)
    {
        b.setColour (juce::TextButton::buttonColourId, accent ? cAccent : cModule);
        b.setColour (juce::TextButton::textColourOffId, accent ? juce::Colour (0xff1a0e06) : cText);
        addAndMakeVisible (b);
    };
    styleButton (loadButton,  false);
    styleButton (audioButton, true);

    loadButton.onClick  = [this] { chooseModel(); };
    audioButton.onClick = [this] { openAudioSettings(); };

    setSize (520, 300);

    ensureInputUnmuted();   // amp sim: la entrada es la guitarra, no hay feedback real
    startTimer (1000);      // y la mantenemos desmuteada por si se re-silencia al reconfigurar
}

void MusicAppAudioProcessorEditor::ensureInputUnmuted()
{
   #if JucePlugin_Build_Standalone
    if (auto* holder = juce::StandalonePluginHolder::getInstance())
        if ((bool) holder->getMuteInputValue().getValue())
            holder->getMuteInputValue().setValue (false);
   #endif
}

void MusicAppAudioProcessorEditor::timerCallback()
{
    ensureInputUnmuted();
}

//==============================================================================
void MusicAppAudioProcessorEditor::styleKnob (juce::Slider& s)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    s.setColour (juce::Slider::rotarySliderFillColourId,    cAccent);
    s.setColour (juce::Slider::rotarySliderOutlineColourId, cTrack);
    s.setColour (juce::Slider::thumbColourId,               cText);
    s.setColour (juce::Slider::textBoxTextColourId,         cText2);
    s.setColour (juce::Slider::textBoxOutlineColourId,      cBorder);
    s.setTextValueSuffix (" dB");
}

//==============================================================================
void MusicAppAudioProcessorEditor::chooseModel()
{
    chooser = std::make_unique<juce::FileChooser> (
        "Elige un modelo NAM (.nam)",
        juce::File ("/Users/automatizacionesbch/Desktop/AGENTES IA/Music App/libs/"
                    "NeuralAmpModelerCore/example_models"),
        "*.nam");

    const auto flags = juce::FileBrowserComponent::openMode
                     | juce::FileBrowserComponent::canSelectFiles;

    chooser->launchAsync (flags, [this] (const juce::FileChooser& fc)
    {
        const auto file = fc.getResult();
        if (file == juce::File())
            return;

        if (processorRef.loadNamModel (file))
            modelLabel.setText ("  Modelo: " + processorRef.getLoadedModelName(),
                                juce::dontSendNotification);
        else
            modelLabel.setText ("  Error al cargar el modelo", juce::dontSendNotification);
    });
}

void MusicAppAudioProcessorEditor::openAudioSettings()
{
   #if JucePlugin_Build_Standalone
    if (auto* holder = juce::StandalonePluginHolder::getInstance())
    {
        holder->showAudioSettingsDialog();
        return;
    }
   #endif
    juce::NativeMessageBox::showMessageBoxAsync (
        juce::MessageBoxIconType::InfoIcon, "Preferencias de audio",
        "Abre los ajustes de audio desde el menu de la ventana.");
}

//==============================================================================
void MusicAppAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (cBg);

    // Barra inferior tipo "IO" (decorativa por ahora)
    auto bottom = getLocalBounds().removeFromBottom (56);
    g.setColour (cPanel);
    g.fillRect (bottom);
    g.setColour (cBorder);
    g.drawLine ((float) bottom.getX(), (float) bottom.getY(),
                (float) bottom.getRight(), (float) bottom.getY(), 1.0f);

    g.setColour (cText2);
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.drawText ("Conecta tu interfaz y abre \"Preferencias de audio\" para elegir entrada, sample rate y buffer.",
                bottom.reduced (14, 0), juce::Justification::centredLeft, true);
}

void MusicAppAudioProcessorEditor::resized()
{
    auto r = getLocalBounds().reduced (16);

    auto header = r.removeFromTop (44);
    titleLabel.setBounds (header.removeFromTop (24));
    subtitleLabel.setBounds (header);

    r.removeFromTop (8);
    modelLabel.setBounds (r.removeFromTop (28));

    r.removeFromBottom (56);          // espacio de la barra inferior
    r.removeFromBottom (8);

    auto buttons = r.removeFromBottom (34);
    loadButton.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2).reduced (4, 0));
    audioButton.setBounds (buttons.reduced (4, 0));

    r.removeFromTop (8);
    auto knobs = r;
    auto kw = knobs.getWidth() / 2;

    auto kIn = knobs.removeFromLeft (kw);
    inputLabel.setBounds (kIn.removeFromTop (16));
    inputGain.setBounds (kIn.reduced (10, 4));

    auto kOut = knobs;
    outputLabel.setBounds (kOut.removeFromTop (16));
    outputGain.setBounds (kOut.reduced (10, 4));
}
