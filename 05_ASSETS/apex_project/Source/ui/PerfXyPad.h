#pragma once

#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>

class WolfsDenAudioProcessor;

namespace wolfsden
{

/** 300×300 performance XY pad; writes ModMatrix XY atomics. */
class PerfXyPad : public juce::Component, private juce::Timer
{
public:
    explicit PerfXyPad(WolfsDenAudioProcessor& processor);
    ~PerfXyPad() override;

    /** Called every timer tick with the current (x, y) position in [0..1].
     *  Set this from ModPage to receive position updates while recording. */
    std::function<void(float x, float y)> onPositionChanged;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

private:
    void timerCallback() override;

    int getPhysicsMode() const noexcept;
    void updateFromMouse(const juce::MouseEvent& e);
    void pushPosition();

    WolfsDenAudioProcessor& proc;

    float targetX = 0.5f;
    float targetY = 0.5f;
    float posX = 0.5f;
    float posY = 0.5f;
    float velX = 0.f;
    float velY = 0.f;
    bool dragging = false;

    double lorenzX = 0.1;
    double lorenzY = 0.0;
    double lorenzZ = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PerfXyPad)
};

} // namespace wolfsden
