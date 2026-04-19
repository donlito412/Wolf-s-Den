TASK ID: 021

PHASE: 2

STATUS: DONE

GOAL:
Complete the CPU optimization that TASK_013 left unfinished. Three root causes were correctly identified and fixed (RC1-fineTune, RC2-unison, RC3-keytrack). Three others were never applied: the modMatrix std::pow is still called per sample (the single largest remaining hit), and the FxEngine Compressor, Gate, HighPass, LowPass, and LFO still call std::exp/std::pow/std::sin inside the per-sample loop. Fix all remaining root causes. Target: CPU ≤ 20% at 16-voice, 4-layer, full FX, 44.1kHz, 512 buffer on Apple M1.

ASSIGNED TO:
Cursor

BUG REFERENCE:
BUG_P2_003 — CPU 45–55% on Apple M1 (target <20%).
Follow-up to TASK_013 (partially done: RC1-fineTune ✅, RC2 ✅, RC3 ✅ — remaining below).

INPUTS:
/05_ASSETS/apex_project/Source/engine/synth/VoiceLayer.cpp
/05_ASSETS/apex_project/Source/engine/synth/VoiceLayer.h
/05_ASSETS/apex_project/Source/engine/SynthEngine.cpp
/05_ASSETS/apex_project/Source/engine/FxEngine.cpp
/05_ASSETS/apex_project/Source/engine/FxEngine.h

OUTPUT:
/03_OUTPUTS/021_cpu_remaining_fixes_report.md

---

WHAT WAS ALREADY FIXED (DO NOT TOUCH)

These were correctly implemented in TASK_013 — confirmed by reading source:

RC1-fineTune: ✅ DONE
  VoiceLayer.cpp line 136: computeTargetHz now takes `double fineTuneFactor` parameter.
  VoiceLayer.cpp line 142: `hz *= fineTuneFactor;` — no std::pow.
  VoiceLayer.cpp noteOn() line 161–163: cachedFineTuneFactor baked at noteOn.

RC2-unison: ✅ DONE
  VoiceLayer.cpp lines 323–329: uniPitchFactor[] array rebuilt only when nv or detune changes.
  VoiceLayer.cpp lines 349, 369, 389: `hz * uniPitchFactor[v]` — no std::pow in oscillator loops.

RC3-keytrack: ✅ DONE
  VoiceLayer.cpp lines 746–751: cachedKtFactor updated only when kt changes.
  VoiceLayer.cpp line 752: `smoothedCut1 * cachedKtFactor` — no std::pow per 4-sample update.

---

REMAINING ROOT CAUSE A — modMatrix std::pow still called per sample
Confirmed by reading VoiceLayer.cpp line 144 and SynthEngine.cpp line 554–566.

VoiceLayer.cpp line 144 (inside computeTargetHz, called once per SAMPLE per voice per layer):
```cpp
if (modMatrixPitchSemi != 0.f)
    hz *= std::pow(2.0, (double)modMatrixPitchSemi / 12.0);  // NOT FIXED
```

SynthEngine.cpp line 554–566 (the voice render loop):
```cpp
v.layers[(size_t)L].renderAdd(layL[(size_t)L],
                               layR[(size_t)L],
                               L,
                               v.midiNote,
                               v.velocity,
                               gLfoToVoice,          // <-- mod matrix pitch in semitones
                               ...
                               layerPitchSemi[(size_t)L],  // passed as raw semitone
                               ...);
```

At 16 voices × 4 layers × 44100 samples = 2.8M std::pow calls/sec when pitch mod is active.
This is the largest remaining CPU hit.

FIX FOR ROOT CAUSE A:

Step A1: Change `computeTargetHz` to accept `double modMatrixPitchFactor` (pre-baked) instead of
`float modMatrixPitchSemi`:

File: VoiceLayer.cpp, computeTargetHz() (lines 132–146)

OLD:
```cpp
static double computeTargetHz(int midiNote,
                               int layerIndex,
                               const ParamPointers& p,
                               float modMatrixPitchSemi,
                               double fineTuneFactor) noexcept
{
    ...
    hz *= fineTuneFactor;
    if (modMatrixPitchSemi != 0.f)
        hz *= std::pow(2.0, (double)modMatrixPitchSemi / 12.0);
    return hz;
}
```

NEW:
```cpp
static double computeTargetHz(int midiNote,
                               int layerIndex,
                               const ParamPointers& p,
                               double modMatrixPitchFactor,   // pre-baked, default 1.0
                               double fineTuneFactor) noexcept
{
    ...
    hz *= fineTuneFactor;
    hz *= modMatrixPitchFactor;   // 1.0 when no mod = no cost; non-1.0 = single multiply
    return hz;
}
```

Step A2: Update all callers of computeTargetHz. Search for `computeTargetHz` in VoiceLayer.cpp —
there are two calls: one in noteOn() (line 167) and one inside renderAdd(). For both,
pass `1.0` as modMatrixPitchFactor (noteOn doesn't need it; renderAdd uses the pre-baked value
computed in Step A3).

