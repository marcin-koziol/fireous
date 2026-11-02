# FIReous — plan v0.1.0

Dokument po decyzjach z rozmowy. **v0.1.0 = to, co już wiemy.** Reszta
idzie do backlogu v0.2+.

Najważniejsze:

- **główne wymaganie: nie zmieniać fazy wejściowej** — realizowane przez
  tryb **LINEAR PHASE (FIR)**, także dla PEQ,
- **domyślnie IIR** — zwykłe filtry, prawie zero latency, faza
  nieliniowa (świadomy kompromis na co dzień),
- **bez crossovera** — multiband/routing zostaje w DAW,
- **jeden insert stereo**, 5 modułów, reorder jak w `sideous-effects`,
- **target latency 10 ms**,
- **UI hardware DSP** (dbx/rack + akcenty Vox), motywy do wyboru,
  małe LCD w modułach.

Mockup: `docs/ui-hardware-mockup.png`
Generator: `tools/mockup_hardware.py`

---

## 1. Cel i zakres v0.1.0

Jeden stereo insert, w którym użytkownik układa chain widocznych modułów,
przeciąga je i bypassuje. Cała liniowa część filtrująca ma dwa tryby:

- **IIR** (domyślny) — biquady RBJ, near-zero latency, faza nieliniowa,
  mało CPU. Codzienny tryb do wszystkiego.
- **LINEAR PHASE (FIR)** — filtry o liniowej fazie, brak zmiany fazy
  wejściowej (poza stałym opóźnieniem), wyższe latency i CPU. Głównie
  mastering / praca fazowo-krytyczna. Działa też dla PEQ.

Zakres v0.1.0:

- **Graphic EQ** — 10 pasm, pionowe fadery,
- **Parametric EQ (PEQ)** — 6 pasm + low-pass/high-pass,
- **Delay** — efekt stereo + per-kanałowy alignment,
- **Compressor** — globalny per instancja, peak/RMS, stereo link,
  auto-makeup, sidechain,
- **Limiter** — globalny per instancja, sample/true peak, opcjonalny
  lookahead,
- input/output trim, bypass, phase mode, presety, meters, reorder,
- sidechain input bus (2 porty),
- motywy UI: DBX / VOX / HYBRID.

Poza v0.1.0:

- crossover i wewnętrzny multiband,
- per-band compressor/limiter w jednej instancji (w v0.1.0 per-band robi
  DAW przez wiele instancji),
- multi-out,
- AU,
- dynamiczna liczba wejść,
- 4-way / zaawansowany routing.

---

## 2. Łańcuch sygnału

```
IN L/R (+ opcjonalny sidechain L/R)
  → [INPUT TRIM]
  → [GRAPHIC EQ]     (IIR lub LINEAR, reorder, bypass)
  → [PARAMETRIC EQ]  (IIR lub LINEAR, reorder, bypass)
  → [DELAY]          (effect + align L/R, reorder, bypass)
  → [COMPRESSOR]     (global per instance, reorder, bypass)
  → [LIMITER]        (global per instance, reorder, bypass)
  → [OUTPUT TRIM]
OUT L/R
```

Kolejność zapisana jako `chain_pos_*` (0..4), rozwiązywana stabilnym
sortowaniem — jak w `sideous-effects`. UI przebudowuje layout po zmianie
pozycji. Bypass per moduł.

Dla modułów liniowych (EQ/PEQ/delay) kolejność nie zmienia brzmienia
(LTI są przemienne). Dla compressor/limiter kolejność ma znaczenie, bo to
efekty nieliniowe.

---

## 3. Decyzje projektowe

### 3.1 Główne wymaganie: brak zmiany fazy

„Nie zmieniać fazy, która wchodzi” = **linear phase**. Dlatego:

- tryb **LINEAR PHASE** projektuje EQ i PEQ jako FIR o liniowej fazie,
- faza jest stała w całym pasmie (poza stałym opóźnieniem `M`),
- PEQ również może działać w tym trybie — to jest ta „super” opcja,
- **IIR** nie spełnia tego wymagania: z definicji zmienia fazę. Jest
  domyślny tylko dlatego, że jest tani i ma near-zero latency.

