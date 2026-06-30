import * as Juce from "./juce/index.js";

const $ = (id) => document.getElementById(id);
const esc = (s) => String(s).replace(/[&<>"]/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[c]));


// ===== estado: re-renderiza la cadena (amp/cab muestran su nombre+foto) =====
function refreshState() { refreshChain(); }

// ===== buscador de tonos (panel izquierdo) =====
const listModels = Juce.getNativeFunction("listModels");
const loadModelByPath = Juce.getNativeFunction("loadModelByPath");
const loadIR = Juce.getNativeFunction("loadIR");

let activeTab = "";   // "" = todos | "amp" | "amp-cab" | "pedal" | "ir"

function archBadge(a) {
  const arch = (a || "").toLowerCase();
  return arch ? '<span class="arch ' + arch + '">' + arch.toUpperCase() + "</span>" : "";
}

// Cada grupo = un equipo (amp/cab/pedal) con TODAS sus capturas.
// Click en el equipo -> carga una captura por defecto (neutral) y despliega el submenú.
function renderList(groups) {
  const listEl = $("model-list");
  listEl.innerHTML = "";
  groups.forEach((g) => {
    const multi = (g.count || 1) > 1;

    const row = document.createElement("div");
    row.className = "mitem mgroup";
    row.innerHTML =
      '<div class="mthumb">' + (g.ir ? "&#9636;" : "&#9835;") + '</div><div class="minfo">' +
      '<div class="mname">' + esc(g.name || "") + "</div>" +
      '<div class="msub">' + esc(g.gear || "") + (multi ? " &middot; " + g.count + " capturas" : "") + "</div></div>" +
      archBadge(g.arch) +
      (multi ? '<span class="gchev">&#9656;</span>' : "");

    let sub = null;
    if (multi) {
      sub = document.createElement("div");
      sub.className = "gsub";
      sub.hidden = true;
      (g.captures || []).forEach((c) => {
        const cr = document.createElement("div");
        cr.className = "citem";
        cr.innerHTML = '<div class="cdetail">' + esc(c.detail || "(base)") + "</div>" + archBadge(c.arch);
        cr.addEventListener("click", async (ev) => {
          ev.stopPropagation();
          document.querySelectorAll(".citem.sel").forEach((e) => e.classList.remove("sel"));
          cr.classList.add("sel");
          await loadModelByPath(c.path);
          refreshChain();
        });
        sub.appendChild(cr);
      });
    }

    row.addEventListener("click", async () => {
      document.querySelectorAll(".mitem.sel").forEach((e) => e.classList.remove("sel"));
      row.classList.add("sel");
      const opening = !sub || sub.hidden;
      if (opening) { await loadModelByPath(g.defaultPath); refreshChain(); }
      if (sub) { sub.hidden = !sub.hidden; row.classList.toggle("open"); }
    });

    listEl.appendChild(row);
    if (sub) listEl.appendChild(sub);
  });
  $("model-count").textContent = groups.length + (groups.length === 1 ? " equipo" : " equipos");
}

async function search() {
  try { renderList(await listModels(activeTab, $("search").value || "")); } catch (e) {}
}

let searchTimer;
$("search").addEventListener("input", () => {
  clearTimeout(searchTimer);
  searchTimer = setTimeout(search, 150);
});
document.querySelectorAll(".chips .chip").forEach((chip) => {
  chip.addEventListener("click", () => {
    document.querySelectorAll(".chips .chip.on").forEach((c) => c.classList.remove("on"));
    chip.classList.add("on");
    activeTab = chip.dataset.tab || "";
    search();
  });
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

// ===== zoom de la interfaz (escala todo) =====
let uiZoom = 1;
function applyZoom() {
  document.body.style.zoom = uiZoom;
  const l = $("zoom-lbl");
  if (l) l.textContent = Math.round(uiZoom * 100) + "%";
}
$("zoom-out").addEventListener("click", () => { uiZoom = Math.max(0.5, Math.round((uiZoom - 0.1) * 10) / 10); applyZoom(); });
$("zoom-in").addEventListener("click", () => { uiZoom = Math.min(1.6, Math.round((uiZoom + 0.1) * 10) / 10); applyZoom(); });

// ===== rack flexible: TODA la cadena (Drive/Amp/Cab + efectos), arrastrable =====
const loadModel = Juce.getNativeFunction("loadModel");
const loadPedal = Juce.getNativeFunction("loadPedal");
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
    const knobs = document.createElement("div"); knobs.className = "knobs";
    (blk.params || []).forEach((p) => knobs.appendChild(makeKnob(blk.uid, p)));
    sec.appendChild(knobs);
    if (blk.kind === "drive") {
      const loaded = (blk.extra && blk.extra.loaded) || "";
      if (loaded) {
        const nm = document.createElement("div"); nm.className = "lname small"; nm.textContent = loaded;
        sec.appendChild(nm);
      }
      const btn = document.createElement("button"); btn.className = "change";
      btn.textContent = loaded ? "Cambiar pedal" : "Cargar pedal";
      btn.addEventListener("click", (e) => { e.stopPropagation(); loadPedal(blk.uid); });
      sec.appendChild(btn);
    }
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
window.__JUCE__.backend.addEventListener("modelChanged", () => refreshChain());
refreshChain();
search();
