#include "WebUIEditor.h"
#include "ModelBrowser.h"
#include "fx/FxFactory.h"
#include "BinaryData.h"
#include <cstddef>
#include <cstring>

namespace
{
    juce::WebBrowserComponent::Resource bytesOf (const void* data, size_t n, juce::String mime)
    {
        const auto* b = static_cast<const std::byte*> (data);
        return { std::vector<std::byte> (b, b + n), std::move (mime) };
    }

    // Ventana popup que hospeda el ModelBrowser; al cerrar avisa por callback.
    class BrowserWindow : public juce::DocumentWindow
    {
    public:
        BrowserWindow (juce::Component* content, std::function<void()> onClose)
            : juce::DocumentWindow ("Biblioteca de modelos", juce::Colour (0xff0E0F11),
                                    juce::DocumentWindow::closeButton),
              mOnClose (std::move (onClose))
        {
            setUsingNativeTitleBar (true);
            setContentOwned (content, true);
            setResizable (true, false);
            centreWithSize (560, 460);
            setVisible (true);
        }
        void closeButtonPressed() override { if (mOnClose) mOnClose(); }
    private:
        std::function<void()> mOnClose;
    };
}

//==============================================================================
WebUIEditor::WebUIEditor (MusicAppAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p),
      webView (juce::WebBrowserComponent::Options{}
                 .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                 .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2{}
                     .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                            .getChildFile ("MusicAppWebView2")))
                 .withNativeIntegrationEnabled()
                 .withResourceProvider ([this] (const auto& url) { return provide (url); })
                 .withOptionsFrom (inputRelay)
                 .withOptionsFrom (outputRelay)
                 .withNativeFunction ("loadModel",
                     [this] (const juce::Array<juce::var>&, auto complete)
                     { openModelBrowser ("amp"); complete (juce::var()); })
                 .withNativeFunction ("loadPedal",
                     [this] (const juce::Array<juce::var>&, auto complete)
                     { openModelBrowser ("pedal"); complete (juce::var()); })
                 .withNativeFunction ("loadIR",
                     [this] (const juce::Array<juce::var>&, auto complete)
                     { openModelBrowser ("ir"); complete (juce::var()); })
                 .withNativeFunction ("listModels",
                     [this] (const juce::Array<juce::var>& args, auto complete)
                     {
                         const juce::String tab = args.size() > 0 ? args[0].toString() : juce::String();
                         const juce::String q   = args.size() > 1 ? args[1].toString() : juce::String();
                         const auto entries = mLibrary.filter (tab, q);

                         // Agrupa por tono (Entry.group): 1 equipo = 1 grupo con todas sus capturas.
                         struct Grp { juce::String name, gear, arch; bool ir = false; juce::Array<juce::var> caps; };
                         std::vector<Grp> groups;
                         juce::HashMap<juce::String, int> idx;
                         for (const auto& e : entries)
                         {
                             int gi;
                             if (idx.contains (e.group)) { gi = idx[e.group]; }
                             else
                             {
                                 if ((int) groups.size() >= 400) continue;     // tope de equipos
                                 gi = (int) groups.size();
                                 idx.set (e.group, gi);
                                 Grp g; g.name = e.display; g.gear = e.gear; g.arch = e.arch; g.ir = e.isIR;
                                 groups.push_back (std::move (g));
                             }
                             auto& g = groups[(size_t) gi];
                             if (g.caps.size() < 400)
                             {
                                 juce::DynamicObject::Ptr c = new juce::DynamicObject();
                                 c->setProperty ("detail", e.detail);
                                 c->setProperty ("arch",   e.arch);
                                 c->setProperty ("path",   e.file.getFullPathName());
                                 g.caps.add (juce::var (c.get()));
                             }
                         }

                         juce::Array<juce::var> out;
                         for (auto& g : groups)
                         {
                             // default = captura mas "neutral": prefiere noon/flat, si no la de detalle mas corto (base).
                             juce::String def; int best = -1;
                             for (const auto& cv : g.caps)
                             {
                                 auto* o = cv.getDynamicObject();
                                 const juce::String d = o->getProperty ("detail").toString();
                                 int score = (d.containsIgnoreCase ("noon") || d.containsIgnoreCase ("flat"))
                                                 ? 1000 : (200 - juce::jmin (200, d.length()));
                                 if (score > best) { best = score; def = o->getProperty ("path").toString(); }
                             }
                             juce::DynamicObject::Ptr go = new juce::DynamicObject();
                             go->setProperty ("name",    g.name);
                             go->setProperty ("gear",    g.gear);
                             go->setProperty ("arch",    g.arch);
                             go->setProperty ("ir",      g.ir);
                             go->setProperty ("count",   g.caps.size());
                             go->setProperty ("defaultPath", def);
                             go->setProperty ("captures", g.caps);
                             out.add (juce::var (go.get()));
                         }
                         complete (juce::var (out));
                     })
                 .withNativeFunction ("loadModelByPath",
                     [this] (const juce::Array<juce::var>& args, auto complete)
                     {
                         if (args.size() > 0)
                         {
                             const juce::File f (args[0].toString());
                             if (f.hasFileExtension ("wav"))   // IR -> cab
                             {
                                 if (processorRef.loadIR (f)) notifyChanged();
                             }
                             else
                             {
                                 const juce::String rel  = f.getRelativePathFrom (mLibrary.getFolder()).replaceCharacter ('\\', '/');
                                 const juce::String gear = rel.upToFirstOccurrenceOf ("/", false, false);
                                 if (gear == "pedal")        // pedal -> bloque Drive
                                 {
                                     const int uid = processorRef.fx().firstUidOfKind ("drive");
                                     if (uid != 0) { processorRef.fx().loadFileInto (uid, f); notifyChanged(); }
                                 }
                                 else                        // amp / amp-cab / ...
                                 {
                                     if (processorRef.loadNamModel (f)) notifyChanged();
                                 }
                             }
                         }
                         complete (juce::var());
                     })
                 .withNativeFunction ("savePreset",
                     [this] (const juce::Array<juce::var>& args, auto complete)
                     {
                         if (args.size() > 0) writePreset (args[0].toString());
                         complete (juce::var());
                     })
                 .withNativeFunction ("listPresets",
                     [this] (const juce::Array<juce::var>&, auto complete)
                     {
                         juce::Array<juce::var> out;
                         for (const auto& f : presetsDir().findChildFiles (juce::File::findFiles, false, "*.json"))
                         {
                             juce::DynamicObject::Ptr e = new juce::DynamicObject();
                             e->setProperty ("name", f.getFileNameWithoutExtension());
                             e->setProperty ("path", f.getFullPathName());
                             out.add (juce::var (e.get()));
                         }
                         complete (juce::var (out));
                     })
                 .withNativeFunction ("loadPreset",
                     [this] (const juce::Array<juce::var>& args, auto complete)
                     {
                         juce::String name;
                         if (args.size() > 0) name = applyPreset (juce::File (args[0].toString()));
                         juce::DynamicObject::Ptr r = new juce::DynamicObject();
                         r->setProperty ("name", name);
                         complete (juce::var (r.get()));
                     })
                 //== Rack flexible de efectos ==
                 .withNativeFunction ("fxTypes",
                     [this] (const juce::Array<juce::var>&, auto complete)
                     {
                         juce::Array<juce::var> out;
                         for (const auto& t : FxFactory::available())
                         {
                             juce::DynamicObject::Ptr o = new juce::DynamicObject();
                             o->setProperty ("type", t.typeId);
                             o->setProperty ("name", t.name);
                             out.add (juce::var (o.get()));
                         }
                         complete (juce::var (out));
                     })
                 .withNativeFunction ("fxGetChain",
                     [this] (const juce::Array<juce::var>&, auto complete)
                     { complete (processorRef.fx().describe()); })
                 .withNativeFunction ("fxAdd",
                     [this] (const juce::Array<juce::var>& args, auto complete)
                     {
                         if (args.size() > 0) processorRef.fx().add (args[0].toString());
                         complete (processorRef.fx().describe());
                     })
                 .withNativeFunction ("fxRemove",
                     [this] (const juce::Array<juce::var>& args, auto complete)
                     {
                         if (args.size() > 0) processorRef.fx().remove ((int) args[0]);
                         complete (processorRef.fx().describe());
                     })
                 .withNativeFunction ("fxMove",
                     [this] (const juce::Array<juce::var>& args, auto complete)
                     {
                         if (args.size() > 1) processorRef.fx().move ((int) args[0], (int) args[1]);
                         complete (processorRef.fx().describe());
                     })
                 .withNativeFunction ("fxBypass",
                     [this] (const juce::Array<juce::var>& args, auto complete)
                     {
                         if (args.size() > 1) processorRef.fx().setBypass ((int) args[0], (bool) args[1]);
                         complete (processorRef.fx().describe());
                     })
                 .withNativeFunction ("fxSetParam",
                     [this] (const juce::Array<juce::var>& args, auto complete)
                     {
                         if (args.size() > 2)
                             processorRef.fx().setParam ((int) args[0], args[1].toString(), (float) (double) args[2]);
                         complete (juce::var());
                     })
                 .withNativeFunction ("appHeight",
                     [this] (const juce::Array<juce::var>&, auto complete)
                     { complete (juce::var (getHeight())); }))
{
    addAndMakeVisible (webView);

    inAtt  = std::make_unique<juce::WebSliderParameterAttachment> (
                 *processorRef.apvts.getParameter ("inputGain"),  inputRelay,  nullptr);
    outAtt = std::make_unique<juce::WebSliderParameterAttachment> (
                 *processorRef.apvts.getParameter ("outputGain"), outputRelay, nullptr);

    // Carpeta de librería persistida (para el ModelBrowser).
    juce::PropertiesFile::Options opts;
    opts.applicationName     = "Music App";
    opts.filenameSuffix      = "settings";
    opts.folderName          = "Music App";
    opts.osxLibrarySubFolder = "Application Support";
    mSettings = std::make_unique<juce::PropertiesFile> (opts);

    juce::File libFolder (mSettings->getValue ("modelLibraryFolder", {}));
    if (! libFolder.isDirectory())
        libFolder = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                      .getChildFile ("Music App").getChildFile ("models");
    mLibrary.setFolder (libFolder);

    webView.goToURL (juce::WebBrowserComponent::getResourceProviderRoot());
    setSize (1320, 700);   // reskin pedalboard: topbar + board + footer (flujo normal)

    mPitchBuf.assign (2048, 0.0f);
    startTimerHz (24);   // medidores + afinador
}

