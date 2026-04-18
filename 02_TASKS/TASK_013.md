TASK ID: 013

PHASE: 2

STATUS: DONE

GOAL:
Bring CPU from 45–55% down to under 20% on Apple M1 in Logic Pro (16-voice polyphony, 4 layers, full FX). The root causes have been identified by reading the actual source code. All expensive math calls in the hot render path have been located with their exact file and line numbers. Do not profile first — the causes are confirmed. Implement every fix below, then measure the result.

ASSIGNED TO:
Cursor

BUG REFERENCE:
BUG_P2_003 — CPU 45–55% on Apple M1 (target <20%).

INPUTS:
/05_ASSETS/apex_project/Source/engine/synth/VoiceLayer.h
/05_ASSETS/apex_project/Source/engine/synth/VoiceLayer.cpp
/05_ASSETS/apex_project/Source/engine/FxEngine.h
/05_ASSETS/apex_project/Source/engine/FxEngine.cpp
/05_ASSETS/apex_project/Source/engine/SynthEngine.cpp

OUTPUT:
/03_OUTPUTS/013_cpu_optimization_report.md
(Code changes in /05_ASSETS/apex_project/Source/)

---

ROOT CAUSES — CONFIRMED BY SOURCE CODE INSPECTION

All of the following are expensive math functions executing inside the audio render loop
(per sample, per voice, per layer = up to 64 calls per sample at 16-voice polyphony, 4 layers).

ROOT CAUSE 1 — computeTargetHz() called per sample (THE BIGGEST HIT)
File: VoiceLayer.cpp, lines 132–146
Function: `static double computeTargetHz(...)`

This function is called once per sample per active voice per layer inside renderAdd().
It contains:
- Line 142: `hz *= std::pow(2.0, fineCents / 1200.0)` — fine tune conversion
- Line 144: `hz *= std::pow(2.0, (double)modMatrixPitchSemi / 12.0)` — mod matrix pitch

`std::pow(2.0, x)` is a transcendental function. At 16 voices × 4 layers × 44100 samples/sec:
- Line 142 alone: 64 × 44100 = ~2.8 million std::pow calls per second
- Line 144: same, when pitch modulation is active

Fix: fine tune (`fineCents`) does not change per sample — it is an APVTS parameter. 
Pre-compute `std::pow(2.0, fineCents / 1200.0)` once per block (or once per noteOn and on parameter change) and cache it as a member variable `double cachedFineTuneFactor`. In renderAdd(), multiply `hz` by `cachedFineTuneFactor` directly.

For modMatrixPitchSemi: this does change per block (mod matrix output). Pre-compute `std::pow(2.0, modMatrixPitchSemi / 12.0)` once per block in SynthEngine::process() before the voice loop (line 550) and pass the pre-computed factor as the argument instead of the raw semitone value. Change the renderAdd() signature to accept `double modMatrixPitchFactor` (defaulting to 1.0) instead of `float modMatrixPitchSemi`.

ROOT CAUSE 2 — Unison std::pow per sample per voice
File: VoiceLayer.cpp, lines 327, 348, 369, 390

Inside the unison oscillator loops (cases 0, 1, 2, 3 of the oscillator type switch):
```
const double cents = centOffset(v, nv);
const double hzv = hz * std::pow(2.0, cents / 1200.0);
```
This fires per sample per unison voice (up to 8 unison voices). For 8-voice unison × 16 note-voices × 4 layers = 512 std::pow calls per sample.

