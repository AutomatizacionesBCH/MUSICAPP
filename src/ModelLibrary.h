#pragma once

#include <JuceHeader.h>

//==============================================================================
// Capa de datos del explorador (sin UI): escanea la carpeta de librería buscando
// .nam (amps/pedales) Y .wav (IRs) de forma recursiva. Cada entrada lleva su
// "gear" (amp/amp-cab/pedal/cab) + un nombre corto legible (equipo + detalle).
//==============================================================================
class ModelLibrary
{
public:
    struct Entry
    {
        juce::File   file;
        juce::String relPath;   // ruta relativa (para búsqueda)
        juce::String gear;      // "amp" | "amp-cab" | "pedal" | "cab" | "experimental" | "outboard"
        juce::String display;   // nombre corto del equipo (ej. "Fender Super Reverb 1977")
        juce::String detail;    // settings (P5 B6 M8 T6...) o mic/EQ
        juce::String arch;      // "a1" | "a2" | "custom" | "" (IRs/desconocido) -> badge en la lista
        bool         isIR = false;   // .wav (cabinet IR)
    };

    void setFolder (const juce::File& folder);          // fija la carpeta y reescanea
    juce::File getFolder() const { return mFolder; }
    void rescan();

    // Filtra por categoría de pestaña ("amp"|"amp-cab"|"pedal"|"ir"|"" todos) y por
    // substring case-insensitive (sobre relPath + display). Query vacía = todo el tab.
    juce::Array<Entry> filter (const juce::String& tab, const juce::String& query) const;

    int size() const { return mEntries.size(); }
    int countForTab (const juce::String& tab) const;

private:
    juce::File         mFolder;
    juce::Array<Entry> mEntries;
};
