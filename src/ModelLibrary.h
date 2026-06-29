#pragma once

#include <JuceHeader.h>

//==============================================================================
// Capa de datos del explorador de modelos (sin UI): escanea una carpeta de
// librería buscando .nam de forma recursiva y filtra por substring.
//==============================================================================
class ModelLibrary
{
public:
    struct Entry
    {
        juce::File   file;
        juce::String relPath;   // ruta relativa a la carpeta (para mostrar y buscar)
    };

    void setFolder (const juce::File& folder);          // fija la carpeta y reescanea
    juce::File getFolder() const { return mFolder; }
    void rescan();

    // Substring case-insensitive sobre relPath; query vacía devuelve todo.
    juce::Array<Entry> filter (const juce::String& query) const;

    int size() const { return mEntries.size(); }

private:
    juce::File         mFolder;
    juce::Array<Entry> mEntries;
};
