TASK ID: 003

STATUS: PENDING

GOAL:
Build the core AudioProcessor (DSP backbone) and parameter system for Wolf's Den. This task establishes the AudioProcessor class, all parameter definitions with fixed IDs, the state save/recall system (getStateInformation/setStateInformation), and the thread-safe bridge between the UI thread and DSP thread. No actual audio generation yet — this is the skeleton that all engines plug into.

ASSIGNED TO:
Cursor

INPUTS:
/03_OUTPUTS/001_architecture_design_document.md
/03_OUTPUTS/002_project_setup_report.md

OUTPUT:
/03_OUTPUTS/003_core_processor_report.md
(Actual code in /05_ASSETS/apex_project/Source/)

DELIVERABLES INSIDE OUTPUT:
- Summary of all parameters defined (ID, name, range, default, type)
- Description of the lock-free ring buffer implementation for UI↔DSP communication
- State save/recall confirmation (tested with preset save in DAW)
- Parameter automation test: confirm DAW can automate at least 5 parameters

SYSTEMS TO IMPLEMENT:

1. PARAMETER SYSTEM (AudioProcessorValueTreeState):
   - Master Volume, Master Pitch, Master Pan
   - Per-Layer: Volume, Pan, Osc Type, Tune Coarse, Tune Fine
   - Per-Layer: Filter Cutoff, Filter Resonance, Filter Type
   - Per-Layer: Amp ADSR (Attack, Decay, Sustain, Release)
   - Filter ADSR (Attack, Decay, Sustain, Release)
   - LFO Rate, LFO Depth, LFO Shape
   - Theory Engine: Scale Root, Scale Type, Chord Type, Voice Leading On/Off
   - MIDI Pipeline: Keys Lock Mode, Chord Mode On/Off, Arp On/Off, Arp Rate
   - FX: Reverb Mix, Delay Mix, Chorus Mix, Master Compressor
   (All parameters get fixed 32-bit IDs — never change after v1.0)

2. THREAD MODEL:
   - DSP thread: processBlock() — audio/MIDI processing only
   - UI thread: editor components — display and user interaction only
   - Bridge: juce::AbstractFifo + lock-free ring buffer for parameter changes
   - No mutexes in processBlock() — lockless only

3. STATE MANAGEMENT:
   - getStateInformation(): serialize AudioProcessorValueTreeState + custom chunk (chord data, preset name, window size)
   - setStateInformation(): deserialize and restore full state
   - Binary format for custom chunk (XML or MemoryBlock)

CONSTRAINTS:
- Zero allocations in processBlock()
- All parameter IDs must be documented in report for permanent reference
- Follow system rules

AFTER COMPLETION:
- Update /04_LOGS/project_log.md
- Change STATUS to DONE