To trzeba nazwać w UI wprost: `PHASE: IIR` vs `PHASE: LINEAR`.
W trybie IIR nie obiecujemy braku zmiany fazy.

### 3.2 Domyślnie IIR, LINEAR jako opcja

- **IIR (default)** — biquady RBJ, latency ~0, phase nonlinear, CPU low.
- **LINEAR** — FIR, phase linear, latency zależne od długości, CPU
  wyższe. Głównie mastering.

Przełącznik globalny `phase_mode` (IIR / LINEAR), nie per moduł.
Przełączenie robi crossfade i aktualizuje `setLatency()`.

### 3.3 Crossover poza v0.1.0

Crossover zostaje w DAW. Per-band compressor/limiter w v0.1.0 realizujemy
przez **wiele instancji FIReous** na pasmach wydzielonych w DAW. Jeśli
kiedyś ma być w jednej instancji, wróci jako osobny plugin/module
(`FIReous Xover`) w v0.2+.

### 3.4 „Wszystkie FIR” — co to znaczy

| Moduł | IIR mode | LINEAR mode |
|---|---|---|
| Graphic EQ | biquady RBJ | linear-phase FIR |
| Parametric EQ | biquady RBJ | linear-phase FIR |
| Delay | delay line | delay line |
| Compressor | dynamika | dynamika |
| Limiter | dynamika | dynamika |

Compressor/limiter to nie filtry — nie mają transmitancji FIR. Mogą być
zero-latency i nie wnosić fazy, gdy nie redukują gainu. Delay to linia
opóźniająca, nie filtr kształtujący.

### 3.5 Reorder i bypass

- 5 modułów, drag za header, bypass LED, slot number.
- `chain_pos_*` 0..4, stable sort.
- **IIR**: bypass = true bypass (0 latency).
- **LINEAR**: bypass modułu FIR zamieniamy na czysty delay o tym samym
  `M`, żeby total latency nie skakało.
- Crossfade przy zmianie współczynników i bypassie.

### 3.6 Target latency 10 ms

**10 ms to domyślny target, nie twardy limit.** Można wybrać dłuższy FIR
dla masteringu, ale UI ma wtedy pokazać ostrzeżenie.

- **IIR**: ~0.1 ms (bez lookahead/true peak).
- **LINEAR**: domyślnie ~10 ms. Długość FIR wyliczana z czasu:
  `N ≈ 2 * latency_ms * fs / 1000 + 1`.
  - 2.7 ms @48 kHz → ~256 tapów (low-latency preview),
  - 5 ms → ~481 tapów,
  - 10 ms → ~961 tapów (default),
  - 20 ms → ~1921 tapów (mastering),
  - 40 ms → ~3841 tapów (mastering).
- EQ i PEQ **scalamy w jeden FIR**, gdy sąsiadują — wtedy 10 ms to
  łączne opóźnienie całego bloku EQ+PEQ, nie 2×10 ms.
- Jeśli są rozdzielone innym modułem, LINEAR ma dwa FIR-y i latency
  może przekroczyć 10 ms; UI pokazuje sumę i ostrzega.
- Limiter lookahead/true peak dolicza się do total latency; domyślnie 0.

---

## 4. DSP

### 4.1 IIR mode

- RBJ cookbook biquady: peaking, low shelf, high shelf, low-pass,
  high-pass.
- Graphic EQ: 10 peaking filters (stałe częstotliwości, Q ≈ 1.4).
- PEQ: 6 pasm + HP/LP.
- Cascade; stabilność i denormal protection.
- Latency 0; faza nieliniowa.

### 4.2 LINEAR mode

- Target magnitude = iloczyn pasm (te same krzywe co IIR, ale bez fazy
  biquadów).
- Linear-phase FIR: `H(ω) * e^{-jωM}`, `M=(N-1)/2`, IFFT, Kaiser
  (`beta ≈ 8.6`), normalizacja.
- Gdy EQ i PEQ sąsiadują: jeden wspólny FIR.
- Gdy nie sąsiadują: dwa FIR-y, latency = `M1 + M2`.
- Bypass: delay `M` zamiast FIR, żeby latency było stałe.

