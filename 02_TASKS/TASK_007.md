TASK ID: 007

STATUS: PENDING

GOAL:
Build the Modulation System — the Mod Matrix, XY Performance Pad (Wolf's Den equivalent of Omnisphere's Orb), and the Flex-Assign system. This connects modulation sources (LFOs, Envelopes, MIDI, XY Pad) to any modulatable parameter in the synth. This system gives Wolf's Den its motion and expressiveness.

ASSIGNED TO:
Cursor

INPUTS:
/03_OUTPUTS/001_architecture_design_document.md
/03_OUTPUTS/004_synthesis_engine_report.md
/01_BRIEF/vst_research.md

OUTPUT:
/03_OUTPUTS/007_modulation_system_report.md
(Code in /05_ASSETS/apex_project/Source/engine/ — add to SynthEngine + new ModMatrix.h/.cpp)

DELIVERABLES INSIDE OUTPUT:
- Mod Matrix: 32 slots confirmed, all sources and targets functional
- XY Pad: X and Y axes confirmed routing to Cutoff and Resonance as default test
- Flex-Assign: right-click assign on any parameter confirmed working
- Inertia mode: XY cursor continues moving after release, gradually slows
- All source types producing signal: LFO, Envelope, Mod Wheel, Aftertouch, Velocity, XY

MOD MATRIX ARCHITECTURE:

MatrixSlot (32 slots):
```
struct ModSlot {
  ModSource source;      // enum: LFO1, LFO2, AmpEnv, FilterEnv, ModEnv1-4,
                         //       ModWheel, Aftertouch, Velocity, Breath,
                         //       PitchBend, XY_X, XY_Y, Random, Alternating, MIDI_CC[0-127]
  ModTarget target;      // enum: Osc1Pitch, Osc1Fine, Filter1Cutoff, Filter1Res,
                         //       AmpLevel, Pan, LFO1Rate, LFO1Depth, FX1Param1, 
                         //       ... (any parameter from param system)
  float amount;          // -1.0 to +1.0
  bool inverted;         // flip polarity
  float smooth;          // smoothing coefficient (0.0 = none, 1.0 = very smooth)
  bool muted;            // temporarily disable without deleting
  int layerScope;        // which layer this applies to (0=all, 1-4=specific layer)
}
```

Every audio render cycle:
1. For each active ModSlot: read source signal value (already computed by LFO/Envelope pass)
2. Apply amount scaling + inversion
3. Apply smoothing (one-pole filter: output = output + (input - output) * (1-smooth))
4. Add to target parameter's base value
5. Clamp result to target's valid range
6. Flag "out of range" for UI red indicator if clamped

---

XY PERFORMANCE PAD:

Interface: 300×300px square pad (touch/mouse)
Axes: X (0.0-1.0) and Y (0.0-1.0) — both exposed as ModSources XY_X and XY_Y
Default mappings:
- X → Filter1 Cutoff (depth: +0.6)
- Y → LFO1 Rate (depth: +0.4)
User can remap via Mod Matrix (XY_X / XY_Y as sources)

Physics Modes:
- DIRECT: cursor snaps to mouse position
- INERTIA: cursor has velocity — apply damping each frame:
    velocity += (targetPos - currentPos) * attraction
    currentPos += velocity
    velocity *= damping (0.90 = moderate, 0.98 = very floaty)
- CHAOS: Lorenz attractor math — cursor follows chaotic trajectory:
    dx = σ(y - x)           σ=10
    dy = x(ρ - z) - y       ρ=28
    dz = xy - βz            β=8/3
    Normalize output to [0,1] range, add small user-bias toward touch position

Dice button: randomize XY Pad target assignments (intelligently — choose musically relevant targets for current patch type)
Record: capture XY movement as automation data for DAW

---

FLEX-ASSIGN (Quick Modulation):
- Right-click any knob/slider → context menu shows "Modulate with LFO 1", "Modulate with Envelope 1", etc.
- On selection: auto-find next empty ModSlot, fill with source + target, default amount = 0.5
- New routing appears highlighted in Mod Matrix view
- Shift+right-click on a knob: show all current modulations targeting that parameter

CONSTRAINTS:
- ModMatrix is read and written from UI thread only — atomic float array for DSP thread to read
- Smooth coefficient recalculated on sample rate change
- XY Pad: cursor position smoothed to prevent zipper noise on param changes
- Follow system rules

AFTER COMPLETION:
- Update /04_LOGS/project_log.md
- Change STATUS to DONE
