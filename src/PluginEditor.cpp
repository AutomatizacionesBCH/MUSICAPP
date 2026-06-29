#include "PluginEditor.h"
#include "ModelBrowser.h"

#if JucePlugin_Build_Standalone
 #include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#endif

namespace
{
    // Ventana popup que hospeda el ModelBrowser; al cerrar avisa por callback para
    // que el editor libere su unique_ptr (el browser se destruye con la ventana).
    class BrowserWindow : public juce::DocumentWindow
    {
    public:
        BrowserWindow (juce::Component* content, std::function<void()> onClose)
            : juce::DocumentWindow ("Biblioteca de modelos",
                                    juce::Colour (0xff0E0F11),
                                    juce::DocumentWindow::closeButton),
              mOnClose (std::move (onClose))
        {
            setUsingNativeTitleBar (true);
            setContentOwned (content, true);
            setResizable (true, false);
            centreWithSize (560, 460);
            setVisible (true);
        }

        void closeButtonPressed() override { if (mOnClose) mOnClose(); }

    private:
        std::function<void()> mOnClose;
    };
}

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
    styleKnob (reverbMix);
    reverbMix.setTextValueSuffix ("");           // es mezcla 0..1, no dB
    addAndMakeVisible (inputGain);
    addAndMakeVisible (outputGain);
    addAndMakeVisible (reverbMix);

    inAtt  = std::make_unique<SliderAttachment> (processorRef.apvts, "inputGain",  inputGain);
    outAtt = std::make_unique<SliderAttachment> (processorRef.apvts, "outputGain", outputGain);
    revAtt = std::make_unique<SliderAttachment> (processorRef.apvts, "reverbMix",  reverbMix);

    auto setupCaption = [this] (juce::Label& l, const juce::String& t)
    {
        l.setText (t, juce::dontSendNotification);
        l.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
        l.setColour (juce::Label::textColourId, cText2);
        l.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (l);
    };
    setupCaption (inputLabel,  "INPUT");
    setupCaption (reverbLabel, "REVERB");
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

    // Cabinet IR: botón de carga + toggle ON/OFF + label del IR cargado.
    styleButton (loadIrButton, false);
    loadIrButton.onClick = [this] { chooseIR(); };

    irToggle.setColour (juce::ToggleButton::textColourId,         cText2);
    irToggle.setColour (juce::ToggleButton::tickColourId,         cAccent);
    irToggle.setColour (juce::ToggleButton::tickDisabledColourId, cTrack);
    addAndMakeVisible (irToggle);
    irAtt = std::make_unique<ButtonAttachment> (processorRef.apvts, "irOn", irToggle);

    irLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    irLabel.setColour (juce::Label::textColourId,       cText2);
    irLabel.setColour (juce::Label::backgroundColourId, cModule);
    irLabel.setJustificationType (juce::Justification::centredLeft);
    irLabel.setText ("  IR: " + (processorRef.getLoadedIRName().isNotEmpty()
                                   ? processorRef.getLoadedIRName()
                                   : juce::String ("(ninguno)")),
                     juce::dontSendNotification);
    addAndMakeVisible (irLabel);

    // Explorador de modelos: botón + librería con carpeta persistida.
    styleButton (modelsButton, false);
    modelsButton.onClick = [this] { openModelBrowser(); };

    juce::PropertiesFile::Options opts;
    opts.applicationName     = "Music App";
    opts.filenameSuffix      = "settings";
    opts.folderName          = "Music App";
    opts.osxLibrarySubFolder = "Application Support";
    mSettings = std::make_unique<juce::PropertiesFile> (opts);

    juce::File libFolder (mSettings->getValue ("modelLibraryFolder", {}));
    if (! libFolder.isDirectory())
        libFolder = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                      .getChildFile ("Music App").getChildFile ("models");
    mLibrary.setFolder (libFolder);

    setSize (520, 352);

    ensureInputUnmuted();   // amp sim: la entrada es la guitarra, no hay feedback real
    startTimerHz (30);      // desmuteo + refresco de medidores
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

    // Caída suave de los medidores (pico instantáneo con release).
    const float decay = 0.80f;
    mInLevel  = juce::jmax (processorRef.getInPeak(),  mInLevel  * decay);
    mOutLevel = juce::jmax (processorRef.getOutPeak(), mOutLevel * decay);
    repaint();
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
    // Directorio inicial multiplataforma: ~/Documents/Music App/models si existe,
    // si no la carpeta de música del usuario (sin rutas específicas de SO).
    auto startDir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                      .getChildFile ("Music App").getChildFile ("models");
    if (! startDir.isDirectory())
        startDir = juce::File::getSpecialLocation (juce::File::userMusicDirectory);

    chooser = std::make_unique<juce::FileChooser> (
        "Elige un modelo NAM (.nam)",
        startDir,
        "*.nam");

    const auto flags = juce::FileBrowserComponent::openMode
                     | juce::FileBrowserComponent::canSelectFiles;

    chooser->launchAsync (flags, [this] (const juce::FileChooser& fc)
    {
        const auto file = fc.getResult();
        if (file == juce::File())
            return;

        loadModelFile (file);
    });
}