### 4.3 Długość FIR / quality

Zamiast „256 tapów” lepiej sterować **czasem**:

- `linear_latency_ms`: 2.7 / 5 / 10 / 20 / 40 (default 10).
- 2.7 ms ≈ 256 tapów @48 kHz — low-latency preview, słabsze niskie,
- 10 ms ≈ 961 tapów — domyślny kompromis,
- 20/40 ms — mastering, lepsza rozdzielczość niskich.

UI pokazuje rzeczywiste `N` i latency. 256 tapów zostaje jako opcja
„Low”, ale nie jako domyślna dla pełnego pasma.

### 4.4 EQ / PEQ

- Graphic EQ: 10 pasm (31.5, 63, 125, 250, 500, 1k, 2k, 4k, 8k, 16k Hz),
  gain ±12 dB, pionowe fadery.
- PEQ: 6 pasm Bell / Low Shelf / High Shelf, freq 20 Hz–20 kHz,
  gain ±18 dB, Q 0.1–10.
- PEQ + HP/LP: freq, slope 12/24 dB/oct, on/off.
- W LINEAR całość wchodzi do wspólnego targetu FIR.

### 4.5 Delay

- **Effect**: sync Free + 16 podziałów, time 1–2000 ms, feedback
  0–0.95, ping-pong 0–1, mix 0–1, tone (LP w feedbacku).
- **Align**: osobno L i R, 0–10 ms, czysty delay per kanał.
- Tryb `Delay mode`: Effect / Align / Both.
- Czas efektu nie jest raportowany jako latency pluginu; align też nie
  (to świadome opóźnienie użytkownika).

### 4.6 Compressor

- globalny per instancja, stereo-linked (link 0–100%),
- detector RMS / Peak (default RMS),
- zero-latency, bez lookahead,
- threshold -60..0 dB, ratio 1:1–20:1, attack 0.1–100 ms,
  release 10–1000 ms, knee 0–24 dB,
- auto-makeup 0/1,
- key: Internal / External sidechain,
- GR meter.

### 4.7 Limiter

**Lookahead** = limiter patrzy w przyszłość: opóźnia sygnał o kilka ms,
żeby zdążyć ściszyć szczyt, zanim ten dojdzie do ceilingu. Efekt: mniej
overshootów i mniej zniekształceń, ale większa latency. Bez lookahead
limiter działa reaktywnie — jest szybszy, ale może przepuścić krótki
szczyt albo go obciąć.

**True peak** = mierzy szczyty między próbkami (oversampling, np. 4×),
a nie tylko wartości próbek. Sample peak może pokazać -0.1 dB, gdy
rzeczywisty analogowy szczyt po rekonstrukcji jest powyżej 0 dBFS.
True peak jest bezpieczniejszy dla masteringu i kodeków, ale kosztuje
CPU i dokłada małe opóźnienie (filtry oversamplingu).

Wariant v0.1.0:

- globalny per instancja, reorderowalny,
- **lookahead 0 / 1 / 2 ms** (default 0 = najniższa latency),
- **sample peak / true peak** (default sample peak; true peak opcjonalnie),
- ceiling -12..0 dB (default -0.3 dB), release 10–1000 ms,
- GR meter,
- wszystkie dodatki (lookahead/true peak) doliczane do raportowanego
  latency i pokazane w UI.

Jeśli używasz FabFilter Pro-L po FIReous, wbudowany limiter można
bypassować — ale zostaje jako safety i jako opcja dla osób, które chcą
wszystko w jednym pluginie.

### 4.8 Sidechain

- stały, opcjonalny bus 2 portów (`kAudioPortIsSidechain`),
- compressor `key = External` używa sidechainu, jeśli podłączony;
  inaczej main input,
- w LINEAR sidechain jest opóźniony o to samo `M` co main path,
- brak sidechainu = `nullptr` → fallback do main.

### 4.9 Parametry i RT-safety

