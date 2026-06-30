import * as Juce from "./juce/index.js";

const $ = (id) => document.getElementById(id);
const esc = (s) => String(s).replace(/[&<>"]/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[c]));

// ===== estado: re-renderiza la cadena (amp/cab muestran su nombre+foto) =====
function refreshState() { refreshChain(); }

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

// ===== toggles (WebToggleButtonRelay) =====
function bindToggle(id, paramName) {
  const t = $(id), st = Juce.getToggleState(paramName);
  const render = () => { const on = st.getValue(); t.classList.toggle("on", on); t.textContent = on ? "ON" : "OFF"; };
  st.valueChangedEvent.addListener(render);
  render();
  t.addEventListener("click", () => st.setValue(!st.getValue()));
}

// ===== rack flexible: TODA la cadena (Drive/Amp/Cab + efectos), arrastrable =====
const loadModel = Juce.getNativeFunction("loadModel");
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

// arrastre de perillas (un solo set de listeners global)
let knobDrag = null;
window.addEventListener("mousemove", (e) => {
  if (!knobDrag) return;
  let nrm = knobDrag.norm(knobDrag.startV) + (knobDrag.startY - e.clientY) / 180;
  nrm = Math.max(0, Math.min(1, nrm));
  const v = knobDrag.p.min + nrm * (knobDrag.p.max - knobDrag.p.min);
  knobDrag.p.value = v;
  knobDrag.render(v);
  fxSetParam(knobDrag.uid, knobDrag.p.id, v);
});
window.addEventListener("mouseup", () => { knobDrag = null; });

function makeKnob(uid, p) {
  const wrap = document.createElement("div"); wrap.className = "kwrap";
  const knob = document.createElement("div"); knob.className = "knob";
  const label = document.createElement("div"); label.className = "klabel"; label.textContent = p.label.toUpperCase();
  const val = document.createElement("div"); val.className = "kval";
  const norm = (v) => (v - p.min) / (p.max - p.min);
  const render = (v) => { knob.style.setProperty("--pct", norm(v)); val.textContent = fxFmt(p, v); };
  render(p.value);
  knob.addEventListener("mousedown", (e) => { knobDrag = { uid, p, startY: e.clientY, startV: p.value, norm, render }; e.preventDefault(); });
  wrap.append(knob, label, val);
  return wrap;
}

function blockHeader(blk) {
  const head = document.createElement("div"); head.className = "fxhead";
  const grip = document.createElement("span"); grip.className = "fxgrip"; grip.innerHTML = "&#9776;"; grip.title = "Arrastra para mover";
  const nm = document.createElement("span"); nm.className = "fxnm"; nm.textContent = blk.name;
  head.append(grip, nm);
  if (blk.kind !== "amp") {
    const byp = document.createElement("button");
    byp.className = "fxbtn" + (blk.bypass ? "" : " on"); byp.innerHTML = "&#9211;"; byp.title = blk.bypass ? "Activar" : "Bypass";
    byp.addEventListener("click", async (e) => { e.stopPropagation(); renderChain(await fxBypass(blk.uid, !blk.bypass)); });
    head.append(byp);
  }
  if (blk.removable) {
    const rm = document.createElement("button");
    rm.className = "fxbtn"; rm.innerHTML = "&#10005;"; rm.title = "Quitar";
    rm.addEventListener("click", async (e) => { e.stopPropagation(); renderChain(await fxRemove(blk.uid)); });
    head.append(rm);
  }
  return head;
}

function blockBody(sec, blk) {
  if (blk.kind === "amp") {
    const loaded = document.createElement("div"); loaded.className = "loaded";
    loaded.innerHTML = '<div class="lcap">MODELO CARGADO</div><div class="lname"></div>';
    loaded.querySelector(".lname").textContent = (blk.extra && blk.extra.loaded) || "(ninguno)";
    const photo = document.createElement("div"); photo.className = "photo";
    photo.innerHTML = '<img src="amp-photo?_=' + Date.now() + '" alt="">';
    const btn = document.createElement("button"); btn.className = "change"; btn.textContent = "Cargar modelo";
    btn.addEventListener("click", (e) => { e.stopPropagation(); loadModel(); });
    sec.append(loaded, photo, btn);
  } else if (blk.kind === "cab") {
    const photo = document.createElement("div"); photo.className = "photo";
    photo.innerHTML = '<img src="cab-photo?_=' + Date.now() + '" alt="">';
    const nm = document.createElement("div"); nm.className = "lname small";
    nm.textContent = (blk.extra && blk.extra.loaded) || "(ninguno)";
    const btn = document.createElement("button"); btn.className = "change"; btn.textContent = "Cargar IR";
    btn.addEventListener("click", async (e) => { e.stopPropagation(); await loadIR(); refreshChain(); });
    sec.append(photo, nm, btn);
  } else {
    (blk.params || []).forEach((p) => sec.appendChild(makeKnob(blk.uid, p)));
  }
}

// drag-and-drop de bloques (se arrastra por el header; los knobs quedan libres)
let dragUid = null;
function renderChain(chain) {
  if (!Array.isArray(chain)) return;
  fxRack.innerHTML = "";
  chain.forEach((blk, idx) => {
    const sec = document.createElement("section");
    sec.className = "block fxblock k-" + blk.kind + (blk.bypass ? " byp" : "");
    const head = blockHeader(blk);
    head.draggable = true;
    head.addEventListener("dragstart", (e) => {
      dragUid = blk.uid; sec.classList.add("dragging");
      if (e.dataTransfer) { e.dataTransfer.effectAllowed = "move"; try { e.dataTransfer.setDragImage(sec, 20, 20); } catch (_) {} }
    });
    head.addEventListener("dragend", () => {
      dragUid = null; sec.classList.remove("dragging");
      fxRack.querySelectorAll(".dragover").forEach((x) => x.classList.remove("dragover"));
    });
    sec.addEventListener("dragover", (e) => { e.preventDefault(); if (dragUid != null && dragUid !== blk.uid) sec.classList.add("dragover"); });
    sec.addEventListener("dragleave", () => sec.classList.remove("dragover"));
    sec.addEventListener("drop", async (e) => {
      e.preventDefault(); sec.classList.remove("dragover");
      if (dragUid != null && dragUid !== blk.uid) renderChain(await fxMove(dragUid, idx));
    });
    sec.appendChild(head);
    blockBody(sec, blk);
    fxRack.appendChild(sec);
    if (idx < chain.length - 1) { const ar = document.createElement("div"); ar.className = "arrow"; fxRack.appendChild(ar); }
  });
}

async function refreshChain() { try { renderChain(await fxGetChain()); } catch (e) {} }

// menú "+ Efecto" agrupado por categorías
$("fx-add-btn").addEventListener("click", async (e) => {
  e.stopPropagation();
  if (!fxMenu.hidden) { fxMenu.hidden = true; return; }
  let types = [];
  try { types = await fxTypes(); } catch (e2) {}
  const cats = {};
  types.forEach((t) => { (cats[t.category] = cats[t.category] || []).push(t); });
  fxMenu.innerHTML = "";
  Object.keys(cats).forEach((cat) => {
    const h = document.createElement("div"); h.className = "fxmcat"; h.textContent = cat;
    fxMenu.appendChild(h);
    cats[cat].forEach((t) => {
      const it = document.createElement("div"); it.className = "fxmitem"; it.textContent = t.name;
      it.addEventListener("click", async () => { fxMenu.hidden = true; renderChain(await fxAdd(t.type)); });
      fxMenu.appendChild(it);
    });
  });
  fxMenu.hidden = false;
});
document.addEventListener("click", (e) => { if (!fxMenu.hidden && !e.target.closest(".fxaddwrap")) fxMenu.hidden = true; });
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