Fix: Pre-compute the unison pitch factors at noteOn() and whenever the unison detune parameter changes. Add a cached array to VoiceLayer:
```cpp
std::array<double, 8> uniPitchFactor {};  // pre-baked std::pow(2.0, cents/1200.0) per voice
int lastNv = 0;                            // track last unison count to know when to rebake
double lastUniDetune = -1.0;               // track last detune amount
```
In renderAdd(), before the oscillator switch, check if `nv` or the unison detune parameter changed:
```cpp
const double uniDetune = readFloatAP(p.layerUnison[layerIndex], 0.f);
if (nv != lastNv || uniDetune != lastUniDetune)
{
    for (int v = 0; v < nv; ++v)
        uniPitchFactor[(size_t)v] = std::pow(2.0, centOffset(v, nv) / 1200.0);
    lastNv = nv;
    lastUniDetune = uniDetune;
}
```
In the oscillator cases, replace `hz * std::pow(2.0, cents / 1200.0)` with `hz * uniPitchFactor[(size_t)v]`.

ROOT CAUSE 3 — Filter keytracking std::pow every 4 samples
File: VoiceLayer.cpp, lines 725–728

```cpp
const float kt = readFloatAP(p.layerKeytrack[layerIndex], 0.f);
const double ktSemi = (double)(midiNote - 60) * (double)kt * 0.12;
const double cut1 = smoothedCut1 * std::pow(2.0, ktSemi / 12.0);
const double cut2 = smoothedCut2 * std::pow(2.0, ktSemi / 12.0);
```
This runs every `kFilterUpdateInterval` (4) samples. For 16 voices × 4 layers = 64 calls every 4 samples = ~700,000 std::pow calls per second just for keytracking.

Fix: `ktSemi` depends on `midiNote` (fixed per note) and `kt` (APVTS parameter, changes rarely). Pre-compute `std::pow(2.0, ktSemi / 12.0)` at noteOn() and cache it as `double cachedKtFactor`. Update the cached value when `kt` changes (detected via parameter comparison). In renderAdd(), replace the two `std::pow` calls with `smoothedCut1 * cachedKtFactor` and `smoothedCut2 * cachedKtFactor`.

ROOT CAUSE 4 — FxEngine Compressor/Limiter std::exp per sample
File: FxEngine.cpp, lines 395–396 (Compressor), lines 420–421 (Limiter)

```cpp
// Compressor (runs every sample when slot type = Compressor):
const float atk = std::exp(-1.f / (float)(sr * (0.001 + p0 * 0.1)));
const float rel = std::exp(-1.f / (float)(sr * (0.01  + p1 * 1.0)));

// Limiter:
const float atk = std::exp(-1.f / (float)(sr * (0.0001 + p1 * 0.01)));
const float rel = std::exp(-1.f / (float)(sr * (0.001  + p2 * 0.5)));
```
`p0`, `p1`, `p2` are APVTS parameters — they do not change per sample. These 4 std::exp calls run inside the `for (int i = 0; i < numSamples; ++i)` loop (line 375), meaning they fire 512 times per block (at buffer size 512). They are constant within the block.

Fix: Move all 4 computations to BEFORE the per-sample loop. Add slot-state caching to `SlotDSP` struct in FxEngine.h:
```cpp
float cachedCompAtk = 0.f, cachedCompRel = 0.f;  // Compressor
float cachedLimAtk  = 0.f, cachedLimRel  = 0.f;  // Limiter
float lastCompP0 = -1.f, lastCompP1 = -1.f;       // dirty detection
float lastLimP1  = -1.f, lastLimP2  = -1.f;
```
Before the per-sample loop, for Compressor slots:
```cpp
if (p0 != st.lastCompP0 || p1 != st.lastCompP1)
{
    st.cachedCompAtk = std::exp(-1.f / (float)(sr * (0.001 + p0 * 0.1)));
    st.cachedCompRel = std::exp(-1.f / (float)(sr * (0.01  + p1 * 1.0)));
    st.lastCompP0 = p0; st.lastCompP1 = p1;
}
```
Inside the loop use `st.cachedCompAtk` and `st.cachedCompRel`. Apply same pattern for Limiter.

ROOT CAUSE 5 — FxEngine EQ/Filter std::pow per sample
File: FxEngine.cpp, lines 441, 452

