#pragma once

#include <JuceHeader.h>
#include "ModelLibrary.h"

//==============================================================================
// Explorador: pestañas por tipo (AMP / AMP-CAB / PEDAL / IR) + búsqueda + lista
// con nombre corto del equipo y su detalle (EQ/mic/arch). No toca el DSP: avisa
// por callbacks (onLoad recibe la Entry para que el host la rutee al slot correcto).
//==============================================================================
class ModelBrowser : public juce::Component,
                     private juce::ListBoxModel
{
public:
    explicit ModelBrowser (ModelLibrary& library);
    ~ModelBrowser() override;

    std::function<void (ModelLibrary::Entry)> onLoad;          // cargar la entrada elegida
    std::function<void (juce::File)>          onFolderChanged;  // nueva carpeta de librería

    void setActiveTab (const juce::String& tab);   // "amp"|"amp-cab"|"pedal"|"ir"
    void refresh();   // reaplica pestaña + filtro

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    // ListBoxModel
    int  getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;

    void loadSelected();
    void chooseFolder();
    void styleTabs();

    ModelLibrary&                    mLibrary;
    juce::Array<ModelLibrary::Entry> mFiltered;
    juce::String                     mActiveTab { "amp" };

    juce::OwnedArray<juce::TextButton> mTabs;
    juce::StringArray mTabKeys { "amp", "amp-cab", "pedal", "ir" };
    juce::TextEditor searchBox;
    juce::ListBox    listBox;
    juce::TextButton loadButton         { "Cargar" };
    juce::TextButton changeFolderButton { "Carpeta..." };
    juce::Label      countLabel;
    std::unique_ptr<juce::FileChooser> chooser;

    // Paleta (consistente con el editor WebView)
    const juce::Colour cBg     { 0xff0E0F11 };
    const juce::Colour cModule { 0xff1E2125 };
    const juce::Colour cBorder { 0xff2A2E33 };
    const juce::Colour cText   { 0xffE6E8EA };
    const juce::Colour cText2  { 0xff9AA0A6 };
    const juce::Colour cText3  { 0xff5A6068 };
    const juce::Colour cAccent { 0xffFF7A29 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModelBrowser)
};