void MusicAppAudioProcessorEditor::chooseIR()
{
    // Directorio inicial multiplataforma: ~/Documents/Music App/irs si existe.
    auto startDir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                      .getChildFile ("Music App").getChildFile ("irs");
    if (! startDir.isDirectory())
        startDir = juce::File::getSpecialLocation (juce::File::userMusicDirectory);

    chooser = std::make_unique<juce::FileChooser> ("Elige un Cabinet IR (.wav)", startDir, "*.wav");

    const auto flags = juce::FileBrowserComponent::openMode
                     | juce::FileBrowserComponent::canSelectFiles;

    chooser->launchAsync (flags, [this] (const juce::FileChooser& fc)
    {
        const auto file = fc.getResult();
        if (file == juce::File())
            return;

        if (processorRef.loadIR (file))
            irLabel.setText ("  IR: " + processorRef.getLoadedIRName(), juce::dontSendNotification);
        else
            irLabel.setText ("  Error al cargar el IR", juce::dontSendNotification);
    });
}

void MusicAppAudioProcessorEditor::loadModelFile (const juce::File& file)
{
    if (processorRef.loadNamModel (file))
        modelLabel.setText ("  Modelo: " + processorRef.getLoadedModelName(),
                            juce::dontSendNotification);
    else
        modelLabel.setText ("  Error al cargar el modelo", juce::dontSendNotification);
}

void MusicAppAudioProcessorEditor::openModelBrowser()
{
    if (mBrowserWindow != nullptr) { mBrowserWindow->toFront (true); return; }

    auto* browser = new ModelBrowser (mLibrary);
    browser->setSize (560, 460);
    browser->onLoad = [this] (juce::File f) { loadModelFile (f); };
    browser->onFolderChanged = [this] (juce::File dir)
    {
        if (mSettings != nullptr)
        {
            mSettings->setValue ("modelLibraryFolder", dir.getFullPathName());
            mSettings->saveIfNeeded();
        }
    };
    // Al cerrar: diferir la destrucción (no borrar la ventana dentro de su propio
    // closeButtonPressed) y usar SafePointer por si el editor ya no existe.
    juce::Component::SafePointer<MusicAppAudioProcessorEditor> safe (this);
    mBrowserWindow.reset (new BrowserWindow (browser, [safe]
    {
        juce::MessageManager::callAsync ([safe]
        {
            if (auto* ed = safe.getComponent())
                ed->mBrowserWindow.reset();
        });
    }));
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

    // Barra inferior con medidores In/Out (comportamiento AIDA-X: verde->ámbar->rojo).
    auto bottom = getLocalBounds().removeFromBottom (56);
    g.setColour (cPanel);
    g.fillRect (bottom);
    g.setColour (cBorder);
    g.drawLine ((float) bottom.getX(), (float) bottom.getY(),
                (float) bottom.getRight(), (float) bottom.getY(), 1.0f);

    auto bar = bottom.reduced (14, 11);
    auto inRow = bar.removeFromTop (bar.getHeight() / 2 - 2);
    bar.removeFromTop (4);
    drawMeter (g, inRow, mInLevel,  "IN");
    drawMeter (g, bar,   mOutLevel, "OUT");
}

void MusicAppAudioProcessorEditor::drawMeter (juce::Graphics& g, juce::Rectangle<int> r,
                                              float level, const juce::String& label)
{
    g.setColour (cText2);
    g.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold)));
    g.drawText (label, r.removeFromLeft (30), juce::Justification::centredLeft);

    g.setColour (cBg);
    g.fillRoundedRectangle (r.toFloat(), 3.0f);
    g.setColour (cBorder);
    g.drawRoundedRectangle (r.toFloat(), 3.0f, 1.0f);

    const float lvl = juce::jlimit (0.0f, 1.0f, level);
    auto fill = r.reduced (1).toFloat();
    fill.setWidth (fill.getWidth() * lvl);
    const juce::Colour c = lvl > 0.95f ? cRed : (lvl > 0.70f ? cAmber : cGreen);
    g.setColour (c);
    g.fillRoundedRectangle (fill, 2.0f);
}

void MusicAppAudioProcessorEditor::resized()
{
    auto r = getLocalBounds().reduced (16);

    auto header = r.removeFromTop (44);
    titleLabel.setBounds (header.removeFromTop (24));
    subtitleLabel.setBounds (header);

    r.removeFromTop (8);
    auto modelRow = r.removeFromTop (28);
    modelsButton.setBounds (modelRow.removeFromRight (96));
    modelRow.removeFromRight (8);
    modelLabel.setBounds (modelRow);

    // Fila Cabinet IR: [Cargar IR...] [toggle IR] [label del IR]
    r.removeFromTop (6);
    auto irRow = r.removeFromTop (28);
    loadIrButton.setBounds (irRow.removeFromLeft (130));
    irRow.removeFromLeft (10);
    irToggle.setBounds (irRow.removeFromLeft (60));
    irRow.removeFromLeft (10);
    irLabel.setBounds (irRow);

    r.removeFromBottom (56);          // espacio de la barra inferior
    r.removeFromBottom (8);

    auto buttons = r.removeFromBottom (34);
    loadButton.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2).reduced (4, 0));
    audioButton.setBounds (buttons.reduced (4, 0));

    r.removeFromTop (8);
    auto knobs = r;
    const int kw = knobs.getWidth() / 3;

    auto kIn = knobs.removeFromLeft (kw);
    inputLabel.setBounds (kIn.removeFromTop (16));
    inputGain.setBounds (kIn.reduced (8, 4));

    auto kRev = knobs.removeFromLeft (kw);
    reverbLabel.setBounds (kRev.removeFromTop (16));
    reverbMix.setBounds (kRev.reduced (8, 4));

    auto kOut = knobs;
    outputLabel.setBounds (kOut.removeFromTop (16));
    outputGain.setBounds (kOut.reduced (8, 4));
}