```cpp
// HighPass and LowPass EQ frequency:
const float f = juce::jlimit(20.f, 20000.f, 20.f * std::pow(1000.f, p0));
```
This runs per sample inside the loop. `p0` is an APVTS parameter — constant within the block.

Fix: Move before the per-sample loop. Add `float cachedHpFreq, lastHpP0` to SlotDSP. Same for LP. Recompute only when `p0` changes.

ROOT CAUSE 6 — FxEngine LFO std::sin per sample
File: FxEngine.cpp, lines 507, 522, 538, 554, 566

LFO-based FX (Chorus, Flanger, Phaser, Tremolo, AutoPan) call `std::sin(st.lfo)` per sample. Unlike the above cases, these DO need to run per sample (the LFO must advance every sample). However, `std::sin` is expensive.

Fix: Replace `std::sin(st.lfo)` with a fast sine approximation. Use the Bhaskara I sine approximation which has <0.2% error and no transcendental cost:
```cpp
// Fast sine approximation — input x in [0, 2*pi]
inline float fastSin(float x) noexcept
{
    // Normalize to [0, 1)
    x *= 0.15915494f; // 1 / (2*pi)
    x -= std::floor(x);
    // Bhaskara I:
    const float s = x < 0.5f ? x * 2.f : 2.f - x * 2.f;
    return 4.f * s * (1.f - s);
}
```
Add this as a file-local inline function in FxEngine.cpp. Replace all `std::sin(st.lfo)` calls with `fastSin(st.lfo)`.
Note: `st.lfo` must still advance per-sample using the rate parameter — only the sine evaluation is replaced.

---

FIXES — DO NOT IMPLEMENT (THESE ARE ALREADY CORRECT)

Do NOT touch these — they were already optimized in Phase 1:
- `kFilterUpdateInterval = 4` (VoiceLayer.h line 198) — filter coefficients already update every 4 samples, not every sample. ✅
- FxEngine Off-slot early-out at line 293: `if (ti <= 0) continue;` — Off slots already skip processing. ✅
- FxEngine zero-mix early-out at line 297: `if (m <= 0.0001f) continue;` — zero-mix slots already skip. ✅
- Voice active check in SynthEngine.cpp line 553: `if (!v.active) continue;` — inactive voices already skip renderAdd(). ✅

---

IMPLEMENTATION ORDER (DO IN THIS ORDER)

1. ROOT CAUSE 1 first — computeTargetHz is the largest single source of redundant std::pow calls.
   After implementing, run a Release build and check CPU in Logic. If it drops below 25%, the remaining fixes are refinements. If still above 25%, continue.

2. ROOT CAUSE 2 — Unison pitch factor caching.

3. ROOT CAUSE 3 — Filter keytracking factor caching.

4. ROOT CAUSES 4 + 5 — FxEngine coefficient caching (group these together, same pattern).

5. ROOT CAUSE 6 — fastSin replacement.

Build and test after EACH root cause fix. If a fix introduces an audible artifact (aliasing, filter glitch), revert that specific fix and document it.

---

DELIVERABLES INSIDE OUTPUT REPORT:
- For each root cause: exact lines changed, old code vs new code
- CPU measurement in Logic Pro BEFORE all fixes (baseline: should match the ~45–55% reported)
- CPU measurement AFTER each individual fix (measure after each so the contribution is known)
- Final CPU measurement after all fixes (target: <20% at 16-voice, 4-layer, full FX, 44.1kHz, 512 buffer)
- Confirmation that audio quality is unchanged: same preset, same notes, no new aliasing or filter artifacts
- Build: cmake --build build --config Release — zero errors and zero new warnings

CONSTRAINTS:
- Zero new allocations in processBlock() or renderAdd()
- Do not change any APVTS parameter IDs
- Do not change any user-visible parameter behavior
- If a fast approximation introduces audible artifact on any test sound, revert that specific fix only
- Audio quality must not degrade
- Follow system rules

AFTER COMPLETION:
- Update /04_LOGS/project_log.md
- Change STATUS to DONE
