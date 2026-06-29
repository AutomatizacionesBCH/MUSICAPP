#include "WebUIEditor.h"
#include <cstring>
#include <cstddef>

//==============================================================================
// Assets de la UI (embebidos como literales para no depender de binary-data).
//==============================================================================
namespace
{
    const char* kIndexHtml = R"HTML(<!doctype html>
<html lang="es"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<link rel="stylesheet" href="style.css"></head>
<body>
  <header class="topbar">
    <span class="logo"></span>
    <span class="title">MUSIC APP</span>
    <span class="sub">cadena de senal</span>
  </header>
  <main class="rack">
    <section class="block io">
      <div class="bhead"><span>IN</span></div>
      <div class="knob"></div><div class="kval">0.0 dB</div>
    </section>
    <div class="arrow"></div>
    <section class="block amp on">
      <div class="bhead"><span>AMP &middot; NAM</span></div>
      <div class="photo"><img id="amp-photo" src="amp-photo" alt=""></div>
      <div class="bname" id="amp-name">&mdash;</div>
      <button class="change" id="amp-change">Cambiar</button>
    </section>
    <div class="arrow"></div>
    <section class="block cab">
      <div class="bhead"><span>CAB &middot; IR</span><span class="toggle" id="ir-toggle">OFF</span></div>
      <div class="photo"><img id="cab-photo" src="cab-photo" alt=""></div>
      <div class="bname" id="cab-name">&mdash;</div>
      <button class="change" id="cab-change">Cambiar</button>
    </section>
    <div class="arrow"></div>
    <section class="block fx">
      <div class="bhead"><span>REVERB</span></div>
      <div class="fxicon">&#8767;</div>
      <div class="knob"></div><div class="kval">0%</div>
    </section>
    <div class="arrow"></div>
    <section class="block io">
      <div class="bhead"><span>OUT</span></div>
      <div class="knob"></div><div class="kval">0.0 dB</div>
    </section>
  </main>
  <script src="app.js"></script>
</body></html>
)HTML";

    const char* kStyleCss = R"CSS(
:root{--bg:#0E0F11;--panel:#17191C;--module:#1E2125;--border:#2A2E33;
      --text:#E6E8EA;--muted:#9AA0A6;--dim:#5A6068;--accent:#FF7A29;}
*{box-sizing:border-box;margin:0;padding:0;}
html,body{height:100%;background:var(--bg);color:var(--text);
  font-family:'Segoe UI',Inter,system-ui,sans-serif;-webkit-font-smoothing:antialiased;}
.topbar{display:flex;align-items:center;gap:10px;padding:12px 18px;
  border-bottom:1px solid var(--border);background:var(--panel);}
.logo{width:14px;height:14px;border-radius:3px;background:var(--accent);}
.title{font-weight:700;letter-spacing:.5px;}
.sub{color:var(--muted);font-size:12px;}
.rack{display:flex;align-items:stretch;padding:26px 18px;overflow-x:auto;}
.block{background:var(--module);border:1px solid var(--border);border-radius:10px;
  padding:12px;min-width:166px;display:flex;flex-direction:column;align-items:center;gap:8px;}
.block.io{min-width:100px;justify-content:flex-start;}
.bhead{width:100%;display:flex;justify-content:space-between;align-items:center;
  font-size:11px;font-weight:700;letter-spacing:1.5px;color:var(--muted);}
.amp.on .bhead span:first-child,.cab .bhead span:first-child{color:var(--accent);}
.photo{width:150px;height:104px;border-radius:8px;overflow:hidden;background:#15171a;border:1px solid var(--border);}
.photo img{width:100%;height:100%;object-fit:cover;display:block;}
.bname{font-size:11px;color:var(--text);text-align:center;max-width:150px;
  overflow:hidden;text-overflow:ellipsis;white-space:nowrap;}
.change{background:var(--panel);color:var(--text);border:1px solid var(--border);
  border-radius:5px;padding:5px 12px;font-size:11px;cursor:pointer;}
.change:hover{border-color:var(--accent);}
.toggle{font-size:10px;font-weight:700;color:var(--dim);border:1px solid var(--border);
  border-radius:4px;padding:1px 7px;cursor:pointer;}
.toggle.on{color:#1a0e06;background:var(--accent);border-color:var(--accent);}
.knob{width:48px;height:48px;border-radius:50%;margin-top:8px;position:relative;
  background:conic-gradient(from 216deg,var(--accent) 0deg 144deg,var(--border) 144deg 288deg,transparent 288deg 360deg);}
.knob::after{content:"";position:absolute;inset:6px;border-radius:50%;background:#23262B;}
.kval{font-size:11px;color:var(--muted);font-family:'Cascadia Mono',Consolas,monospace;}
.fxicon{font-size:30px;color:var(--accent);height:104px;display:flex;align-items:center;}
.arrow{align-self:center;width:24px;height:2px;background:var(--border);position:relative;flex:0 0 auto;}
.arrow::after{content:"";position:absolute;right:-1px;top:-3px;border:4px solid transparent;border-left-color:var(--border);}
)CSS";

    const char* kAppJs = R"JS(
async function refresh(){
  try{
    const r = await fetch('state.json?_=' + Date.now());
    const s = await r.json();
    document.getElementById('amp-name').textContent = s.model || '(ninguno)';
    document.getElementById('cab-name').textContent = s.ir || '(ninguno)';
    const t = document.getElementById('ir-toggle');
    if (s.irOn) { t.classList.add('on'); t.textContent = 'ON'; }
    else { t.classList.remove('on'); t.textContent = 'OFF'; }
    document.getElementById('amp-photo').src = 'amp-photo?_=' + Date.now();
    document.getElementById('cab-photo').src = 'cab-photo?_=' + Date.now();
  } catch (e) {}
}
refresh();
)JS";

    juce::WebBrowserComponent::Resource bytesOf (const void* data, size_t n, juce::String mime)
    {
        const auto* b = static_cast<const std::byte*> (data);
        return { std::vector<std::byte> (b, b + n), std::move (mime) };
    }

    juce::WebBrowserComponent::Resource textOf (const char* s, juce::String mime)
    {
        return bytesOf (s, std::strlen (s), std::move (mime));
    }
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
                 .withResourceProvider ([this] (const auto& url) { return provide (url); }))
{
    addAndMakeVisible (webView);
    webView.goToURL (juce::WebBrowserComponent::getResourceProviderRoot());
    setSize (900, 320);
}

void WebUIEditor::resized()
{
    webView.setBounds (getLocalBounds());
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
    o->setProperty ("irOn",  processorRef.getLoadedIRName().isNotEmpty()); // visual; el real va en Etapa 2
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

    if (path == "/index.html") return textOf (kIndexHtml, "text/html");
    if (path == "/style.css")  return textOf (kStyleCss,  "text/css");
    if (path == "/app.js")     return textOf (kAppJs,     "text/javascript");

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
        // Placeholder SVG (sin foto)
        static const char* svg =
            R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="150" height="104">)SVG"
            R"SVG(<rect width="100%" height="100%" fill="#15171a"/>)SVG"
            R"SVG(<text x="50%" y="52%" fill="#5A6068" font-family="sans-serif" font-size="12" text-anchor="middle">sin foto</text></svg>)SVG";
        return textOf (svg, "image/svg+xml");
    }

    return std::nullopt;
}