Step A3: In SynthEngine.cpp, BEFORE the voice loop (before line 549: `for (auto& v : voices)`),
pre-compute the modMatrix pitch factor ONCE per block per layer:

```cpp
// Pre-compute modMatrix pitch factor per layer (RC1-modMatrix fix)
double layerPitchFactor[kNumLayers];
for (int L = 0; L < kNumLayers; ++L)
{
    const float semiF = layerPitchSemi[(size_t)L];
    layerPitchFactor[(size_t)L] = (semiF != 0.f)
        ? std::pow(2.0, (double)semiF / 12.0)
        : 1.0;
}
```

Step A4: In SynthEngine.cpp inside the voice loop (line 554), pass `layerPitchFactor[L]`
instead of `layerPitchSemi[L]`:

OLD:
```cpp
v.layers[(size_t)L].renderAdd(..., layerPitchSemi[(size_t)L], ...);
```

NEW:
```cpp
v.layers[(size_t)L].renderAdd(..., layerPitchFactor[(size_t)L], ...);
```

Step A5: Update renderAdd() signature in VoiceLayer.h and VoiceLayer.cpp to accept
`double modMatrixPitchFactor` (instead of `float modMatrixPitchSemi`). Update its body to
call `computeTargetHz(..., modMatrixPitchFactor, cachedFineTuneFactor)`.

---

REMAINING ROOT CAUSE B — FxEngine Compressor std::exp per sample
Confirmed by reading FxEngine.cpp lines 395–396 (inside the per-sample loop, line 375).

```cpp
// Inside for (int i = 0; i < numSamples; ++i) loop:
const float atk = std::exp(-1.f / (float)(sr * (0.001 + p0 * 0.1)));   // NOT FIXED
const float rel = std::exp(-1.f / (float)(sr * (0.01 + p1 * 1.0)));    // NOT FIXED
```

p0 and p1 are APVTS parameters — constant within the block. 2 std::exp calls × 512 samples = 1024 calls/block per Compressor slot.

FIX: Cache compressor coefficients in SlotDSP and only recompute when params change.

File: FxEngine.h — add to SlotDSP struct:
```cpp
float cachedCompAtk = 0.f, cachedCompRel = 0.f;
float lastCompP0 = -999.f, lastCompP1 = -999.f;
```

File: FxEngine.cpp — BEFORE the per-sample loop (before `for (int i = 0; i < numSamples; ++i)`),
add for Compressor case:
```cpp
if (t == FxUnitType::Compressor)
{
    const float p0b = readP(0), p1b = readP(1);
    if (p0b != st.lastCompP0 || p1b != st.lastCompP1)
    {
        st.cachedCompAtk = std::exp(-1.f / (float)(sr * (0.001 + p0b * 0.1)));
        st.cachedCompRel = std::exp(-1.f / (float)(sr * (0.01  + p1b * 1.0)));
        st.lastCompP0 = p0b;
        st.lastCompP1 = p1b;
    }
}
```

Inside the per-sample loop's Compressor case (lines 392–407), REMOVE the two std::exp lines
and replace `atk` / `rel` with `st.cachedCompAtk` / `st.cachedCompRel`.

---

REMAINING ROOT CAUSE C — FxEngine Gate std::exp per sample
Confirmed by reading FxEngine.cpp lines 420–421 (same per-sample loop).

```cpp
const float atk = std::exp(-1.f / (float)(sr * (0.0001 + p1 * 0.01)));  // NOT FIXED
const float rel = std::exp(-1.f / (float)(sr * (0.001  + p2 * 0.5)));   // NOT FIXED
```

Same fix pattern as Compressor. Add to FxEngine.h SlotDSP:
```cpp
float cachedGateAtk = 0.f, cachedGateRel = 0.f;
float lastGateP1 = -999.f, lastGateP2 = -999.f;
```

Pre-loop guard for Gate type:
```cpp
if (t == FxUnitType::Gate)
{
    const float p1b = readP(1), p2b = readP(2);
    if (p1b != st.lastGateP1 || p2b != st.lastGateP2)
    {
        st.cachedGateAtk = std::exp(-1.f / (float)(sr * (0.0001 + p1b * 0.01)));
        st.cachedGateRel = std::exp(-1.f / (float)(sr * (0.001  + p2b * 0.5)));
        st.lastGateP1 = p1b;
        st.lastGateP2 = p2b;
    }
}
```

Inside Gate per-sample case (lines 416–428), remove std::exp and replace with cached values.

---

REMAINING ROOT CAUSE D — FxEngine HighPass/LowPass std::pow per sample
Confirmed by reading FxEngine.cpp lines 441 (HighPass) and 452 (LowPass).

