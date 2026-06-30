#include "FxChain.h"
#include "FxFactory.h"

std::unique_ptr<FxBlock> FxChain::make (const juce::String& typeId) const
{
    if (factory) return factory (typeId);     // el processor crea amp/cab/drive + delega fx
    return FxFactory::create (typeId);
}

void FxChain::prepare (double sr, int bs)
{
    sampleRate = sr; blockSize = bs;
    const juce::ScopedLock sl (lock);
    for (auto& b : blocks) b->prepare (sr, bs);
}

void FxChain::processMono (float* data, int n)
{
    const juce::ScopedTryLock stl (lock);
    if (! stl.isLocked())          // edición estructural en curso -> pasa en seco
        return;
    for (auto& b : blocks)
        if (! b->bypassed.load (std::memory_order_relaxed))
            b->processMono (data, n);
}

//==============================================================================
int FxChain::add (const juce::String& typeId, int pos)
{
    auto block = make (typeId);
    if (block == nullptr)
        return 0;

    block->prepare (sampleRate, blockSize);   // aloca buffers FUERA del lock
    const int uid = nextUid++;
    block->uid = uid;

    const juce::ScopedLock sl (lock);
    if (pos < 0 || pos >= (int) blocks.size())
        blocks.push_back (std::move (block));
    else
        blocks.insert (blocks.begin() + pos, std::move (block));
    return uid;
}

void FxChain::remove (int uid)
{
    std::unique_ptr<FxBlock> dead;   // se destruye al salir (fuera del lock)
    const juce::ScopedLock sl (lock);
    for (size_t i = 0; i < blocks.size(); ++i)
        if (blocks[i]->uid == uid)
        {
            if (! blocks[i]->removable())   // anclas (amp/cab/drive) no se quitan
                return;
            dead = std::move (blocks[i]);
            blocks.erase (blocks.begin() + (long) i);
            break;
        }
}

void FxChain::move (int uid, int newIndex)
{
    const juce::ScopedLock sl (lock);
    int cur = -1;
    for (size_t i = 0; i < blocks.size(); ++i)
        if (blocks[i]->uid == uid) { cur = (int) i; break; }
    if (cur < 0)
        return;
    newIndex = juce::jlimit (0, (int) blocks.size() - 1, newIndex);
    if (newIndex == cur)
        return;
    auto b = std::move (blocks[(size_t) cur]);
    blocks.erase (blocks.begin() + cur);
    blocks.insert (blocks.begin() + newIndex, std::move (b));
}

void FxChain::setBypass (int uid, bool bypassed)
{
    if (auto* b = find (uid))             // lock-free: sólo toca un atómico
        b->bypassed.store (bypassed);
}

void FxChain::setParam (int uid, const juce::String& paramId, float value)
{
    if (auto* b = find (uid))             // lock-free: sólo toca un atómico
        if (auto* p = b->param (paramId))
            p->set (value);
}

void FxChain::loadFileInto (int uid, const juce::File& file)
{
    // message thread: loadFile prepara el modelo y lo entrega por staging atómico;
    // no toca la estructura de la cadena, así que no necesita el lock.
    if (auto* b = find (uid))
        if (b->canLoadFile())
            b->loadFile (file);
}

int FxChain::firstUidOfKind (const juce::String& kind) const
{
    for (auto& b : blocks)
        if (b->kind() == kind)
            return b->uid;
    return 0;
}

void FxChain::clear()
{
    std::vector<std::unique_ptr<FxBlock>> dead;
    {
        const juce::ScopedLock sl (lock);
        dead = std::move (blocks);
        blocks.clear();
    }
}

//==============================================================================
FxBlock* FxChain::find (int uid) const
{
    for (auto& b : blocks)
        if (b->uid == uid) return b.get();
    return nullptr;
}

juce::var FxChain::describe() const
{
    juce::Array<juce::var> arr;
    for (auto& b : blocks)
    {
        juce::DynamicObject::Ptr o = new juce::DynamicObject();
        o->setProperty ("uid",       b->uid);
        o->setProperty ("type",      b->typeId());
        o->setProperty ("name",      b->displayName());
        o->setProperty ("kind",      b->kind());
        o->setProperty ("removable", b->removable());
        o->setProperty ("bypass",    b->bypassed.load());
        o->setProperty ("extra",     b->extra());

        juce::Array<juce::var> ps;
        for (auto& p : b->params)
        {
            juce::DynamicObject::Ptr po = new juce::DynamicObject();
            po->setProperty ("id",    p->id);
            po->setProperty ("label", p->label);
            po->setProperty ("value", p->get());
            po->setProperty ("min",   p->minV);
            po->setProperty ("max",   p->maxV);
            po->setProperty ("unit",  p->unit);
            ps.add (juce::var (po.get()));
        }
        o->setProperty ("params", ps);
        arr.add (juce::var (o.get()));
    }
    return arr;
}

juce::ValueTree FxChain::toValueTree() const
{
    juce::ValueTree root ("fxchain");
    for (auto& b : blocks)
    {
        juce::ValueTree bt ("block");
        bt.setProperty ("type",   b->typeId(), nullptr);
        bt.setProperty ("bypass", b->bypassed.load(), nullptr);
        for (auto& p : b->params)
            bt.setProperty (p->id, (double) p->get(), nullptr);
        root.appendChild (bt, nullptr);
    }
    return root;
}

void FxChain::fromValueTree (const juce::ValueTree& root)
{
    clear();
    if (! root.hasType ("fxchain"))
        return;
    for (int i = 0; i < root.getNumChildren(); ++i)
    {
        const auto bt = root.getChild (i);
        auto block = make (bt.getProperty ("type").toString());
        if (block == nullptr)
            continue;
        block->prepare (sampleRate, blockSize);
        block->uid = nextUid++;
        block->bypassed.store ((bool) bt.getProperty ("bypass", false));
        for (auto& p : block->params)
            if (bt.hasProperty (p->id))
                p->set ((float) (double) bt.getProperty (p->id));

        const juce::ScopedLock sl (lock);
        blocks.push_back (std::move (block));
    }
}