- `Params.hpp` wspólny dla DSP i UI,
- `setParameterValue()` → `std::atomic<float>` + `dirty`,
- worker thread projektuje IIR/FIR poza audio thread,
- publikacja przez potrójny bufor / lock-free swap,
- audio thread: swap na granicy bloku + crossfade,
- brak alokacji, locków i FFT w `run()`,
- bufory alokowane w `activate()`,
- output-only meter paramy na końcu listy, pomijane w presetach.

### 4.10 Latency

| Tryb | EQ+PEQ | Delay | Comp | Limiter | Total |
|---|---|---:|---:|---:|---:|
| IIR | 0 | 0 | 0 | 0 | **~0.1 ms** |
| LINEAR 10 ms (combined) | ~10 ms | 0 | 0 | 0 | **~10 ms** |
| LINEAR 20 ms | ~20 ms | 0 | 0 | 0 | **~20 ms** |
| LINEAR 10 ms + lookahead 2 ms | ~10 ms | 0 | 0 | 2 ms | **~12 ms** |
| LINEAR + true peak | + oversampling | 0 | 0 | + latency | pokazane w UI |

---

## 5. Parametry — draft v0.1.0

### Global
- `input_trim`, `output_gain` (-24..+24 dB)
- `bypass` (0/1)
- `phase_mode` (IIR / LINEAR, def IIR)
- `linear_latency_ms` (2.7 / 5 / 10 / 20 / 40, def 10; tylko LINEAR)
- `monitor` (Normal / Delta, def Normal; pomijany w presetach)

### Graphic EQ
- 10 × `eq_gain_*` (-12..+12 dB)
- `eq_on`

### PEQ
- 6 × type (Bell / Low Shelf / High Shelf)
- 6 × freq (20–20000 Hz, log)
- 6 × gain (-18..+18 dB)
- 6 × Q (0.1–10, log)
- HP: `hp_on`, `hp_freq`, `hp_slope` (12/24 dB/oct)
- LP: `lp_on`, `lp_freq`, `lp_slope` (12/24 dB/oct)
- `peq_on`

### Delay
- `delay_on`
- `delay_mode` (Effect / Align / Both)
- `delay_sync` (Free + 16)
- `delay_time_ms` (1–2000 ms, log)
- `delay_feedback` (0–0.95)
- `delay_pingpong` (0–1)
- `delay_mix` (0–1)
- `delay_tone` (0–1, LP w feedbacku)
- `align_l_ms`, `align_r_ms` (0–10 ms)

### Compressor
- `comp_on`
- `comp_threshold`, `comp_ratio`, `comp_attack`, `comp_release`,
  `comp_knee`, `comp_makeup`
- `comp_detector` (RMS / Peak, def RMS)
- `comp_link` (0–100%)
- `comp_auto_makeup` (0/1, def 0)
- `comp_key` (Internal / External, def Internal)

### Limiter
- `lim_on` (def 1)
- `lim_ceiling` (def -0.3 dB), `lim_release`
- `lim_lookahead` (0 / 1 / 2 ms, def 0)
- `lim_true_peak` (0/1, def 0)

### Chain
- 5 × `chain_pos_*` (0..4)

### Meters (output-only, poza presetami)
- input/output peak L/R
- GR comp, GR limiter
- latency samples, phase mode

---

## 6. UI — hardware DSP

### 6.1 Kierunek i motywy

Baza: **dbx / rack DSP** — czarna anodowana płyta, rack ears, śruby,
srebrne gałki, LED bar meters, bursztynowy LCD, sitodruk. Akcenty Vox:
krem/copper, cieplejsze LED-y.

Motywy do wyboru:

- **DBX** — czarno-srebrny, niebieskie/amber LED,
- **VOX** — kremowy/czarny, złote/copper akcenty, cieplejsze gałki,
- **HYBRID** (default) — dbx chassis + krem/copper akcenty + ember LED.

Bez kopiowania logotypów Vox/dbx. Motyw zapisany w configu UI
(`~/.config/fireous/theme`), nie w presecie.

### 6.2 Layout 1280 × 900 (stały + UI scale)

