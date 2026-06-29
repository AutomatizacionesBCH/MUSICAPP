#include "ModelLibrary.h"
#include <algorithm>

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

    const auto files = mFolder.findChildFiles (juce::File::findFiles, true, "*.nam");
    for (const auto& f : files)
        mEntries.add ({ f, f.getRelativePathFrom (mFolder).replaceCharacter ('\\', '/') });

    std::sort (mEntries.begin(), mEntries.end(),
               [] (const Entry& a, const Entry& b)
               { return a.relPath.compareIgnoreCase (b.relPath) < 0; });
}

juce::Array<ModelLibrary::Entry> ModelLibrary::filter (const juce::String& query) const
{
    if (query.isEmpty())
        return mEntries;

    juce::Array<Entry> out;
    for (const auto& e : mEntries)
        if (e.relPath.containsIgnoreCase (query))
            out.add (e);
    return out;
}
