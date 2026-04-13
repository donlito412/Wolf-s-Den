#pragma once

#include "UITheme.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include <map>

namespace wolfsden::ui
{

/**
 * Custom-painted controls per TASK_009 (no stock OS chrome).
 * Implements 80ms hover fade animation for buttons and tabs via a 60 Hz timer.
 */
class WolfsDenLookAndFeel : public juce::LookAndFeel_V4,
                            private juce::Timer,
                            private juce::ComponentListener
{
public:
    WolfsDenLookAndFeel();

    /** Min at bottom-left (~7:30); 270° clockwise sweep (JUCE: radians CW from 12 o'clock). Call from every rotary `styleKnob`. */
    static void configureRotarySlider(juce::Slider& s) noexcept;
    void drawRotarySlider(juce::Graphics& g,
                          int x,
                          int y,
                          int width,
                          int height,
                          float sliderPosProportional,
                          float rotaryStartAngle,
                          float rotaryEndAngle,
                          juce::Slider& slider) override;

    void drawLinearSlider(juce::Graphics& g,
                          int x,
                          int y,
                          int width,
                          int height,
                          float sliderPos,
                          float minSliderPos,
                          float maxSliderPos,
                          const juce::Slider::SliderStyle style,
                          juce::Slider& slider) override;

    void drawButtonBackground(juce::Graphics& g,
                             juce::Button& button,
                             const juce::Colour& backgroundColour,
                             bool shouldDrawButtonAsHighlighted,
                             bool shouldDrawButtonAsDown) override;

    void drawToggleButton(juce::Graphics& g,
                          juce::ToggleButton& button,
                          bool shouldDrawButtonAsHighlighted,
                          bool shouldDrawButtonAsDown) override;

    void drawComboBox(juce::Graphics& g,
                      int width,
                      int height,
                      bool isButtonDown,
                      int buttonX,
                      int buttonY,
                      int buttonW,
                      int buttonH,
                      juce::ComboBox& box) override;

    juce::Font getLabelFont(juce::Label& label) override;
    juce::Font getComboBoxFont(juce::ComboBox& box) override;

private:
    /** Per-component hover state for the 80ms fade animation. */
    struct HoverState { float alpha = 0.f; bool hovering = false; };
    std::map<juce::Component*, HoverState> hoverMap;

    /** Query (and lazily register) hover alpha for a button/component. */
    float getHoverAlpha(juce::Component& c, bool hovering);

    // juce::Timer — advances all hover alphas toward their target at 60 Hz
    void timerCallback() override;

    // juce::ComponentListener — removes stale entries when a component is deleted
    void componentBeingDeleted(juce::Component& c) override;
};

} // namespace wolfsden::ui