```
┌──────────────────────────────────────────────────────────────┐
│ [rack ear] FIReous FIR-5  ● POWER  [LCD: LATENCY 0.1 ms]     │
│ preset bar: < > name SAVE DELETE UI                          │
│ phase: IIR (default) / LINEAR (no phase change)              │
├──────────────────────────────────────────────────────────────┤
│ IN → EQ → PEQ → DELAY → COMP → LIMIT → OUT                   │
├─────────────────────────────────────┬────────────────────────┤
│ GRAPHIC EQ (2 komórki)              │ PARAMETRIC EQ          │
│ [mały LCD: krzywa / MAG / PHASE]    │ [mały LCD: krzywa]     │
│ 10 pionowych faderów                │ 6 pasm + HP/LP         │
├──────────────────┬──────────────────┼────────────────────────┤
│ DELAY            │ COMPRESSOR       │ LIMITER                │
│ [LCD: czas/align]│ [LCD + GR meter] │ [LCD + GR meter]       │
│ effect + align   │ peak/RMS, link   │ lookahead/true peak    │
└──────────────────┴──────────────────┴────────────────────────┘
```

- 5 modułów zawsze widocznych, bez zwijania.
- Małe LCD w modułach, nie pełnowymiarowy wykres.
- Graphic EQ: pionowe fadery.
- Każdy moduł: header, slot, bypass LED, drag dots, śruby.
- Górny LCD: latency, phase mode, sample rate.
- EQ/PEQ LCD: MAG / PHASE toggle (w IIR phase nie jest płaski,
  w LINEAR jest — to wizualny dowód).
- Comp/limiter: LED GR meters.

### 6.3 Widgety Cairo

- `HardwareKnob`, `VerticalFader`, `LedButton`, `ToggleSwitch`,
  `LedBarMeter`, `ModuleLcd`, `Screw`, `RackEar`, `ModuleCard`.
- Wspólne hit-testy rysowania i klikania.

### 6.4 Wydajność

Statyczne tło (chassis, panele, śruby, sitodruk) cache'owane do
`cairo_image_surface`; przerysowujemy tylko dynamiczne widgety.

### 6.5 Presety / UI scale / offline renderer

- `PresetStore.hpp`, `UiScale.hpp`, `FactoryPresets.hpp` jak w rodzinie.
- `~/.local/share/fireous/presets`, `.firpreset`.
- `tools/offline_render.cpp` do finalnego renderu PNG.
- Presety: Flat / Null, Vocal Air, Drum Bus Glue, Bass Tight,
  Bright Master, Lo-Fi Telephone, Delay Space, Phase Check.

---

## 7. Repo / build / CI

```
fireous/
  CMakeLists.txt
  install.sh
  README.md
  fireous-plan.md
  docs/ui-hardware-mockup.png
  tools/mockup_hardware.py
  dpf/
  plugins/FIReous/
    DistrhoPluginInfo.h
    Params.hpp
    FireousPlugin.cpp
    FireousUI.cpp
    UiScale.hpp
    Engine.hpp
    dsp/
      Fft.hpp
      FirDesign.hpp
      FirConvolver.hpp
      Biquad.hpp
      EqDesign.hpp
      DelayLine.hpp
      Sync.hpp
      Compressor.hpp
      Limiter.hpp
      SmoothedValue.hpp
    ui/
      UIPainter.hpp
      PresetStore.hpp
      FactoryPresets.hpp
    tools/
      offline_render.cpp
  tests/
    CMakeLists.txt
    test_fir.cpp
    test_eq.cpp
    test_dynamics.cpp
    test_engine.cpp
```

- Brand: **FIReous**, repo **fireous**, bez „Sideous”.
- URI: `https://github.com/marcin-koziol/fireous` (placeholder do
  potwierdzenia).
- CLAP ID: `fireous.fireous` (placeholder).
- Formaty: VST3 / LV2 / CLAP / JACK; **bez AU** w v0.1.0.
- `DISTRHO_PLUGIN_NUM_INPUTS 4` (2 main + 2 sidechain),
  `NUM_OUTPUTS 2`, `WANT_LATENCY 1`, `WANT_TIMEPOS 1`.
