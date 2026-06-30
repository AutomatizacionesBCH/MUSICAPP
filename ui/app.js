import * as Juce from "./juce/index.js";

const $ = (id) => document.getElementById(id);
const esc = (s) => String(s).replace(/[&<>"]/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[c]));

// ===== estado (nombres + fotos) =====
async function refreshState() {
  try {
    const s = await (await fetch("state.json?_=" + Date.now())).json();
    $("amp-name").textContent = s.model || "(ninguno)";
    $("cab-name").textContent = s.ir || "(ninguno)";
    $("amp-photo").src = "amp-photo?_=" + Date.now();
    $("cab-photo").src = "cab-photo?_=" + Date.now();
  } catch (e) {}
}

// ===== buscador de tonos (panel izquierdo) =====
const listModels = Juce.getNativeFunction("listModels");
const loadModelByPath = Juce.getNativeFunction("loadModelByPath");
const loadIR = Juce.getNativeFunction("loadIR");

function renderList(items) {
  const listEl = $("model-list");
  listEl.innerHTML = "";
  items.forEach((it) => {
    const parts = it.label.split("/");
    const file = parts[parts.length - 1];
    const name = (file.split("__")[0] || file).replace(/\.nam$/, "");
    const sub = parts.slice(0, -1).join(" / ");
    const row = document.createElement("div");
    row.className = "mitem";
    row.innerHTML =
      '<div class="mthumb">&#9835;</div><div class="minfo">' +
      '<div class="mname">' + esc(name) + "</div>" +
      '<div class="msub">' + esc(sub) + "</div></div>";
    row.addEventListener("click", async () => {
      document.querySelectorAll(".mitem.sel").forEach((e) => e.classList.remove("sel"));
      row.classList.add("sel");
      await loadModelByPath(it.path);
      refreshState();
    });
    listEl.appendChild(row);
  });
  $("model-count").textContent = items.length + " modelos";
}

async function search(q) {
  try { renderList(await listModels(q || "")); } catch (e) {}
}

let searchTimer;
$("search").addEventListener("input", (e) => {
  clearTimeout(searchTimer);
  searchTimer = setTimeout(() => search(e.target.value), 150);
});
$("cab-change").addEventListener("click", () => loadIR());

// ===== presets =====
const savePreset = Juce.getNativeFunction("savePreset");
const listPresets = Juce.getNativeFunction("listPresets");
const loadPreset = Juce.getNativeFunction("loadPreset");
const presetName = $("preset-name");
const pop = $("presets-pop");

$("btn-save").addEventListener("click", () => savePreset(presetName.value || "preset"));

$("btn-presets").addEventListener("click", async (e) => {
  e.stopPropagation();
  if (!pop.hidden) { pop.hidden = true; return; }
  let items = [];
  try { items = await listPresets(); } catch (e2) {}
  pop.innerHTML = items.length ? "" : '<div class="ppempty">(sin presets guardados)</div>';
  items.forEach((it) => {
    const row = document.createElement("div");
    row.className = "ppitem";
    row.textContent = it.name;
    row.addEventListener("click", async () => {
      const r = await loadPreset(it.path);
      if (r && r.name) presetName.value = r.name;
      pop.hidden = true;
      refreshState();
    });
    pop.appendChild(row);
  });
  pop.hidden = false;
});
document.addEventListener("click", (e) => {
  if (!pop.hidden && !e.target.closest(".presetwrap")) pop.hidden = true;
});

// ===== knobs (WebSliderRelay) =====
function bindKnob(knobId, valId, paramName, fmt) {
  const knob = $(knobId), valEl = $(valId);
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
    state.setNormalisedValue(Math.max(0, Math.min(1, startVal + (startY - e.clientY) / 180)));
    render();
  });
  window.addEventListener("mouseup", () => { if (dragging) { dragging = false; state.sliderDragEnded(); } });
}
const fmtDb = (v) => (v > 0 ? "+" : "") + v.toFixed(1) + " dB";
const fmtPct = (v) => Math.round(v * 100) + "%";
bindKnob("k-in", "v-in", "inputGain", fmtDb);
bindKnob("k-out", "v-out", "outputGain", fmtDb);
bindKnob("k-drv", "v-drv", "driveAmount", fmtPct);
bindKnob("k-lvl", "v-lvl", "driveLevel", fmtPct);

// ===== toggles (WebToggleButtonRelay) =====
function bindToggle(id, paramName) {
  const t = $(id), st = Juce.getToggleState(paramName);
  const render = () => { const on = st.getValue(); t.classList.toggle("on", on); t.textContent = on ? "ON" : "OFF"; };
  st.valueChangedEvent.addListener(render);
  render();
  t.addEventListener("click", () => st.setValue(!st.getValue()));
}
bindToggle("ir-toggle", "irOn");
bindToggle("drive-toggle", "driveOn");

// ===== rack flexible de efectos =====
const fxTypes = Juce.getNativeFunction("fxTypes");
const fxGetChain = Juce.getNativeFunction("fxGetChain");
const fxAdd = Juce.getNativeFunction("fxAdd");
const fxRemove = Juce.getNativeFunction("fxRemove");
const fxMove = Juce.getNativeFunction("fxMove");
const fxBypass = Juce.getNativeFunction("fxBypass");
const fxSetParam = Juce.getNativeFunction("fxSetParam");
const fxRack = $("fx-rack");
const fxMenu = $("fx-menu");

