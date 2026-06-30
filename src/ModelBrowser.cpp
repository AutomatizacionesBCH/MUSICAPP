#include "ModelBrowser.h"

ModelBrowser::ModelBrowser (ModelLibrary& library) : mLibrary (library)
{
    // Pestañas por tipo
    const char* labels[] = { "AMP", "AMP-CAB", "PEDAL", "IR" };
    for (int i = 0; i < mTabKeys.size(); ++i)
    {
        auto* b = mTabs.add (new juce::TextButton (labels[i]));
        const juce::String key = mTabKeys[i];
        b->onClick = [this, key] { setActiveTab (key); };
        addAndMakeVisible (b);
    }

    searchBox.setTextToShowWhenEmpty ("Buscar por equipo, EQ, mic...", cText2);
    searchBox.setColour (juce::TextEditor::backgroundColourId, cModule);
    searchBox.setColour (juce::TextEditor::textColourId,       cText);
    searchBox.setColour (juce::TextEditor::outlineColourId,    cBorder);
    searchBox.onTextChange = [this] { refresh(); };
    addAndMakeVisible (searchBox);

    listBox.setModel (this);
    listBox.setColour (juce::ListBox::backgroundColourId, cBg);
    listBox.setRowHeight (42);
    addAndMakeVisible (listBox);

    countLabel.setColour (juce::Label::textColourId, cText2);
    countLabel.setFont (juce::Font (juce::FontOptions (11.0f)));
    addAndMakeVisible (countLabel);

    auto styleBtn = [this] (juce::TextButton& b, bool accent)
    {
        b.setColour (juce::TextButton::buttonColourId,  accent ? cAccent : cModule);
        b.setColour (juce::TextButton::textColourOffId, accent ? juce::Colour (0xff1a0e06) : cText);
        addAndMakeVisible (b);
    };
    styleBtn (changeFolderButton, false);
    styleBtn (loadButton,         true);
    changeFolderButton.onClick = [this] { chooseFolder(); };
    loadButton.onClick         = [this] { loadSelected(); };

    styleTabs();
    refresh();
}

ModelBrowser::~ModelBrowser()
{
    listBox.setModel (nullptr);
}

void ModelBrowser::styleTabs()
{
    for (int i = 0; i < mTabs.size(); ++i)
    {
        const bool on = (mTabKeys[i] == mActiveTab);
        mTabs[i]->setColour (juce::TextButton::buttonColourId,  on ? cAccent : cModule);
        mTabs[i]->setColour (juce::TextButton::textColourOffId, on ? juce::Colour (0xff1a0e06) : cText2);
    }
}

void ModelBrowser::setActiveTab (const juce::String& tab)
{
    if (mActiveTab == tab)
        return;
    mActiveTab = tab;
    styleTabs();
    refresh();
}

void ModelBrowser::refresh()
{
    mFiltered = mLibrary.filter (mActiveTab, searchBox.getText());
    listBox.updateContent();
    listBox.deselectAllRows();
    countLabel.setText (juce::String (mFiltered.size()) + " de "
                          + juce::String (mLibrary.countForTab (mActiveTab)),
                        juce::dontSendNotification);
    repaint();
}

int ModelBrowser::getNumRows() { return mFiltered.size(); }

void ModelBrowser::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (row < 0 || row >= mFiltered.size())
        return;
    const auto& e = mFiltered[row];

    if (selected)
    {
        g.setColour (cAccent.withAlpha (0.18f));
        g.fillRect (0, 0, w, h);
        g.setColour (cAccent);
        g.fillRect (0, 0, 3, h);
    }
    // Badge de arquitectura (A1 / A2 / CUSTOM) a la derecha; reserva su espacio.
    int textRight = w - 16;
    if (e.arch.isNotEmpty())
    {
        const juce::String label = e.arch.toUpperCase();
        const int bw = (label.length() <= 2) ? 28 : 54;
        const int bh = 16;
        const int bx = w - bw - 10;
        const int by = (h - bh) / 2;

        juce::Colour bg, fg;
        if (e.arch == "a2")          { bg = cAccent;                                    fg = juce::Colour (0xff1a0e06); }
        else if (e.arch == "custom") { bg = juce::Colour (0xff8a5ad2).withAlpha (0.22f); fg = juce::Colour (0xffd8b4fe); }
        else                         { bg = juce::Colour (0xff5684c9).withAlpha (0.20f); fg = juce::Colour (0xff9fc7ff); }

        g.setColour (bg);
        g.fillRoundedRectangle ((float) bx, (float) by, (float) bw, (float) bh, 4.0f);
        g.setColour (fg);
        g.setFont (juce::Font (juce::FontOptions (9.5f, juce::Font::bold)));
        g.drawText (label, bx, by, bw, bh, juce::Justification::centred, false);
        textRight = bx - 8;
    }

    g.setColour (cText);
    g.setFont (juce::Font (juce::FontOptions (13.5f, juce::Font::bold)));
    g.drawText (e.display, 12, 4, textRight - 12, 19, juce::Justification::centredLeft, true);

    g.setColour (cText3);
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.drawText (e.detail, 12, 22, textRight - 12, 15, juce::Justification::centredLeft, true);
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
        onLoad (mFiltered[sel]);
}

void ModelBrowser::chooseFolder()
{
    chooser = std::make_unique<juce::FileChooser> ("Carpeta de biblioteca", mLibrary.getFolder());
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

    // fila de pestañas
    auto tabsRow = r.removeFromTop (30);
    const int tw = tabsRow.getWidth() / juce::jmax (1, mTabs.size());
    for (int i = 0; i < mTabs.size(); ++i)
        mTabs[i]->setBounds (tabsRow.removeFromLeft (i == mTabs.size() - 1 ? tabsRow.getWidth() : tw).reduced (2, 0));
    r.removeFromTop (8);

    searchBox.setBounds (r.removeFromTop (30));
    r.removeFromTop (8);

    auto bottom = r.removeFromBottom (34);
    countLabel.setBounds (bottom.removeFromLeft (110));
    loadButton.setBounds (bottom.removeFromRight (110));
    bottom.removeFromRight (8);
    changeFolderButton.setBounds (bottom.removeFromRight (120));
    r.removeFromBottom (8);

    listBox.setBounds (r);
}
