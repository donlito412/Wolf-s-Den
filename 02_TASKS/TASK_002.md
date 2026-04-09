TASK ID: 002

STATUS: DONE

GOAL:
Initialize the JUCE 7+ project for Wolf's Den VST using CMake. Set up the full build system, configure all target plugin formats (VST3, AU, AAX), establish the folder/file structure for the codebase, and produce a working "hello world" plugin that loads in a DAW with no audio yet.

ASSIGNED TO:
Cursor

INPUTS:
/03_OUTPUTS/001_architecture_design_document.md
/01_BRIEF/project.md

OUTPUT:
/03_OUTPUTS/002_project_setup_report.md
(Actual code lives in /05_ASSETS/apex_project/ source directory)

DELIVERABLES INSIDE OUTPUT:
- Confirmation that project compiles without errors
- Folder structure of the initialized codebase
- CMakeLists.txt summary
- DAW load test screenshots or confirmation log
- Any decisions made during setup that deviate from the ADD

CODEBASE STRUCTURE TO CREATE:
```
/05_ASSETS/apex_project/
├── CMakeLists.txt
├── Source/
│   ├── PluginProcessor.h / .cpp     (AudioProcessor — DSP thread)
│   ├── PluginEditor.h / .cpp        (AudioProcessorEditor — UI thread)
│   ├── engine/
│   │   ├── SynthEngine.h / .cpp
│   │   ├── TheoryEngine.h / .cpp
│   │   ├── MidiPipeline.h / .cpp
│   │   └── FxEngine.h / .cpp
│   └── ui/
│       ├── MainComponent.h / .cpp
│       └── pages/ (empty — populated in later tasks)
├── Resources/
│   └── Database/
│       └── apex.db (empty SQLite — schema in TASK_005)
└── Tests/
```

CONSTRAINTS:
- Use JUCE 7+ (CMake-based — not Projucer)
- Must compile clean on both macOS (Apple Silicon) and Windows
- No placeholder DSP — audio output = silence, UI = blank window with plugin name
- Follow system rules

AFTER COMPLETION:
- Update /04_LOGS/project_log.md
- Change STATUS to DONE
