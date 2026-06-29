#pragma once

#include <JuceHeader.h>
#include "ModelLibrary.h"

//==============================================================================
// UI del explorador: campo de búsqueda + lista filtrable + botones. No toca el
// DSP: avisa por callbacks (onLoad / onFolderChanged). Estilo dark del editor.
//==============================================================================
class ModelBrowser : public juce::Component,
                     private juce::ListBoxModel
{
public:
    explicit ModelBrowser (ModelLibrary& library);
    ~ModelBrowser() override;

    std::function<void (juce::File)> onLoad;          // cargar el modelo elegido
    std::function<void (juce::File)> onFolderChanged;  // nueva carpeta de librería

    void refresh();   // reaplica el filtro y actualiza lista + contador

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    // ListBoxModel
    int  getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;

    void loadSelected();
    void chooseFolder();

    ModelLibrary&                  mLibrary;
    juce::Array<ModelLibrary::Entry> mFiltered;

    juce::TextEditor searchBox;
    juce::ListBox    listBox;
    juce::TextButton loadButton         { "Cargar" };
    juce::TextButton changeFolderButton { "Cambiar carpeta..." };
    juce::Label      countLabel;
    std::unique_ptr<juce::FileChooser> chooser;

    // Paleta (consistente con PluginEditor)
    const juce::Colour cBg     { 0xff0E0F11 };
    const juce::Colour cModule { 0xff1E2125 };
    const juce::Colour cBorder { 0xff2A2E33 };
    const juce::Colour cText   { 0xffE6E8EA };
    const juce::Colour cText2  { 0xff9AA0A6 };
    const juce::Colour cAccent { 0xffFF7A29 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModelBrowser)
};
