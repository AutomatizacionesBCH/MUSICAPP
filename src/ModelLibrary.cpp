#include "ModelLibrary.h"
#include <algorithm>

namespace
{
    // "fender-super-reverb-1977-19" -> "Fender Super Reverb 1977" (quita el id final).
    juce::String deslugTitle (juce::String s)
    {
        const int dash = s.lastIndexOfChar ('-');
        if (dash > 0 && s.substring (dash + 1).containsOnly ("0123456789"))
            s = s.substring (0, dash);
        s = s.replaceCharacter ('-', ' ').replaceCharacter ('_', ' ');
        juce::StringArray words;
        words.addTokens (s, " ", "");
        words.removeEmptyStrings();
        for (auto& w : words)
            if (w.isNotEmpty())
                w = w.substring (0, 1).toUpperCase() + w.substring (1);
        return words.joinIntoString (" ");
    }

    // quita del inicio de `s` las palabras que coinciden con `prefix` (para no repetir
    // el nombre del equipo en el detalle).
    juce::String stripWordPrefix (const juce::String& s, const juce::String& prefix)
    {
        juce::StringArray sw, pw;
        sw.addTokens (s, " ", "");      sw.removeEmptyStrings();
        pw.addTokens (prefix, " ", ""); pw.removeEmptyStrings();
        int i = 0;
        while (i < sw.size() && i < pw.size() && sw[i].equalsIgnoreCase (pw[i]))
            ++i;
        juce::StringArray rest;
        for (int j = i; j < sw.size(); ++j) rest.add (sw[j]);
        return rest.joinIntoString (" ");
    }

    // Detalle corto: EQ/mic (parte del filename antes de "__") + arquitectura (a1/a2).
    juce::String fileDetail (const juce::String& fileName, const juce::String& equipment)
    {
        const juce::String base = fileName.upToLastOccurrenceOf (".", false, false);
        juce::StringArray fields;
        fields.addTokens (base, "__", "");   // {modelName}__{size}__{arch}__{id}
        const juce::String modelName = fields.size() > 0 ? fields[0] : base;
        const juce::String arch      = fields.size() >= 2 ? fields[fields.size() - 2] : juce::String();

        juce::String mn = modelName.replaceCharacter ('-', ' ').trim();
        mn = stripWordPrefix (mn, equipment);
        while (mn.startsWithChar (',') || mn.startsWithChar (' '))
            mn = mn.substring (1).trim();
        if (mn.length() > 40)
            mn = mn.substring (0, 38).trim() + "...";

        juce::String detail = mn;   // ASCII (se muestra en el WebView via JSON)
        if (arch.isNotEmpty() && ! arch.equalsIgnoreCase ("None") && ! arch.equalsIgnoreCase ("ir"))
            detail = detail.isEmpty() ? arch.toUpperCase() : detail + " - " + arch.toUpperCase();
        return detail;
    }

    // ¿La entrada pertenece a la pestaña? (experimental/outboard van a AMP).
    bool gearMatchesTab (const juce::String& gear, bool isIR, const juce::String& tab)
    {
        if (tab.isEmpty())    return true;
        if (tab == "ir")      return isIR;
        if (tab == "amp")     return ! isIR && (gear == "amp" || gear == "experimental" || gear == "outboard");
        if (tab == "amp-cab") return ! isIR && gear == "amp-cab";
        if (tab == "pedal")   return ! isIR && gear == "pedal";
        return true;
    }
}

void ModelLibrary::setFolder (const juce::File& folder)
{
    mFolder = folder;
    rescan();
}

void ModelLibrary::rescan()
{
    mEntries.clearQuick();
    if (! mFolder.isDirectory())
        return;

    juce::Array<juce::File> files;
    files.addArray (mFolder.findChildFiles (juce::File::findFiles, true, "*.nam"));
    files.addArray (mFolder.findChildFiles (juce::File::findFiles, true, "*.wav"));

    for (const auto& f : files)
    {
        Entry e;
        e.file    = f;
        e.relPath = f.getRelativePathFrom (mFolder).replaceCharacter ('\\', '/');
        e.isIR    = f.hasFileExtension ("wav");

        juce::StringArray segs;
        segs.addTokens (e.relPath, "/", "");
        e.gear = segs.size() > 0 ? segs[0] : juce::String();
        const juce::String toneFolder = segs.size() >= 2 ? segs[segs.size() - 2] : juce::String();

        e.display = deslugTitle (toneFolder);
        if (e.display.isEmpty())
            e.display = f.getFileNameWithoutExtension();
        e.detail = fileDetail (f.getFileName(), e.display);
        mEntries.add (e);
    }

    std::sort (mEntries.begin(), mEntries.end(),
               [] (const Entry& a, const Entry& b)
               { return a.display.compareIgnoreCase (b.display) < 0; });
}

juce::Array<ModelLibrary::Entry> ModelLibrary::filter (const juce::String& tab, const juce::String& query) const
{
    juce::Array<Entry> out;
    for (const auto& e : mEntries)
    {
        if (! gearMatchesTab (e.gear, e.isIR, tab))
            continue;
        if (query.isNotEmpty()
            && ! e.relPath.containsIgnoreCase (query)
            && ! e.display.containsIgnoreCase (query)
            && ! e.detail.containsIgnoreCase (query))
            continue;
        out.add (e);
    }
    return out;
}

int ModelLibrary::countForTab (const juce::String& tab) const
{
    int n = 0;
    for (const auto& e : mEntries)
        if (gearMatchesTab (e.gear, e.isIR, tab))
            ++n;
    return n;
}
