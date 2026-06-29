import * as Juce from "./juce/index.js";

// --- estado (nombres + fotos) vía resource provider ---
async function refreshState() {
  try {
    const s = await (await fetch("state.json?_=" + Date.now())).json();
    document.getElementById("amp-name").textContent = s.model || "(ninguno)";
    document.getElementById("cab-name").textContent = s.ir || "(ninguno)";
    document.getElementById("amp-photo").src = "amp-photo?_=" + Date.now();
    document.getElementById("cab-photo").src = "cab-photo?_=" + Date.now();
  } catch (e) {}
}

// --- knob arrastrable, sincronizado con un WebSliderRelay del backend ---
function bindKnob(knobId, valId, paramName, fmt) {
  const knob = document.getElementById(knobId);
  const valEl = document.getElementById(valId);
  const state = Juce.getSliderState(paramName);

  const render = () => {
    knob.style.setProperty("--pct", state.getNormalisedValue());
    valEl.textContent = fmt(state.getScaledValue());
  };
  state.valueChangedEvent.addListener(render);
  state.propertiesChangedEvent.addListener(render);
  render();

  let dragging = false, startY = 0, startVal = 0;
  knob.addEventListener("mousedown", (e) => {
    dragging = true; startY = e.clientY; startVal = state.getNormalisedValue();
    state.sliderDragStarted(); e.preventDefault();
  });
  window.addEventListener("mousemove", (e) => {
    if (!dragging) return;
    const v = Math.max(0, Math.min(1, startVal + (startY - e.clientY) / 180));
    state.setNormalisedValue(v);
    render();
  });
  window.addEventListener("mouseup", () => {
    if (dragging) { dragging = false; state.sliderDragEnded(); }
  });
}

const fmtDb = (v) => (v > 0 ? "+" : "") + v.toFixed(1) + " dB";
const fmtPct = (v) => Math.round(v * 100) + "%";

bindKnob("k-in", "v-in", "inputGain", fmtDb);
bindKnob("k-out", "v-out", "outputGain", fmtDb);
bindKnob("k-rev", "v-rev", "reverbMix", fmtPct);

// --- toggle IR (WebToggleButtonRelay) ---
(() => {
  const t = document.getElementById("ir-toggle");
  const st = Juce.getToggleState("irOn");
  const render = () => {
    const on = st.getValue();
    t.classList.toggle("on", on);
    t.textContent = on ? "ON" : "OFF";
  };
  st.valueChangedEvent.addListener(render);
  render();
  t.addEventListener("click", () => st.setValue(!st.getValue()));
})();

// --- botones Cambiar (funciones nativas) ---
const loadModel = Juce.getNativeFunction("loadModel");
const loadIR = Juce.getNativeFunction("loadIR");
document.getElementById("amp-change").addEventListener("click", () => loadModel());
document.getElementById("cab-change").addEventListener("click", () => loadIR());

// --- el backend avisa cuando cambia el modelo/IR -> refrescar nombre + foto ---
window.__JUCE__.backend.addEventListener("modelChanged", () => refreshState());

refreshState();