WebUIEditor::~WebUIEditor()
{
    stopTimer();
}

void WebUIEditor::resized()
{
    webView.setBounds (getLocalBounds());
}

//==============================================================================
void WebUIEditor::timerCallback()
{
    juce::DynamicObject::Ptr o = new juce::DynamicObject();
    o->setProperty ("in",  processorRef.getInPeak());
    o->setProperty ("out", processorRef.getOutPeak());
    o->setProperty ("hz",  detectPitch());
    webView.emitEventIfBrowserIsVisible ("meters", juce::var (o.get()));
}

float WebUIEditor::detectPitch()
{
    const int N = (int) mPitchBuf.size();
    processorRef.getRecentInput (mPitchBuf.data(), N);

    double energy = 0.0;
    for (int i = 0; i < N; ++i) energy += (double) mPitchBuf[i] * mPitchBuf[i];
    if (std::sqrt (energy / N) < 0.004)   // gate de silencio/ruido
        return 0.0f;

    const double sr = processorRef.getSampleRate();
    const int minLag = juce::jmax (2, (int) (sr / 400.0));     // hasta ~400 Hz
    const int maxLag = juce::jmin ((int) (sr / 70.0), N / 2);  // hasta ~70 Hz

    double bestCorr = 0.0; int bestLag = 0;
    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        double corr = 0.0;
        for (int i = 0; i < N - lag; ++i)
            corr += (double) mPitchBuf[i] * mPitchBuf[i + lag];
        if (corr > bestCorr) { bestCorr = corr; bestLag = lag; }
    }

    if (bestLag == 0 || bestCorr < 0.5 * energy)   // claridad insuficiente
        return 0.0f;

    return (float) (sr / (double) bestLag);
}

