# Catálogo de efectos — roadmap (multiefecto estilo Helix Native)

Lista completa de efectos a incorporar al **rack flexible** (todo lo que NO es Drive/Amp/Cab).
Investigado contra el inventario real de Line 6 Helix/Helix Native (fluidsolo.com, helixhelp.com,
manual oficial). **76 tipos** que cubren ~140 modelos de Helix.

**Estado:** ✅ listo · 🔨 en progreso · ⏳ pendiente
**Dificultad:** 🟢 fácil (1-2, casi built-in) · 🟡 moderado (3) · 🔴 difícil (4-5, research-grade)

> Drive/Amp/Cab quedan como anclas (NAM/IR). Los de abajo son bloques del **rack flexible**
> (agregar/quitar/mover). Los 🔴 (pitch polifónico, shimmer, particle, spring auténtico) van al final.

## Arquitectura
`IN → Drive → Amp(NAM) → Cab(IR) → [RACK FLEXIBLE: N bloques reordenables] → OUT`
- `FxBlock` (interfaz) · `FxChain` (rack, ScopedTryLock RT-safe) · UI dinámica (sin relays fijos).

---

## 🌀 Modulación (22)
| Efecto | Cubre | Dif | Estado |
|---|---|---|---|
| Chorus estándar | Chorus, CE-1, Liquifier | 🟢 | ✅ |
| Chorus multi-voz | Tri/Trinity, Dimension D | 🟢 | ⏳ |
| Flanger | MXR, Electric Mistress | 🟡 | ✅ |
| Flanger through-zero | Jet/80A A/DA | 🟡 | ⏳ |
| Phaser clásico | Phase 90, Small Stone | 🟢 | ✅ |
| Phaser multi-etapa/dual | Bi-Phase, Flying Pan | 🟡 | ⏳ |
| Barberpole Phaser | Barberpole | 🔴 | ⏳ |
| Tremolo óptico | Optical/Opto Trem | 🟢 | ✅ |
| Tremolo bias | 60s Bias Trem | 🟡 | ⏳ |
| Tremolo armónico | Harmonic Tremolo | 🟡 | ⏳ |
| Tremolo rítmico | Bleat Chop, Pattern | 🟡 | ⏳ |
| Auto-Pan | Panner, Autopan | 🟢 | ⏳ (estéreo) |
| Uni-Vibe | Ubiquitous/U-Vibe | 🟡 | ⏳ |
| Vibrato (pitch real) | VB-2, FlexoVibe | 🟡 | ⏳ |
| Rotary / Leslie | 122/145, Vibratone | 🔴 | ⏳ |
| Ring Mod | Ring Mod, AM Ring Mod | 🟡 | ⏳ |
| Pitch Ring Mod | Pitch Ring Mod | 🔴 | ⏳ |
| Frequency Shifter | Frequency Shift | 🔴 | ⏳ |
| Poly Detune / Doubler | Poly Detune, Double Take | 🟡 | ⏳ |
| Random S&H | Random S&H | 🟡 | ⏳ |
| Filter Sweep (LFO-wah) | Sweeper, Warble-Matic | 🟡 | ⏳ |
| Tape Wow/Flutter | Retro Reel | 🟡 | ⏳ |

## 🔁 Delay (21)
| Efecto | Cubre | Dif | Estado |
|---|---|---|---|
| Digital (limpio) | Simple, Digital, Dual, Lo-Res | 🟢 | ✅ |
| Tape Echo | Echoplex EP-1/EP-3, Tube Echo | 🟡 | ⏳ |
| Space Echo | Cosmos, Multi-Head | 🟡 | ⏳ |
| Analog/BBD | Bucket Brigade, Memory Man | 🟡 | ⏳ |
| Modulado / Chorus Echo | Mod/Chorus Echo, Phaze Eko | 🟢 | ⏳ |
| Ping-Pong | Ping Pong | 🟡 | ⏳ (estéreo) |
| Multi-Tap | Multitap 4/6, Multi Pass | 🟡 | ⏳ |
| Sweep Echo | Sweep Echo, Bubble | 🟡 | ⏳ |
| Reverse | Reverse Delay | 🟡 | ⏳ |
| Ducking / Dynamic | Ducked, Dynamic (TC 2290) | 🟡 | ⏳ |
| Pitch Echo | Pitch Echo | 🔴 | ⏳ |
| Harmony Delay | Harmony Delay | 🔴 | ⏳ |
| Poly Sustain | Poly Sustain | 🔴 | ⏳ |
| Swell / ADT | Vintage Swell, ADT | 🟡 | ⏳ |
| Glitch | Glitch Delay | 🔴 | ⏳ |
| Euclidean | Euclidean Delay | 🟡 | ⏳ |
| Ratchet | Ratchet | 🟡 | ⏳ |
| Tesselator | Tesselator | 🔴 | ⏳ |
| Crisscross | Crisscross | 🟡 | ⏳ (estéreo) |
| Heliosphere | Heliosphere | 🟡 | ⏳ |
| Echo Platter | Binson EchoRec | 🟡 | ⏳ |

