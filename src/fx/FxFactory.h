#pragma once
#include "FxBlock.h"
#include <memory>

// Registro de tipos de efecto: crea un bloque por typeId y lista los disponibles
// (para el menú "+ Añadir" de la UI). Añadir un efecto nuevo = una línea aquí.
namespace FxFactory
{
    struct TypeInfo { juce::String typeId, name, category; };

    std::unique_ptr<FxBlock> create (const juce::String& typeId);
    juce::Array<TypeInfo>    available();
}
