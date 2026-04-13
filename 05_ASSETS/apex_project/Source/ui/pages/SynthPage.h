#pragma once

#include "../theme/UITheme.h"
#include "WDSampleBrowserOverlay.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <memory>
#include <vector>

class WolfsDenAudioProcessor;

namespace wolfsden::ui
{

/** Simple ADSR outline driven by four sliders (TASK_009 amp / filter env shape display). */
class AdsrPreview : public juce::Component
{
public:
    void setSources(juce::Slider* a, juce::Slider* d, juce::Slider* s, juce::Slider* r)
    {
        sa = a;
        sd = d;
        ss = s;
        sr = r;
    }

    void paint(juce::Graphics& g) override;

private:
    juce::Slider *sa = nullptr, *sd = nullptr, *ss = nullptr, *sr = nullptr;
};

/** Placeholder for oscillator waveform / granular animation (TASK_009). */
class WaveformPreview : public juce::Component
{
public:
    void paint(juce::Graphics& g) override;
};

/**
 * TASK_009 SYNTH: layer tabs A–D, three columns (Osc | Filter | Amp), rotary knobs,
 * vertical volume fader, ADSR previews, bottom LFO1 (global) + LFO2 (per layer).
 */
class SynthPage : public juce::Component,
                  private juce::Timer
{
public:
    explicit SynthPage(WolfsDenAudioProcessor& proc);
    ~SynthPage() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void setActiveLayer(int layerIndex);
    void clearLayerBindings();
    void bindLayer(int layerIndex);
    void syncOscButtons();
    juce::String layerKey(const juce::String& tail) const;

    static void styleKnob(juce::Slider& s);
    static void styleHSlider(juce::Slider& s);
    static void styleVFader(juce::Slider& s);

    WolfsDenAudioProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;
    int activeLayer = 0;

    std::array<juce::TextButton, 4> layerTab;
    std::array<juce::TextButton, 8> oscModeBtn;
    WaveformPreview wavePreview;
    juce::Label granularHint;

    // OSC column
    juce::Slider oscOctave;   // new: -3 to +3 octave
    juce::Slider oscCoarse;
    juce::Slider oscFine;

    // Granular-specific controls (visible only when osc mode == Granular)
    juce::Slider granPos, granSize, granDensity, granScatter;
    juce::Label  lblGranPos, lblGranSize, lblGranDensity, lblGranScatter;
    juce::Rectangle<int> zoneGranular;

    // Filter 1
    juce::ComboBox filterType;
    juce::Slider fCut;
    juce::Slider fRes;
    juce::Slider fDrive;   // new

    // Filter 2
    juce::Label    lblFilter2Header;
    juce::ComboBox filter2Type;
    juce::Slider   fCut2, fRes2, fDrive2;

    juce::ComboBox filterRoute;

    juce::Slider filEnvA, filEnvD, filEnvS, filEnvR;
    AdsrPreview filEnvShape;

    juce::Slider ampA, ampD, ampS, ampR;
    AdsrPreview ampShape;

    juce::Slider ampVol;
    juce::Slider ampPan;

    juce::Slider uniVoices;
    juce::Slider uniDetune;
    juce::Slider uniSpread;

    juce::Slider lfo1Rate, lfo1Depth;
    juce::ComboBox lfo1Shape;
    juce::Slider lfo2Rate, lfo2Depth;
    juce::ComboBox lfo2Shape;

    // Sample browser (visible only when osc type == 7 "Smp")
    juce::TextButton loadSampleBtn { "Browse Samples…" };
    std::unique_ptr<WDSampleBrowserOverlay> sampleBrowser;

    /** Layout zones (updated in resized) for section titles in paint — TASK_009 columns + LFO strip. */
    juce::Rectangle<int> zoneOsc, zoneFilt, zoneAmp, zoneLfo;

    juce::Label lblFilter1, lblFilter2, lblFilterTopology;
    juce::Label lblDrivePlanGap;
    juce::Label lblLfoPlanGap;

    /** Short captions above rotaries / LFO (ADSR uses attachToComponent on sliders). */
    juce::Label lblOct, lblSemi, lblFine;
    juce::Label lblCut1, lblRes1, lblDrv1;
    juce::Label lblCut2, lblRes2, lblDrv2;
    juce::Label lblFilAtk, lblFilDec, lblFilSus, lblFilRel;
    juce::Label lblAmpAtk, lblAmpDec, lblAmpSus, lblAmpRel;
    juce::Label lblUniV, lblUniDet, lblUniSpr;
    juce::Label lblLevel, lblPan;
    juce::Label lblL1R, lblL1D, lblL1Sh;
    juce::Label lblL2R, lblL2D, lblL2Sh;

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> globalSAtt;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> globalCAtt;

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> layerSAtt;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> layerCAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SynthPage)
};

} // namespace wolfsden::ui