void WebUIEditor::notifyChanged()
{
    webView.emitEventIfBrowserIsVisible ("modelChanged", juce::var());
}

//==============================================================================
juce::File WebUIEditor::presetsDir() const
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
             .getChildFile ("Music App").getChildFile ("presets");
}

void WebUIEditor::writePreset (const juce::String& nameIn)
{
    const juce::String name = nameIn.trim().isEmpty() ? juce::String ("preset") : nameIn.trim();
    auto dir = presetsDir();
    dir.createDirectory();

    juce::DynamicObject::Ptr root = new juce::DynamicObject();
    root->setProperty ("name", name);

    juce::DynamicObject::Ptr params = new juce::DynamicObject();
    for (auto* id : { "inputGain", "outputGain", "irOn" })
        if (auto* param = processorRef.apvts.getParameter (id))
            params->setProperty (id, (double) param->getValue());   // valor normalizado [0,1]
    root->setProperty ("params", juce::var (params.get()));
    root->setProperty ("modelPath", processorRef.getLoadedModelFile().getFullPathName());
    root->setProperty ("irPath",    processorRef.getLoadedIRFile().getFullPathName());

    const auto safe = juce::File::createLegalFileName (name);
    dir.getChildFile (safe + ".json").replaceWithText (juce::JSON::toString (juce::var (root.get())));
}

