#include "ModelBrowser.h"

ModelBrowser::ModelBrowser (ModelLibrary& library) : mLibrary (library)
{
    searchBox.setTextToShowWhenEmpty ("Buscar modelos (marca, gear, nombre)...", cText2);
    searchBox.setColour (juce::TextEditor::backgroundColourId, cModule);
    searchBox.setColour (juce::TextEditor::textColourId,       cText);
    searchBox.setColour (juce::TextEditor::outlineColourId,    cBorder);
    searchBox.onTextChange = [this] { refresh(); };
    addAndMakeVisible (searchBox);

    listBox.setModel (this);
    listBox.setColour (juce::ListBox::backgroundColourId, cBg);
    listBox.setRowHeight (22);
    addAndMakeVisible (listBox);

    countLabel.setColour (juce::Label::textColourId, cText2);
    countLabel.setFont (juce::Font (juce::FontOptions (11.0f)));
    addAndMakeVisible (countLabel);

    auto styleBtn = [this] (juce::TextButton& b, bool accent)
    {
        b.setColour (juce::TextButton::buttonColourId,    accent ? cAccent : cModule);
        b.setColour (juce::TextButton::textColourOffId,   accent ? juce::Colour (0xff1a0e06) : cText);
        addAndMakeVisible (b);
    };
    styleBtn (changeFolderButton, false);
    styleBtn (loadButton,         true);
    changeFolderButton.onClick = [this] { chooseFolder(); };
    loadButton.onClick         = [this] { loadSelected(); };

    refresh();
}

ModelBrowser::~ModelBrowser()
{
    listBox.setModel (nullptr);
}

void ModelBrowser::refresh()
{
    mFiltered = mLibrary.filter (searchBox.getText());
    listBox.updateContent();
    listBox.deselectAllRows();
    countLabel.setText (juce::String (mFiltered.size()) + " / "
                          + juce::String (mLibrary.size()) + " modelos",
                        juce::dontSendNotification);
    repaint();
}

int ModelBrowser::getNumRows() { return mFiltered.size(); }

void ModelBrowser::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (row < 0 || row >= mFiltered.size())
        return;

    if (selected)
    {
        g.setColour (cAccent.withAlpha (0.25f));
        g.fillRect (0, 0, w, h);
    }
    g.setColour (selected ? cText : cText2);
    g.setFont (juce::Font (juce::FontOptions (13.0f)));
    g.drawText ("  " + mFiltered[row].relPath, 0, 0, w - 4, h, juce::Justification::centredLeft);
}

void ModelBrowser::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
{
    listBox.selectRow (row);
    loadSelected();
}

void ModelBrowser::loadSelected()
{
    const int sel = listBox.getSelectedRow();
    if (sel >= 0 && sel < mFiltered.size() && onLoad != nullptr)
        onLoad (mFiltered[sel].file);
}

void ModelBrowser::chooseFolder()
{
    chooser = std::make_unique<juce::FileChooser> ("Carpeta de biblioteca de modelos",
                                                   mLibrary.getFolder());
    chooser->launchAsync (juce::FileBrowserComponent::openMode
                          | juce::FileBrowserComponent::canSelectDirectories,
                          [this] (const juce::FileChooser& fc)
    {
        const auto dir = fc.getResult();
        if (dir == juce::File() || ! dir.isDirectory())
            return;

        mLibrary.setFolder (dir);
        if (onFolderChanged != nullptr)
            onFolderChanged (dir);
        searchBox.clear();
        refresh();
    });
}

void ModelBrowser::paint (juce::Graphics& g)
{
    g.fillAll (cBg);
}

void ModelBrowser::resized()
{
    auto r = getLocalBounds().reduced (12);

    searchBox.setBounds (r.removeFromTop (28));
    r.removeFromTop (8);

    auto bottom = r.removeFromBottom (34);
    countLabel.setBounds (bottom.removeFromLeft (140));
    loadButton.setBounds (bottom.removeFromRight (110));
    bottom.removeFromRight (8);
    changeFolderButton.setBounds (bottom.removeFromRight (150));
    r.removeFromBottom (8);

    listBox.setBounds (r);
}