- CI: Linux/Windows/macOS + `linux-tests`; release na tagi `v*`.
- Licencja ISC.
- `fireous` dopisany do `../install-all.sh`.
- **Framework: DPF** — DSP speed zależy od implementacji, nie od
  frameworka; DPF jest lekki i spójny z rodziną. JUCE/iPlug2 dopiero
  gdy pojawi się realna potrzeba I/O/UI.

---

## 8. Testy i kryteria akceptacji

### 8.1 DSP

1. **IIR mode** — flat EQ = identity, biquady stabilne, latency 0.
2. **LINEAR mode** — `h[n] == h[N-1-n]`, group delay `M`,
   `latencySamples() == M`.
3. **LINEAR flat** — wszystkie gainy 0 → delayed identity.
4. **EQ+PEQ combined** — jeden FIR, latency ≤ target (10 ms default).
5. **Separated EQ/PEQ** — dwa FIR-y, latency = `M1 + M2`, UI warning.
6. **HP/LP** — poprawna charakterystyka w IIR i LINEAR.
7. **Bypass** — IIR identity; LINEAR delayed identity (stała latency).
8. **Reorder** — EQ/PEQ/delay LTI invariance; comp/limiter order
   zmienia wynik.
9. **Delay effect + align L/R** — niezależne czasy, feedback, tone.
10. **Compressor** — peak/RMS, stereo link, auto-makeup, sidechain.
11. **Limiter** — ceiling, lookahead, true peak, GR.
12. **Sidechain alignment** — w LINEAR sidechain opóźniony o `M`.
13. **Sample rate** — 44.1/48/96/192 kHz, latency zgodne z targetem.
14. **Block size** — 1/32/512 dają ten sam wynik.
15. **RT safety** — `run()` bez alokacji/locków.

### 8.2 UI

- offline render PNG przy 1280×900,
- Xvfb + xdotool + compare dla knob/fader drag, bypass, reorder,
  phase mode, theme switch, preset save/load, UI scale,
- motywy DBX/VOX/HYBRID renderują się poprawnie.

### 8.3 Kryteria sukcesu

- **workflow** — 5 modułów widocznych, drag, bypass, presety,
- **latency** — IIR ~0.1 ms; LINEAR default ≤10 ms,
- **stabilność** — brak crashy/NaN, stabilne przy zmianie sample rate,
  block size, reorder, bypass, phase mode,
- build VST3/LV2/CLAP/JACK na Linux/Windows/macOS,
- `ctest` zielony,
- brak alokacji/locków w audio thread.

---

## 9. Etapy v0.1.0

1. **Szkielet repo** — git init, DPF submodule, CMake, install.sh, CI,
   README stub, offline renderer stub, mockup.
2. **IIR engine** — Biquad, Graphic EQ, PEQ, HP/LP, Delay, Compressor,
   Limiter, testy.
3. **LINEAR engine** — FFT, FirDesign, FirConvolver, łączony EQ+PEQ,
   latency, testy null/symetrii.
4. **Sidechain + delay align + meters** — integracja w Engine.
5. **UI hardware DSP** — layout 1280×900, małe LCD, motywy, fadery,
   meters, reorder, presety, UI scale.
6. **Polish** — presety fabryczne, screenshot, README, CI z testami,
   Xvfb, benchmark CPU.

---

## 10. Ryzyka / decyzje do potwierdzenia

- **#0 vs IIR default** — IIR zmienia fazę. „Brak zmiany fazy” jest
  spełniony tylko w LINEAR. To musi być jasne w UI.
- **Per-band vs brak crossovera** — w v0.1.0 per-band przez wiele
  instancji w DAW; wewnętrzny per-band wymaga crossovera i wchodzi
  w v0.2+.
- **10 ms vs rozdzielczość niskich** — 10 ms @48 kHz to ~961 tapów
  łącznego FIR; to kompromis. Mastering może wybrać 20/40 ms.
- **Limiter lookahead/true peak** — może przekroczyć 10 ms; opcjonalne.
- **Sidechain** — trzeba wyrównać latency w LINEAR.
- **Motywy UI** — czytelność przy 1x; cache tła.
- **CPU** — LINEAR 10 ms na 5 modułach może być ciężki; partitioned FFT
  w rezerwie.

---

## 11. Decyzje zamknięte i pozostałe pytania