function fxFmt(p, v) {
  if (p.unit === "Hz") return v.toFixed(p.max <= 20 ? 2 : 0) + " Hz";
  if (p.unit === "ms") return Math.round(v) + " ms";
  if (p.unit === "%") return Math.round(((v - p.min) / (p.max - p.min)) * 100) + "%";
  return v.toFixed(2);
}

// arrastre global (un solo set de listeners para todas las perillas del rack)
let fxDrag = null;
window.addEventListener("mousemove", (e) => {
  if (!fxDrag) return;
  let n = fxDrag.norm(fxDrag.startV) + (fxDrag.startY - e.clientY) / 180;
  n = Math.max(0, Math.min(1, n));
  const v = fxDrag.p.min + n * (fxDrag.p.max - fxDrag.p.min);
  fxDrag.p.value = v;
  fxDrag.render(v);
  fxSetParam(fxDrag.uid, fxDrag.p.id, v);
});
window.addEventListener("mouseup", () => { fxDrag = null; });

function makeFxKnob(uid, p) {
  const wrap = document.createElement("div");
  wrap.className = "kwrap";
  const knob = document.createElement("div");
  knob.className = "knob";
  const label = document.createElement("div");
  label.className = "klabel";
  label.textContent = p.label.toUpperCase();
  const val = document.createElement("div");
  val.className = "kval";
  const norm = (v) => (v - p.min) / (p.max - p.min);
  const render = (v) => { knob.style.setProperty("--pct", norm(v)); val.textContent = fxFmt(p, v); };
  render(p.value);
  knob.addEventListener("mousedown", (e) => {
    fxDrag = { uid, p, startY: e.clientY, startV: p.value, norm, render };
    e.preventDefault();
  });
  wrap.appendChild(knob); wrap.appendChild(label); wrap.appendChild(val);
  return wrap;
}

function renderChain(chain) {
  if (!Array.isArray(chain)) return;
  fxRack.innerHTML = "";
  chain.forEach((blk, idx) => {
    const sec = document.createElement("section");
    sec.className = "block fxblock" + (blk.bypass ? " byp" : "");
    const head = document.createElement("div");
    head.className = "fxhead";
    head.innerHTML =
      '<button class="fxbtn" title="Mover izquierda">&#9664;</button>' +
      '<span class="fxnm">' + esc(blk.name) + "</span>" +
      '<button class="fxbtn' + (blk.bypass ? "" : " on") + '" title="Bypass">&#9211;</button>' +
      '<button class="fxbtn" title="Mover derecha">&#9654;</button>' +
      '<button class="fxbtn" title="Quitar">&#10005;</button>';
    const btns = head.querySelectorAll("button");
    btns[0].addEventListener("click", async () => renderChain(await fxMove(blk.uid, idx - 1)));
    btns[1].addEventListener("click", async () => renderChain(await fxBypass(blk.uid, !blk.bypass)));
    btns[2].addEventListener("click", async () => renderChain(await fxMove(blk.uid, idx + 1)));
    btns[3].addEventListener("click", async () => renderChain(await fxRemove(blk.uid)));
    sec.appendChild(head);
    blk.params.forEach((p) => sec.appendChild(makeFxKnob(blk.uid, p)));
    fxRack.appendChild(sec);
    if (idx < chain.length - 1) {
      const ar = document.createElement("div");
      ar.className = "arrow";
      fxRack.appendChild(ar);
    }
  });
}

async function refreshChain() { try { renderChain(await fxGetChain()); } catch (e) {} }

$("fx-add-btn").addEventListener("click", async (e) => {
  e.stopPropagation();
  if (!fxMenu.hidden) { fxMenu.hidden = true; return; }
  let types = [];
  try { types = await fxTypes(); } catch (e2) {}
  fxMenu.innerHTML = "";
  types.forEach((t) => {
    const it = document.createElement("div");
    it.className = "fxmitem";
    it.textContent = t.name;
    it.addEventListener("click", async () => { fxMenu.hidden = true; renderChain(await fxAdd(t.type)); });
    fxMenu.appendChild(it);
  });
  fxMenu.hidden = false;
});
document.addEventListener("click", (e) => {
  if (!fxMenu.hidden && !e.target.closest(".fxaddwrap")) fxMenu.hidden = true;
});
refreshChain();

// ===== medidores + afinador (evento "meters") =====
const NOTES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"];
function setMeter(id, level) {
  const el = $(id), lv = Math.max(0, Math.min(1, level));
  el.style.width = lv * 100 + "%";
  el.style.background = lv > 0.92 ? "var(--red)" : lv > 0.72 ? "var(--amber)" : "var(--green)";
}
function updateTuner(hz) {
  const noteEl = $("t-note"), needle = $("t-needle"), hzEl = $("t-hz");
  if (!hz || hz < 20) { noteEl.textContent = "—"; needle.style.left = "50%"; needle.style.background = "var(--dim)"; hzEl.textContent = ""; return; }
  const midi = 69 + 12 * Math.log2(hz / 440), nearest = Math.round(midi), cents = (midi - nearest) * 100;
  noteEl.textContent = NOTES[((nearest % 12) + 12) % 12] + (Math.floor(nearest / 12) - 1);
  needle.style.left = Math.max(0, Math.min(100, 50 + cents)) + "%";
  needle.style.background = Math.abs(cents) < 5 ? "var(--green)" : "var(--accent)";
  hzEl.textContent = hz.toFixed(1) + " Hz";
}
window.__JUCE__.backend.addEventListener("meters", (m) => {
  setMeter("in-fill", m.in); setMeter("out-fill", m.out); updateTuner(m.hz);
});

// ===== refrescos =====
window.__JUCE__.backend.addEventListener("modelChanged", () => refreshState());
refreshState();
search("");
