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

    // Abreviatura de perilla (cualquier caso) -> etiqueta compacta canónica. "" si no es perilla.
    juce::String canonKnob (const juce::String& letters)
    {
        const juce::String l = letters.toLowerCase();
        if (l == "gain")                                      return "G";
        if (l == "bass" || l == "low" || l == "lows")         return "B";
        if (l == "mid"  || l == "mids" || l == "middle")      return "M";
        if (l == "treble" || l == "treb" || l == "tone"
            || l == "high" || l == "highs")                   return "T";
        if (l == "presence" || l == "pres" || l == "pre")     return "P";
        if (l == "volume" || l == "vol")                      return "V";
        if (l == "master" || l == "mast" || l == "mv" || l == "ms") return "MV";
        if (l == "drive"  || l == "drv")                      return "D";
        if (l == "dist"   || l == "distortion")               return "Dist";
        if (l == "level"  || l == "lvl" || l == "lev")        return "L";
        if (l == "sustain"|| l == "sus")                      return "Sus";
        if (l == "cut")                                       return "Cut";
        if (l == "isf")                                       return "ISF";
        if (l == "depth"  || l == "dep")                      return "Dep";
        // una sola letra del set EQ conocido (g/b/m/t/p/v/d/l)
        if (l.length() == 1 && juce::String ("gbmtpvdl").containsChar (l[0]))
            return l.toUpperCase();
        return {};
    }

    bool isPureNumber (const juce::String& t)
    {
        return t.isNotEmpty()
            && juce::CharacterFunctions::isDigit (t[0])
            && t.containsOnly ("0123456789.,");
    }

    // Valida/normaliza el valor de una perilla; "" si inválido.
    // letras cortas (<=2) => rango 0..10; palabras largas => 0..100. Descarta >=3 dígitos enteros.
    juce::String knobValue (const juce::String& numRaw, int srcLetterLen)
    {
        juce::String n = numRaw.replaceCharacter (',', '.');
        if (n.isEmpty() || ! juce::CharacterFunctions::isDigit (n[0]))
            return {};
        int dots = 0;
        for (int i = 0; i < n.length(); ++i)
        {
            const auto c = n[i];
            if (c == '.') { if (++dots > 1) return {}; }
            else if (! juce::CharacterFunctions::isDigit (c)) return {};
        }
        if (n.upToFirstOccurrenceOf (".", false, false).length() >= 3)
            return {};
        const double v = n.getDoubleValue();
        const double maxv = (srcLetterLen <= 2) ? 10.0 : 100.0;
        if (v < 0.0 || v > maxv) return {};
        if (n.endsWith (".0")) n = n.dropLastCharacters (2);
        return n;
    }

    // Extrae los settings (perillas) del {modelName}: "P5 B6 M8 T6 MV6 G7".
    // Ignora ruido (capturer, load, mic, números de modelo/año) por construcción:
    // sólo acepta tokens que parsean como abreviatura-conocida + número-en-rango.
    juce::String parseSettings (const juce::String& modelName)
    {
        static const juce::String LETTERS ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");
        juce::String s = modelName.replaceCharacter ('-', ' ').replaceCharacter ('_', ' ');
        s = s.replace ("percent", " ", true).replace ("%", " ", true);   // "Gain 25percent" -> "Gain 25"
        juce::StringArray raw;
        raw.addTokens (s, " ", "");
        raw.removeEmptyStrings();

        struct Cand { juce::String canon, value; int srcLen; };
        juce::Array<Cand> cands;

        for (int i = 0; i < raw.size(); ++i)
        {
            const juce::String tk = raw[i];

            // Caso B: abreviatura sola + número aparte ("Vol 7", "Gain 75", "Drive 5")
            if (tk.containsOnly (LETTERS))
            {
                const juce::String canon = canonKnob (tk);
                if (canon.isNotEmpty() && i + 1 < raw.size() && isPureNumber (raw[i + 1]))
                {
                    const juce::String v = knobValue (raw[i + 1], tk.length());
                    if (v.isNotEmpty()) { cands.add ({ canon, v, tk.length() }); ++i; }
                }
                continue;
            }

            // Caso A: token = (letras)(número), una o varias veces ("P5", "MV6", "T6B6D9V6")
            juce::Array<Cand> local;
            int pos = 0; bool ok = true;
            while (pos < tk.length())
            {
                int j = pos;
                while (j < tk.length() && juce::CharacterFunctions::isLetter (tk[j])) ++j;
                int k = j;
                while (k < tk.length()
                       && (juce::CharacterFunctions::isDigit (tk[k]) || tk[k] == '.' || tk[k] == ','))
                    ++k;
                const juce::String letters = tk.substring (pos, j);
                const juce::String num     = tk.substring (j, k);
                if (letters.isEmpty() || num.isEmpty()) { ok = false; break; }
                const juce::String canon = canonKnob (letters);
                const juce::String v     = knobValue (num, letters.length());
                if (canon.isEmpty() || v.isEmpty()) { ok = false; break; }
                local.add ({ canon, v, letters.length() });
                pos = k;
            }
            if (ok && pos == tk.length())
                cands.addArray (local);
        }

        bool hasEQ = false;
        for (const auto& c : cands)
            if (c.canon == "B" || c.canon == "T" || c.canon == "P" || c.canon == "G")
                { hasEQ = true; break; }

        juce::StringArray out;
        for (const auto& c : cands)
        {
            if (c.canon == "M" && ! hasEQ)                       continue;  // Mid sólo con EQ
            if (c.canon == "V" && c.srcLen == 1 && cands.size() < 2) continue;  // V suelto = versión
            const juce::String t = c.canon + c.value;
            if (out.size() > 0 && out[out.size() - 1] == t)      continue;  // dedupe
            out.add (t);
            if (out.size() >= 7) break;                                     // compacto
        }
        return out.joinIntoString (" ");
    }

    // Tokens de ruido (capturer, load, ESR, preset...) que no aportan al detalle.
    bool isNoiseToken (const juce::String& t)
    {
        const juce::String l = t.toLowerCase();
        if (l.startsWith ("esr") || l.contains ("epoch"))
            return true;
        static const char* noise[] = {
            "vossen", "suhrrl", "suhr", "di", "pny", "peterny", "reedd", "tc", "nam",
            "wav", "diy", "clone", "capture", "preset", "standard", "standart",
            "feather", "lite", "nano", "nocab", "preamp", "poweramp", "load", "tone3000" };
        for (auto* n : noise)
            if (l == n) return true;
        return false;
    }

    // Arquitectura del modelo desde el filename {modelName}__{size}__{arch}__{id}:
    // "a1" | "a2" | "custom" | "" (IRs / desconocido).
    juce::String archOf (const juce::String& fileName)
    {
        const juce::String base = fileName.upToLastOccurrenceOf (".", false, false);
        if (! base.contains ("__"))
            return {};
        const juce::String a = base.upToLastOccurrenceOf ("__", false, false)
                                   .fromLastOccurrenceOf ("__", false, false)
                                   .toLowerCase();
        return (a == "a1" || a == "a2" || a == "custom") ? a : juce::String();
    }

    // Detalle corto: settings (gain/bass/mid/treble...) o mic/EQ (la arquitectura va aparte, en un badge).
    juce::String fileDetail (const juce::String& fileName, const juce::String& equipment)
    {
        // {modelName}__{size}__{arch}__{id}  (separador es el STRING "__", no el char '_')
        const juce::String base = fileName.upToLastOccurrenceOf (".", false, false);
        const juce::String modelName = base.contains ("__")
            ? base.upToFirstOccurrenceOf ("__", false, false) : base;

        juce::String body = parseSettings (modelName);   // perillas

        if (body.isEmpty())   // fallback: descripción mic/EQ (como antes)
        {
            juce::String mn = modelName.replaceCharacter ('-', ' ').replaceCharacter ('_', ' ').trim();
            if (mn.startsWithChar ('#'))
                mn = mn.fromFirstOccurrenceOf (" ", false, false).trim();
            mn = stripWordPrefix (mn, equipment);
            // quita tokens de ruido (capturer, load, ESR, preset...)
            juce::StringArray ft, keep;
            ft.addTokens (mn, " ", "");
            ft.removeEmptyStrings();
            for (const auto& t : ft)
                if (! isNoiseToken (t)) keep.add (t);
            mn = keep.joinIntoString (" ");
            while (mn.startsWithChar (',') || mn.startsWithChar (' '))
                mn = mn.substring (1).trim();
            if (mn.length() > 40)
                mn = mn.substring (0, 38).trim() + "...";
            body = mn;
        }

        return body;
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
        e.arch   = e.isIR ? juce::String() : archOf (f.getFileName());
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