juce::String WebUIEditor::applyPreset (const juce::File& file)
{
    const auto v = juce::JSON::parse (file.loadFileAsString());
    if (! v.isObject())
        return {};

    if (auto* params = v.getProperty ("params", juce::var()).getDynamicObject())
        for (const auto& kv : params->getProperties())
            if (auto* param = processorRef.apvts.getParameter (kv.name.toString()))
                param->setValueNotifyingHost ((float) (double) kv.value);

    const juce::String mp = v.getProperty ("modelPath", juce::var()).toString();
    if (mp.isNotEmpty()) { const juce::File f (mp); if (f.existsAsFile()) processorRef.loadNamModel (f); }

    const juce::String ip = v.getProperty ("irPath", juce::var()).toString();
    if (ip.isNotEmpty()) { const juce::File f (ip); if (f.existsAsFile()) processorRef.loadIR (f); }

    notifyChanged();
    return v.getProperty ("name", juce::var()).toString();
}

//==============================================================================
void WebUIEditor::openModelBrowser (const juce::String& defaultTab)
{
    if (mBrowserWindow != nullptr)
    {
        if (auto* b = dynamic_cast<ModelBrowser*> (mBrowserWindow->getContentComponent()))
            b->setActiveTab (defaultTab);
        mBrowserWindow->toFront (true);
        return;
    }

    auto* browser = new ModelBrowser (mLibrary);
    browser->setSize (580, 520);
    browser->setActiveTab (defaultTab);
    // Rutea por TIPO de la entrada: IR -> cab; pedal -> bloque Drive; resto -> amp.
    browser->onLoad = [this] (ModelLibrary::Entry e)
    {
        if (e.isIR)
        {
            if (processorRef.loadIR (e.file))
                notifyChanged();
        }
        else if (e.gear == "pedal")
        {
            const int uid = processorRef.fx().firstUidOfKind ("drive");
            if (uid != 0)
            {
                processorRef.fx().loadFileInto (uid, e.file);
                notifyChanged();
            }
        }
        else                                             // amp / amp-cab / experimental / outboard
        {
            if (processorRef.loadNamModel (e.file))
                notifyChanged();
        }
    };
    browser->onFolderChanged = [this] (juce::File dir)
    {
        if (mSettings != nullptr)
        {
            mSettings->setValue ("modelLibraryFolder", dir.getFullPathName());
            mSettings->saveIfNeeded();
        }
    };

    juce::Component::SafePointer<WebUIEditor> safe (this);
    mBrowserWindow.reset (new BrowserWindow (browser, [safe]
    {
        juce::MessageManager::callAsync ([safe]
        {
            if (auto* e = safe.getComponent())
                e->mBrowserWindow.reset();
        });
    }));
}

