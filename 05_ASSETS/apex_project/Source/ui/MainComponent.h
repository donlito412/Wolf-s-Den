#pragma once

#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

class WolfsDenAudioProcessor;

namespace wolfsden
{

class PerfXyPad;

class MainComponent : public juce::Component
{
public:
    explicit MainComponent(WolfsDenAudioProcessor&);
    ~MainComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    WolfsDenAudioProcessor& processor;

    juce::Label titleLabel;
    juce::Label xyPhysicsLabel;
    juce::ComboBox xyPhysicsCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> xyPhysicsAttachment;

    juce::Slider masterVolSlider;
    juce::Slider masterPanSlider;
    juce::Slider layer0VolSlider;
    juce::Slider layer0CutoffSlider;
    juce::Slider lfoRateSlider;
    juce::Slider lfoDepthSlider;
    juce::Slider reverbMixSlider;
    juce::Slider layer0ResSlider;
    juce::Slider delayMixSlider;
    juce::Slider chorusMixSlider;

    std::unique_ptr<juce::SliderParameterAttachment> attachMasterVol;
    std::unique_ptr<juce::SliderParameterAttachment> attachMasterPan;
    std::unique_ptr<juce::SliderParameterAttachment> attachLayer0Vol;
    std::unique_ptr<juce::SliderParameterAttachment> attachLayer0Cutoff;
    std::unique_ptr<juce::SliderParameterAttachment> attachLfoRate;
    std::unique_ptr<juce::SliderParameterAttachment> attachLfoDepth;
    std::unique_ptr<juce::SliderParameterAttachment> attachReverbMix;
    std::unique_ptr<juce::SliderParameterAttachment> attachLayer0Res;
    std::unique_ptr<juce::SliderParameterAttachment> attachDelayMix;
    std::unique_ptr<juce::SliderParameterAttachment> attachChorusMix;

    std::unique_ptr<PerfXyPad> xyPad;

    std::vector<std::pair<juce::Slider*, juce::String>> flexSliders;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

} // namespace wolfsden
