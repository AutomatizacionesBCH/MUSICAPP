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

// --- medidores IN/OUT + afinador (evento "meters" del backend ~24 Hz) ---
const NOTES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"];

function setMeter(id, level) {
  const el = document.getElementById(id);
  const lv = Math.max(0, Math.min(1, level));
  el.style.width = lv * 100 + "%";
  el.style.background = lv > 0.92 ? "var(--red)" : lv > 0.72 ? "var(--amber)" : "var(--green)";
}

function updateTuner(hz) {
  const noteEl = document.getElementById("t-note");
  const needle = document.getElementById("t-needle");
  const hzEl = document.getElementById("t-hz");
  if (!hz || hz < 20) {
    noteEl.textContent = "—";
    needle.style.left = "50%";
    needle.style.background = "var(--dim)";
    hzEl.textContent = "";
    return;
  }
  const midi = 69 + 12 * Math.log2(hz / 440);
  const nearest = Math.round(midi);
  const cents = (midi - nearest) * 100;
  noteEl.textContent = NOTES[((nearest % 12) + 12) % 12] + (Math.floor(nearest / 12) - 1);
  needle.style.left = Math.max(0, Math.min(100, 50 + cents)) + "%";
  needle.style.background = Math.abs(cents) < 5 ? "var(--green)" : "var(--accent)";
  hzEl.textContent = hz.toFixed(1) + " Hz";
}

window.__JUCE__.backend.addEventListener("meters", (m) => {
  setMeter("in-fill", m.in);
  setMeter("out-fill", m.out);
  updateTuner(m.hz);
});