void WebUIEditor::chooseIR()
{
    juce::File startDir;
    if (mSettings != nullptr)   // misma librería que los modelos -> subcarpeta cab (IRs)
    {
        const juce::File lib (mSettings->getValue ("modelLibraryFolder", {}));
        if (lib.getChildFile ("cab").isDirectory()) startDir = lib.getChildFile ("cab");
        else if (lib.isDirectory())                 startDir = lib;
    }
    if (! startDir.isDirectory())
        startDir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                     .getChildFile ("Music App").getChildFile ("irs");
    if (! startDir.isDirectory())
        startDir = juce::File::getSpecialLocation (juce::File::userMusicDirectory);

    mChooser = std::make_unique<juce::FileChooser> ("Elige un Cabinet IR (.wav)", startDir, "*.wav");
    mChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (f != juce::File() && processorRef.loadIR (f))
                notifyChanged();
        });
}

//==============================================================================
juce::File WebUIEditor::imageFor (const juce::File& mediaFile) const
{
    if (! mediaFile.existsAsFile())
        return {};
    const auto dir = mediaFile.getParentDirectory().getChildFile ("images");
    if (! dir.isDirectory())
        return {};
    const auto imgs = dir.findChildFiles (juce::File::findFiles, false, "*.jpg;*.jpeg;*.png;*.webp");
    return imgs.isEmpty() ? juce::File{} : imgs.getReference (0);
}

juce::String WebUIEditor::stateJson() const
{
    juce::DynamicObject::Ptr o = new juce::DynamicObject();
    o->setProperty ("model", processorRef.getLoadedModelName());
    o->setProperty ("ir",    processorRef.getLoadedIRName());
    return juce::JSON::toString (juce::var (o.get()));
}

std::optional<WebUIEditor::Resource> WebUIEditor::provide (const juce::String& urlIn)
{
    // Quedarnos solo con la ruta (puede venir como URL completa o como path).
    auto path = urlIn;
    const auto scheme = path.indexOf ("://");
    if (scheme >= 0)
    {
        const auto afterScheme = path.substring (scheme + 3);
        const auto slash = afterScheme.indexOfChar ('/');
        path = slash >= 0 ? afterScheme.substring (slash) : juce::String ("/");
    }
    path = path.upToFirstOccurrenceOf ("?", false, false);
    if (path.isEmpty() || path == "/")
        path = "/index.html";

    if (path == "/index.html")
        return bytesOf (BinaryData::index_html, (size_t) BinaryData::index_htmlSize, "text/html");
    if (path == "/style.css")
        return bytesOf (BinaryData::style_css, (size_t) BinaryData::style_cssSize, "text/css");
    if (path == "/app.js")
        return bytesOf (BinaryData::app_js, (size_t) BinaryData::app_jsSize, "text/javascript");
    if (path == "/juce/index.js")
        return bytesOf (BinaryData::index_js, (size_t) BinaryData::index_jsSize, "text/javascript");
    if (path == "/juce/check_native_interop.js")
        return bytesOf (BinaryData::check_native_interop_js,
                        (size_t) BinaryData::check_native_interop_jsSize, "text/javascript");

    if (path == "/state.json")
    {
        const auto js = stateJson();
        return bytesOf (js.toRawUTF8(), (size_t) js.getNumBytesAsUTF8(), "application/json");
    }

    if (path == "/amp-photo" || path == "/cab-photo")
    {
        const auto media = (path == "/amp-photo") ? processorRef.getLoadedModelFile()
                                                   : processorRef.getLoadedIRFile();
        const auto img = imageFor (media);
        if (img.existsAsFile())
        {
            juce::MemoryBlock mb;
            if (img.loadFileAsData (mb))
            {
                const juce::String mime = img.hasFileExtension ("png")  ? "image/png"
                                        : img.hasFileExtension ("webp") ? "image/webp"
                                                                        : "image/jpeg";
                return bytesOf (mb.getData(), mb.getSize(), mime);
            }
        }
        static const char* svg =
            R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="150" height="104">)SVG"
            R"SVG(<rect width="100%" height="100%" fill="#15171a"/>)SVG"
            R"SVG(<text x="50%" y="52%" fill="#5A6068" font-family="sans-serif" font-size="12" text-anchor="middle">sin foto</text></svg>)SVG";
        return bytesOf (svg, std::strlen (svg), "image/svg+xml");
    }

    return std::nullopt;
}