### Zamknięte

1. **#0 / faza:** IIR może zmieniać fazę; brak zmiany fazy jest
   obietnicą trybu **LINEAR PHASE (FIR)**, także dla PEQ.
2. **Per-band:** w v0.1.0 przez wiele instancji FIReous w DAW, bez
   wewnętrznego crossovera.
3. **Latency:** 10 ms to domyślny target, nie twardy limit; 20/40 ms
   dozwolone dla masteringu z ostrzeżeniem.
4. **Domyślny LINEAR:** 10 ms (~961 tapów łącznie dla EQ+PEQ).
5. **Sidechain:** tylko compressor, stereo 2 porty.
6. **Delay align:** 0–10 ms per kanał.
7. **HP/LP:** 12/24 dB/oct.
8. **Delta monitor:** tak, mały przycisk.
9. **Skeuomorphizm:** subtelny industrial domyślnie.
10. **EQ/PEQ LCD:** MAG domyślnie + PHASE toggle.
11. **Auto-makeup:** domyślnie off.
12. **Compressor detector:** wybór Peak/RMS (domyślnie RMS — rekomendacja).
13. **True peak:** domyślnie off.
14. **Brand:** FIReous; URI `https://github.com/marcin-koziol/fireous`,
    CLAP ID `fireous.fireous`; repo `fireous`.
15. **Motywy:** default HYBRID.
16. **Presety fabryczne:** lista z §6.5 OK.
17. **Bez crossovera, bez AU, bez multi-out w v0.1.0.**
18. **5 modułów:** Graphic EQ, PEQ, Delay, Compressor, Limiter.
19. **Graphic EQ:** pionowe fadery, 10 pasm; PEQ 6 pasm + HP/LP.
20. **UI:** 1280×900, małe LCD w modułach, LED meters.
21. **Build:** DPF, VST3/LV2/CLAP/JACK, CI Linux/Windows/macOS + testy,
    ISC, wpis w `install-all.sh`.
22. **Materiał/use case:** wszystko.
23. **Problem do rozwiązania:** zachowanie fazy.
24. **Sukces:** workflow, latency, stabilność.
25. **Brak referencyjnego tracku/sesji.**
26. **v0.1.0 = to, co już wiemy; reszta do v0.2+.**

### Pozostałe do potwierdzenia

1. **Limiter:** czy akceptujesz wbudowany limiter z opcjami
   `lookahead 0/1/2 ms` i `true peak on/off`, domyślnie 0 ms + sample
   peak? Jeśli używasz FabFilter Pro-L, możesz go bypassować.
2. **Compressor detector default:** RMS czy Peak? Rekomendacja: RMS
   (bardziej muzyczny); Peak do kontroli transjentów.
3. **Per-band workflow w DAW:** ile pasm polecam? **3 pasma**
   (low/mid/high) to sweet spot dla większości materiału; 2 pasma do
   sub/tops, 4 do masteringu/sound designu. Rekomendacja: 3, np.
   120 Hz / 3 kHz.

### v0.2+ backlog

- crossover / wewnętrzny multiband,
- osobny plugin `FIReous Xover`,
- AU,
- multi-out,
- dynamiczna liczba wejść,
- 4-way,
- zaawansowany routing,
- per-band w jednej instancji.

---

## 12. Podsumowanie v0.1.0

FIReous v0.1.0 to jeden stereo insert z 5 reorderowalnymi modułami,
bez crossovera i bez wewnętrznego routingu. Domyślnie działa na IIR
(near-zero latency, faza nieliniowa), a tryb LINEAR PHASE (FIR) daje
brak zmiany fazy dla EQ i PEQ — główne wymaganie — przy celu 10 ms
i opcjonalnym masteringu 20/40 ms. Per-band realizuje DAW przez wiele
instancji. UI to hardware DSP z małymi LCD, motywami DBX/VOX/HYBRID
i pionowymi faderami.

Najważniejsze do potwierdzenia: **#0 vs IIR**, **per-band przez DAW**,
**10 ms jako twardy limit czy default**, **domyślne 256 vs 10 ms**
i **limiter vs FabFilter**.
