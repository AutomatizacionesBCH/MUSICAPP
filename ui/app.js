import * as Juce from "./juce/index.js";

const $ = (id) => document.getElementById(id);
const esc = (s) => String(s).replace(/[&<>"]/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[c]));

function refreshState() { refreshChain(); }

// ===== knob compartido (dial con arco + cap con puntero) =====
function setKnob(dial, norm) {
  const a = Math.max(0, Math.min(1, norm));
  dial.style.setProperty("--arc", (a * 270) + "deg");
  const cap = dial.querySelector(".cap");
  if (cap) cap.style.setProperty("--rot", (-135 + a * 270) + "deg");
}

// ===== buscador de tonos (agrupado por equipo) =====
const listModels = Juce.getNativeFunction("listModels");
const loadModelByPath = Juce.getNativeFunction("loadModelByPath");
const loadIR = Juce.getNativeFunction("loadIR");

let activeTab = "";

function archBadge(a, cls) {
  const arch = (a || "").toLowerCase();
  if (!arch) return "";
  return '<span class="' + (cls || "a2") + " " + arch + '">' + arch.toUpperCase() + "</span>";
}

function renderList(groups) {
  const listEl = $("model-list");
  listEl.innerHTML = "";
  groups.forEach((g) => {
    const multi = (g.count || 1) > 1;
    const gear = document.createElement("div");
    gear.className = "gear";
    const row = document.createElement("div");
    row.className = "gearrow";
    row.innerHTML =
      '<div class="gicon">' + (g.ir ? "&#9636;" : "&#9835;") + "</div>" +
      '<div class="gmeta"><div class="gname">' + esc(g.name || "") + "</div>" +
      '<div class="gsub">' + (multi ? "<b>" + g.count + "</b> capturas" : esc(g.gear || "")) + "</div></div>" +
      archBadge(g.arch) +
      (multi ? '<span class="chev">&#9656;</span>' : "");

    let caps = null;
    if (multi) {
      caps = document.createElement("div");
      caps.className = "captures";
      caps.hidden = true;
      (g.captures || []).forEach((c) => {
        const cr = document.createElement("div");
        cr.className = "cap-row";
        cr.innerHTML = '<span class="cap-dot"></span><span class="cap-set">' + esc(c.detail || "(base)") + "</span>" + archBadge(c.arch, "cap-a2");
        cr.addEventListener("click", async (ev) => {
          ev.stopPropagation();
          document.querySelectorAll(".cap-row.sel").forEach((e) => e.classList.remove("sel"));
          cr.classList.add("sel");
          await loadModelByPath(c.path);
          refreshChain();
        });
        caps.appendChild(cr);
      });
    }

    row.addEventListener("click", async () => {
      const opening = !caps || caps.hidden;
      if (opening) { await loadModelByPath(g.defaultPath); refreshChain(); }
      if (caps) { caps.hidden = !caps.hidden; gear.classList.toggle("open"); }
    });

    gear.appendChild(row);
    if (caps) gear.appendChild(caps);
    listEl.appendChild(gear);
  });
  $("model-count").textContent = groups.length + (groups.length === 1 ? " equipo" : " equipos");
}

async function search() {
  try { renderList(await listModels(activeTab, $("search").value || "")); } catch (e) {}
}
let searchTimer;
$("search").addEventListener("input", () => { clearTimeout(searchTimer); searchTimer = setTimeout(search, 150); });
document.querySelectorAll(".tabs .tab").forEach((tab) => {
  tab.addEventListener("click", () => {
    document.querySelectorAll(".tabs .tab.on").forEach((c) => c.classList.remove("on"));
    tab.classList.add("on");
    activeTab = tab.dataset.tab || "";
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
document.addEventListener("click", (e) => { if (!pop.hidden && !e.target.closest(".presetwrap")) pop.hidden = true; });

// ===== IN / OUT (WebSliderRelay) =====
function bindKnob(dialId, valId, paramName, fmt) {
  const dial = $(dialId), valEl = $(valId);
  const state = Juce.getSliderState(paramName);
  const render = () => { setKnob(dial, state.getNormalisedValue()); valEl.textContent = fmt(state.getScaledValue()); };
  state.valueChangedEvent.addListener(render);
  state.propertiesChangedEvent.addListener(render);
  render();
  let dragging = false, startY = 0, startVal = 0;
  dial.addEventListener("mousedown", (e) => {
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
bindKnob("k-in", "v-in", "inputGain", fmtDb);
bindKnob("k-out", "v-out", "outputGain", fmtDb);

// ===== zoom (manual) =====
let uiZoom = 1;
function applyZoom() {
  document.body.style.zoom = uiZoom;
  const l = $("zoom-lbl"); if (l) l.textContent = Math.round(uiZoom * 100) + "%";
}
$("zoom-out").addEventListener("click", () => { uiZoom = Math.max(0.5, Math.round((uiZoom - 0.1) * 10) / 10); applyZoom(); });
$("zoom-in").addEventListener("click", () => { uiZoom = Math.min(1.6, Math.round((uiZoom + 0.1) * 10) / 10); applyZoom(); });

// ===== cadena (Drive / Amp / Cab / FX) =====
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
  const wrap = document.createElement("div"); wrap.className = "knob";
  const dial = document.createElement("div"); dial.className = "dial"; dial.innerHTML = '<div class="cap"></div>';
  const label = document.createElement("div"); label.className = "kl"; label.textContent = p.label.toUpperCase();
  const val = document.createElement("div"); val.className = "kval";
  const norm = (v) => (v - p.min) / (p.max - p.min);
  const render = (v) => { setKnob(dial, norm(v)); val.textContent = fxFmt(p, v); };
  render(p.value);
  dial.addEventListener("mousedown", (e) => { knobDrag = { uid, p, startY: e.clientY, startV: p.value, norm, render }; e.preventDefault(); });
  wrap.append(dial, label, val);
  return wrap;
}

const short = (s, n) => (s && s.length > n ? s.slice(0, n - 1) + "…" : s || "");
function ntagOf(blk) {
  if (blk.kind === "amp" || blk.kind === "cab" || blk.kind === "drive") return blk.kind.toUpperCase();
  return "FX";
}
function pedalColor(blk) {
  if (blk.kind === "drive") return "green";
  if (/reverb|hall|plate|gated|duckrev/.test(blk.kind)) return "blue";
  if (/octaver|pitch|ringmod/.test(blk.kind)) return "red";
  return "silver";
}
function blockTitle(blk) {
  const loaded = blk.extra && blk.extra.loaded;
  if (blk.kind === "amp") return loaded || "(sin amp)";
  if (blk.kind === "cab") return loaded || "(sin IR)";
  if (blk.kind === "drive") return loaded || blk.name;
  return blk.name;
}
function blockSub(blk) {
  if (blk.kind === "amp") return "Amp Head";
  if (blk.kind === "cab") return "Cabinet";
  if (blk.kind === "drive") return "Overdrive";
  return "Efecto";
}

function nodeHead(blk, node) {
  const head = document.createElement("div"); head.className = "nhead";
  const drag = document.createElement("div"); drag.className = "drag"; drag.innerHTML = "<i></i><i></i><i></i>";
  const tag = document.createElement("div"); tag.className = "ntag"; tag.textContent = ntagOf(blk);
  head.append(drag, tag);
  if (blk.kind !== "amp") {
    const byp = document.createElement("div"); byp.className = "bypass " + (blk.bypass ? "off" : "on"); byp.innerHTML = "<i></i>";
    byp.title = blk.bypass ? "Activar" : "Bypass";
    byp.addEventListener("click", async (e) => { e.stopPropagation(); renderChain(await fxBypass(blk.uid, !blk.bypass)); });
    head.append(byp);
  }
  if (blk.removable) {
    const rm = document.createElement("button"); rm.className = "xbtn"; rm.innerHTML = "&#10005;"; rm.title = "Quitar";
    rm.addEventListener("click", async (e) => { e.stopPropagation(); renderChain(await fxRemove(blk.uid)); });
    head.append(rm);
  }
  head.draggable = true;
  head.addEventListener("dragstart", (e) => {
    dragUid = blk.uid; node.classList.add("dragging");
    if (e.dataTransfer) { e.dataTransfer.effectAllowed = "move"; try { e.dataTransfer.setDragImage(node, 30, 20); } catch (_) {} }
  });
  head.addEventListener("dragend", () => {
    dragUid = null; node.classList.remove("dragging");
    fxRack.querySelectorAll(".dragover").forEach((x) => x.classList.remove("dragover"));
  });
  return head;
}

const AMP_KNOBS = [["GAIN", 140], ["BASS", 185], ["MID", 235], ["TREBLE", 170], ["PRES", 210], ["MASTER", 160]];
function ampKnobsHtml() {
  return AMP_KNOBS.map(([l, a]) =>
    '<div class="knob ckhead"><div class="dial" style="--arc:' + a + 'deg"><div class="cap" style="--rot:' + (a - 135) + 'deg"></div></div><div class="kl">' + l + "</div></div>").join("");
}
function ampBody(blk) {
  const loaded = (blk.extra && blk.extra.loaded) || "";
  const words = loaded.split(" ");
  const brand = words[0] || "AMP";
  const sub = words.slice(1).join(" ") || "NAM · A2";
  const amp = document.createElement("div"); amp.className = "amp";
  amp.innerHTML =
    '<span class="corner tl"></span><span class="corner tr"></span><span class="corner bl"></span><span class="corner br"></span>' +
    '<div class="faceplate"><div class="amp-top"><div class="amp-logo">' + esc(brand) + "<small>" + esc(short(sub, 30)) + "</small></div><div class=\"jewel\"></div></div>" +
    '<div class="amp-knobs">' + ampKnobsHtml() + "</div></div>" +
    '<div class="grille"><div class="gbadge">' + (loaded ? "CAMBIAR AMP" : "CARGAR AMP") + "</div></div>";
  amp.querySelector(".grille").addEventListener("click", (e) => { e.stopPropagation(); loadModel(); });
  return amp;
}
function cabBody(blk) {
  const cab = document.createElement("div"); cab.className = "cab"; cab.title = "Cargar IR";
  cab.innerHTML =
    '<span class="corner tl"></span><span class="corner tr"></span><span class="corner bl"></span><span class="corner br"></span>' +
    '<div class="speaker"><div class="dust"></div><div class="mic"><div class="head"></div><div class="body"></div><div class="tag">MIC</div></div></div>';
  cab.addEventListener("click", async (e) => { e.stopPropagation(); await loadIR(); refreshChain(); });
  return cab;
}
function pedalBody(blk) {
  const ped = document.createElement("div"); ped.className = "pedal " + pedalColor(blk);
  const loaded = blk.extra && blk.extra.loaded;
  const face = blk.kind === "drive" && loaded ? short(loaded, 16) : blk.name;
  ped.innerHTML =
    '<span class="screw s1"></span><span class="screw s2"></span><span class="screw s3"></span><span class="screw s4"></span>' +
    '<div class="led ' + (blk.bypass ? "off" : "on") + '"></div>' +
    '<div class="pname">' + esc(face) + '</div><div class="ptag">' + (blk.kind === "drive" ? "OVERDRIVE · NAM" : "EFFECT") + "</div>";
  const knobrow = document.createElement("div"); knobrow.className = "knobrow";
  (blk.params || []).forEach((p) => knobrow.appendChild(makeKnob(blk.uid, p)));
  ped.appendChild(knobrow);
  const stomp = document.createElement("div"); stomp.className = "stomp"; stomp.title = blk.bypass ? "Activar" : "Bypass";
  stomp.addEventListener("click", async (e) => { e.stopPropagation(); renderChain(await fxBypass(blk.uid, !blk.bypass)); });
  ped.appendChild(stomp);
  if (blk.kind === "drive") {
    const btn = document.createElement("button"); btn.className = "loadbtn";
    btn.textContent = loaded ? "Cambiar pedal" : "Cargar pedal";
    btn.addEventListener("click", (e) => { e.stopPropagation(); loadPedal(blk.uid); });
    ped.appendChild(btn);
  }
  return ped;
}
function nodeBody(blk) {
  if (blk.kind === "amp") return ampBody(blk);
  if (blk.kind === "cab") return cabBody(blk);
  return pedalBody(blk);
}

let dragUid = null;
function renderChain(chain) {
  if (!Array.isArray(chain)) return;
  fxRack.innerHTML = "";
  chain.forEach((blk, idx) => {
    const node = document.createElement("div");
    node.className = "node k-" + blk.kind + (blk.bypass ? " byp" : "");
    node.appendChild(nodeHead(blk, node));
    node.appendChild(nodeBody(blk));
    const lab = document.createElement("div"); lab.className = "nlabel";
    lab.innerHTML = '<div class="t">' + esc(blockTitle(blk)) + '</div><div class="s">' + esc(blockSub(blk)) + "</div>";
    node.appendChild(lab);

    node.addEventListener("dragover", (e) => { e.preventDefault(); if (dragUid != null && dragUid !== blk.uid) node.classList.add("dragover"); });
    node.addEventListener("dragleave", () => node.classList.remove("dragover"));
    node.addEventListener("drop", async (e) => {
      e.preventDefault(); node.classList.remove("dragover");
      if (dragUid != null && dragUid !== blk.uid) renderChain(await fxMove(dragUid, idx));
    });

    fxRack.appendChild(node);
    if (idx < chain.length - 1) { const g = document.createElement("div"); g.className = "gap"; fxRack.appendChild(g); }
  });
}
async function refreshChain() { try { renderChain(await fxGetChain()); } catch (e) {} }

// menú "+ FX"
$("fx-add-btn").addEventListener("click", async (e) => {
  e.stopPropagation();
  if (!fxMenu.hidden) { fxMenu.hidden = true; return; }
  let types = [];
  try { types = await fxTypes(); } catch (e2) {}
  const cats = {};
  types.forEach((t) => { (cats[t.category] = cats[t.category] || []).push(t); });
  fxMenu.innerHTML = "";
  Object.keys(cats).forEach((cat) => {
    const h = document.createElement("div"); h.className = "fxmcat"; h.textContent = cat; fxMenu.appendChild(h);
    cats[cat].forEach((t) => {
      const it = document.createElement("div"); it.className = "fxmitem"; it.textContent = t.name;
      it.addEventListener("click", async () => { fxMenu.hidden = true; renderChain(await fxAdd(t.type)); });
      fxMenu.appendChild(it);
    });
  });
  fxMenu.hidden = false;
});
document.addEventListener("click", (e) => { if (!fxMenu.hidden && !e.target.closest(".fxaddwrap")) fxMenu.hidden = true; });

// ===== medidores (ladder) + afinador =====
const NOTES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"];
const LAD_N = 14;
function buildLadder(id) {
  const el = $(id); if (!el) return;
  el.innerHTML = "";
  for (let i = 0; i < LAD_N; i++) el.appendChild(document.createElement("i"));
}
function setMeter(id, level) {
  const el = $(id); if (!el) return;
  const lit = Math.round(Math.max(0, Math.min(1, level)) * LAD_N);
  const segs = el.children;
  for (let i = 0; i < segs.length; i++)
    segs[i].className = i < lit ? (i >= LAD_N - 2 ? "r" : i >= LAD_N - 5 ? "y" : "g") : "";
}
buildLadder("in-ladder");
buildLadder("out-ladder");

function updateTuner(hz) {
  const noteEl = $("t-note"), needle = $("t-needle"), hzEl = $("t-hz");
  if (!hz || hz < 20) { noteEl.textContent = "—"; needle.style.left = "50%"; needle.style.background = "#3a342c"; hzEl.textContent = ""; return; }
  const midi = 69 + 12 * Math.log2(hz / 440), nearest = Math.round(midi), cents = (midi - nearest) * 100;
  noteEl.innerHTML = NOTES[((nearest % 12) + 12) % 12] + '<span style="font-size:14px;color:#9b8a6c">' + (Math.floor(nearest / 12) - 1) + "</span>";
  needle.style.left = Math.max(2, Math.min(98, 50 + cents)) + "%";
  needle.style.background = Math.abs(cents) < 5 ? "#7df0a0" : "var(--warm)";
  hzEl.innerHTML = '<b style="color:#cdbf9f">' + hz.toFixed(1) + "</b> Hz · " + (cents >= 0 ? "+" : "") + cents.toFixed(0) + "¢";
}
window.__JUCE__.backend.addEventListener("meters", (m) => {
  setMeter("in-ladder", m.in); setMeter("out-ladder", m.out); updateTuner(m.hz);
});

// ===== refrescos =====
window.__JUCE__.backend.addEventListener("modelChanged", () => refreshChain());
refreshChain();
search();
