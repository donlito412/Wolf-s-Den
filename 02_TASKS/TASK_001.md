TASK ID: 001

STATUS: DONE

GOAL:
Produce a complete Architecture Design Document (ADD) for Wolf's Den VST. This document must define every major system component, how they interconnect, the thread model, the data flow from MIDI input to audio output, the parameter system, and the full UI page map. This is the blueprint all subsequent tasks are built from.

ASSIGNED TO:
Claude

INPUTS:
/01_BRIEF/project.md
/01_BRIEF/vst_research.md
/01_BRIEF/agent_rules.md

OUTPUT:
/03_OUTPUTS/001_architecture_design_document.md

DELIVERABLES INSIDE OUTPUT:
- System block diagram (text-based)
- Full component list with responsibility of each
- Thread model: UI thread vs. DSP thread vs. MIDI thread
- Data flow: MIDI in → Theory Engine → Synthesis Engine → FX → Audio out
- Parameter system: how all parameters are defined, typed, and automated
- State management: how presets are saved/recalled
- Database schema: chord sets, scale definitions, presets, tags
- Full UI page map: every page, panel, and key control element
- Build system notes: JUCE + CMake setup requirements

CONSTRAINTS:
- No assumptions
- No rework
- All decisions must be justified against reference plugin research
- Follow system rules

AFTER COMPLETION:
- Update /04_LOGS/project_log.md
- Change STATUS to DONE