```cpp
// HighPass, inside per-sample loop:
const float f = juce::jlimit(20.f, 20000.f, 20.f * std::pow(1000.f, p0));  // NOT FIXED

// LowPass, same:
const float f = juce::jlimit(20.f, 20000.f, 20.f * std::pow(1000.f, p0));  // NOT FIXED
```

p0 is APVTS — constant within block. std::pow(1000.f, p0) fires 512 times/block per slot.

Add to FxEngine.h SlotDSP:
```cpp
float cachedHpFreq = 0.f, lastHpP0 = -999.f;
float cachedLpFreq = 0.f, lastLpP0 = -999.f;
```

Pre-loop guard for HighPass:
```cpp
if (t == FxUnitType::HighPass)
{
    const float p0b = readP(0);
    if (p0b != st.lastHpP0)
    {
        st.cachedHpFreq = juce::jlimit(20.f, 20000.f, 20.f * std::pow(1000.f, p0b));
        st.hpL.setCutoffFrequency(st.cachedHpFreq);
        st.hpR.setCutoffFrequency(st.cachedHpFreq);
        st.lastHpP0 = p0b;
    }
}
```

Similarly for LowPass. Inside per-sample loop, remove the std::pow call and the setCutoffFrequency
calls (since frequency only changes between blocks now, not per sample). The filter itself
(`st.hpL.processSample(...)`) still runs per sample — only the coefficient update moves out.

Note: The filter's resonance (p1 in HP/LP) also calls setCutoffFrequency indirectly — cache it
too if the resonance changes, using the same dirty-detection pattern.

---

REMAINING ROOT CAUSE E — FxEngine LFO std::sin per sample
Confirmed by reading FxEngine.cpp:
  Line 507:  `const float mod = std::sin(st.lfo) * modScl;`         (Vibrato/Tremolo)
  Line 522:  `const float mod = (0.5f + 0.5f * std::sin(st.lfo)) * ...` (Flanger)
  Line 538:  `0.05f * std::sin(st.lfo)`                               (Phaser coefficient)
  Line 554:  `const float g = 1.f - p1 * (0.5f + 0.5f * std::sin(st.lfo));` (Tremolo)
  Line 566:  `const float p = 0.5f + 0.5f * std::sin(st.lfo) * depth;` (AutoPan)

The LFO phase must still advance every sample (st.lfo += rate). Only the sine evaluation is
replaced. The Bhaskara I approximation gives <0.2% error with zero transcendental cost.

FIX: Add as a file-local inline function at the top of FxEngine.cpp (before the class methods):

```cpp
/** Fast sine approximation (Bhaskara I) — input x in [0, 2*pi].
 *  Error < 0.2%. Replaces std::sin in LFO hot paths.
 */
static inline float fastSin(float x) noexcept
{
    x *= 0.15915494f;           // map to [0, 1)
    x -= std::floor(x);
    const float s = x < 0.5f ? x * 2.f : 2.f - x * 2.f;
    return 4.f * s * (1.f - s); // Bhaskara I
}
```

Replace ALL occurrences of `std::sin(st.lfo)` in FxEngine.cpp with `fastSin(st.lfo)`.
Do NOT replace the std::sin in filter coefficient calculations (Phaser line 538 uses
`std::sin(st.lfo)` inside a coefficient formula — use fastSin there too, <0.2% error is fine).

There are 5 std::sin(st.lfo) calls. Replace all 5.

---

IMPLEMENTATION ORDER

1. ROOT CAUSE A first (modMatrix pow) — largest remaining hit. Measure CPU after.
2. ROOT CAUSES B + C (Compressor + Gate exp) — group together, same pattern.
3. ROOT CAUSE D (HighPass + LowPass pow) — group together.
4. ROOT CAUSE E (LFO fastSin) last.

Build and run Logic Pro CPU meter after each group. Document the measurement.

---

DELIVERABLES INSIDE OUTPUT REPORT:
- For each root cause: exact diff (old code → new code) with file and line numbers
- CPU measurement BEFORE all fixes (baseline: should be ~45%)
- CPU measurement AFTER ROOT CAUSE A alone
- CPU measurement AFTER ROOT CAUSES B+C
- CPU measurement AFTER ROOT CAUSE D
- CPU measurement AFTER ROOT CAUSE E (final)
- Final CPU target: ≤ 20% at 16-voice, 4-layer, full FX, 44.1kHz, 512 buffer, Logic Pro on M1
- Confirmation: no new audio artifacts (same preset, same notes, no glitches)
- Build: cmake --build build --config Release — zero errors and zero new warnings

CONSTRAINTS:
- Zero new allocations in processBlock(), renderAdd(), or the FxEngine per-sample loop
- Do not change any APVTS parameter IDs
- Do not change any user-visible parameter behavior
- If fastSin introduces audible artifact on any specific FX type, revert only that type
- Do not touch RC1-fineTune, RC2-unison, RC3-keytrack — they are already correct
- Follow system rules

AFTER COMPLETION:
- Update /04_LOGS/project_log.md
- Change STATUS to DONE
