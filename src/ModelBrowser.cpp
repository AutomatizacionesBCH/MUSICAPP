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
    const auto entries = mLibrary.filter (mActiveTab, searchBox.getText());

    // Agrupa por tono (Entry.group): 1 equipo = 1 grupo con todas sus capturas.
    mGroups.clear();
    juce::HashMap<juce::String, int> idx;
    for (const auto& e : entries)
    {
        int gi;
        if (idx.contains (e.group)) { gi = idx[e.group]; }
        else
        {
            gi = (int) mGroups.size();
            idx.set (e.group, gi);
            Group g; g.display = e.display; g.gear = e.gear; g.arch = e.arch;
            mGroups.push_back (std::move (g));
        }
        mGroups[(size_t) gi].caps.add (e);
    }
    // Captura por defecto (neutral): prefiere noon/flat, si no la de detalle más corto (base).
    for (auto& g : mGroups)
    {
        int best = -1;
        for (int i = 0; i < g.caps.size(); ++i)
        {
            const juce::String d = g.caps[i].detail;
            const int s = (d.containsIgnoreCase ("noon") || d.containsIgnoreCase ("flat"))
                              ? 1000 : (200 - juce::jmin (200, d.length()));
            if (s > best) { best = s; g.def = i; }
        }
    }

    rebuildRows();
    listBox.deselectAllRows();
    countLabel.setText (juce::String ((int) mGroups.size()) + " equipos", juce::dontSendNotification);
    repaint();
}

void ModelBrowser::rebuildRows()
{
    mRows.clearQuick();
    for (int gi = 0; gi < (int) mGroups.size(); ++gi)
    {
        mRows.add ({ gi, -1 });
        if (mGroups[(size_t) gi].expanded)
            for (int ci = 0; ci < mGroups[(size_t) gi].caps.size(); ++ci)
                mRows.add ({ gi, ci });
    }
    listBox.updateContent();
}

int ModelBrowser::getNumRows() { return mRows.size(); }

void ModelBrowser::paintArchBadge (juce::Graphics& g, const juce::String& arch, int w, int h, int& textRight)
{
    if (arch.isEmpty()) return;
    const juce::String label = arch.toUpperCase();
    const int bw = (label.length() <= 2) ? 28 : 54;
    const int bh = 16, bx = w - bw - 10, by = (h - bh) / 2;
    juce::Colour bg, fg;
    if (arch == "a2")          { bg = cAccent;                                    fg = juce::Colour (0xff1a0e06); }
    else if (arch == "custom") { bg = juce::Colour (0xff8a5ad2).withAlpha (0.22f); fg = juce::Colour (0xffd8b4fe); }
    else                       { bg = juce::Colour (0xff5684c9).withAlpha (0.20f); fg = juce::Colour (0xff9fc7ff); }
    g.setColour (bg);
    g.fillRoundedRectangle ((float) bx, (float) by, (float) bw, (float) bh, 4.0f);
    g.setColour (fg);
    g.setFont (juce::Font (juce::FontOptions (9.5f, juce::Font::bold)));
    g.drawText (label, bx, by, bw, bh, juce::Justification::centred, false);
    textRight = bx - 8;
}

void ModelBrowser::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (row < 0 || row >= mRows.size())
        return;
    const auto vr = mRows[row];
    const auto& grp = mGroups[(size_t) vr.group];

    if (selected)
    {
        g.setColour (cAccent.withAlpha (0.18f));
        g.fillRect (0, 0, w, h);
        g.setColour (cAccent);
        g.fillRect (0, 0, 3, h);
    }

    int textRight = w - 16;

    if (vr.cap < 0)
    {
        // ---- cabecera del equipo: chevron + nombre + (gear · N capturas) + badge ----
        paintArchBadge (g, grp.arch, w, h, textRight);

        const float cx = 16.0f, cy = h * 0.5f;
        juce::Path tri;
        if (grp.expanded) { tri.addTriangle (cx - 4, cy - 3, cx + 4, cy - 3, cx, cy + 4); }
        else              { tri.addTriangle (cx - 3, cy - 4, cx - 3, cy + 4, cx + 4, cy); }
        g.setColour (grp.expanded ? cAccent : cText3);
        g.fillPath (tri);

        const int tx = 30;
        g.setColour (cText);
        g.setFont (juce::Font (juce::FontOptions (13.5f, juce::Font::bold)));
        g.drawText (grp.display, tx, 4, textRight - tx, 19, juce::Justification::centredLeft, true);

        juce::String sub = grp.gear;
        if (grp.caps.size() > 1) sub << " - " << grp.caps.size() << " capturas";
        g.setColour (cText3);
        g.setFont (juce::Font (juce::FontOptions (11.0f)));
        g.drawText (sub, tx, 22, textRight - tx, 15, juce::Justification::centredLeft, true);
    }
    else
    {
        // ---- captura (indentada): settings/EQ + badge ----
        const auto& e = grp.caps[vr.cap];
        paintArchBadge (g, e.arch, w, h, textRight);
        const int tx = 38;
        g.setColour (cText2);
        g.setFont (juce::Font (juce::FontOptions (12.0f)));
        g.drawText (e.detail.isEmpty() ? juce::String ("(base)") : e.detail,
                    tx, 0, textRight - tx, h, juce::Justification::centredLeft, true);
    }
}

void ModelBrowser::listBoxItemClicked (int row, const juce::MouseEvent&)
{
    if (row < 0 || row >= mRows.size())
        return;
    const auto vr = mRows[row];
    if (vr.cap < 0)   // click en cabecera -> expandir/colapsar
    {
        mGroups[(size_t) vr.group].expanded = ! mGroups[(size_t) vr.group].expanded;
        rebuildRows();
        listBox.selectRow (row);
    }
}

void ModelBrowser::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
{
    listBox.selectRow (row);
    loadSelected();
}

void ModelBrowser::loadSelected()
{
    const int sel = listBox.getSelectedRow();
    if (sel < 0 || sel >= mRows.size() || onLoad == nullptr)
        return;
    const auto vr = mRows[sel];
    const auto& grp = mGroups[(size_t) vr.group];
    if (grp.caps.isEmpty())
        return;
    onLoad ((vr.cap < 0) ? grp.caps[grp.def] : grp.caps[vr.cap]);
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