## 🎚 Pitch / Synth (14)
| Efecto | Cubre | Dif | Estado |
|---|---|---|---|
| Octaver (analógico) | Boctaver, Bass Octaver | 🟡 | ⏳ |
| Ring Mod | Saturn 5 | 🟢 | ⏳ |
| Mono Synth (sub/oct) | Seismik, Octi, Double Bass | 🟡 | ⏳ |
| Synth resonante | Rez, Growler, Synth Lead | 🟡 | ⏳ |
| Synth wavetable/strings | Buzz Wave, Synth String | 🟡 | ⏳ |
| Multi-osc generator | 3-OSC, 4-OSC Generator | 🟡 | ⏳ |
| Mono Pitch shifter | Simple Pitch | 🟡 | ⏳ |
| Dual Pitch | Dual Pitch | 🟡 | ⏳ |
| Pitch Wham (Whammy) | Pitch Wham | 🟡 | ⏳ |
| Smart Harmony | Smart Harmony | 🔴 | ⏳ |
| Twin Harmony | Twin Harmony | 🔴 | ⏳ |
| 12-String | 12 String | 🔴 | ⏳ |
| Poly Pitch / Capo / Wham | Poly Pitch/Capo/Wham | 🔴 | ⏳ (phase vocoder) |

## 🏛 Reverb (19)
| Efecto | Cubre | Dif | Estado |
|---|---|---|---|
| Room | Room, Dynamic Room | 🟢 | ✅ |
| Chamber | Chamber | 🟢 | ⏳ |
| Hall | Hall, Dynamic Hall | 🟡 | ⏳ |
| Plate | Plate, Double Tank | 🟡 | ⏳ (Dattorro) |
| Tile | Tile | 🟢 | ⏳ |
| Cave | Cave | 🟡 | ⏳ |
| Echo Reverb | Echo | 🟢 | ⏳ |
| Spring | '63 Spring, Hot Springs | 🔴 | ⏳ |
| Ducking | Ducking | 🟡 | ⏳ |
| Nonlinear / Gated | Nonlinear | 🟡 | ⏳ |
| Octo (armonizado) | Octo | 🔴 | ⏳ |
| Shimmer | Shimmer | 🔴 | ⏳ |
| Particle | Particle Verb | 🔴 | ⏳ |
| Glitz | Glitz (BigSky Bloom) | 🔴 | ⏳ |
| Searchlights | Searchlights (BigSky Cloud) | 🔴 | ⏳ |
| Plateaux | Plateaux (BigSky Shimmer) | 🔴 | ⏳ |
| Ganymede | Ganymede (RV-6) | 🟡 | ⏳ |
| Dynamic Ambience | Dynamic Ambience | 🟢 | ⏳ |
| Dynamic Bloom | Dynamic Bloom | 🟡 | ⏳ |

---

## Plan de implementación
1. **Cadena flexible** (motor `FxBlock`/`FxChain` + UI dinámica) — ✅ **LISTO**.
   Bloques: agregar (`+ Efecto`), quitar (✕), mover (◀▶), bypass (⏻), perillas dinámicas. Serializado en el estado.
2. **Pack 1** (🟢🟡 alto valor) — 6/~18 listos: ✅ Chorus, Flanger, Phaser, Tremolo, Delay, Reverb(Room).
   Faltan del Pack 1: Vibrato, Uni-Vibe · Tape, Analog/BBD, Ducking, Reverse, Modulado · Hall, Plate, Ducking, Spring · Octaver, Whammy.
3. **Packs siguientes** (más sabores) → al final los 🔴.

> Cada efecto nuevo = 1 archivo header en `src/fx/blocks/` + 1 línea en `FxFactory.cpp`. Paralelizable con workflow.
