#include "WolfsDenLookAndFeel.h"

namespace wolfsden::ui
{
WolfsDenLookAndFeel::WolfsDenLookAndFeel()
{
    if (auto tf = Theme::interTypeface())
        setDefaultSansSerifTypeface(tf);

    setColour(juce::ComboBox::backgroundColourId, Theme::panelSurface());
    setColour(juce::ComboBox::outlineColourId, Theme::textDisabled());
    setColour(juce::ComboBox::textColourId, Theme::textPrimary());
    setColour(juce::PopupMenu::backgroundColourId, Theme::backgroundMid());
    setColour(juce::PopupMenu::textColourId, Theme::textPrimary());
    setColour(juce::TextButton::buttonColourId, Theme::panelSurface());
    setColour(juce::TextButton::buttonOnColourId, Theme::accentPrimary().withAlpha(0.35f));
    setColour(juce::TextButton::textColourOffId, Theme::textPrimary());
    setColour(juce::TextButton::textColourOnId, Theme::accentHot());
    setColour(juce::Label::textColourId, Theme::textSecondary());
    setColour(juce::Slider::thumbColourId, Theme::accentHot());
    setColour(juce::Slider::trackColourId, Theme::panelSurface());
    setColour(juce::Slider::backgroundColourId, Theme::backgroundDark());
}

void WolfsDenLookAndFeel::drawRotarySlider(juce::Graphics& g,
                                           int x,
                                           int y,
                                           int width,
                                           int height,
                                           float sliderPosProportional,
                                           float rotaryStartAngle,
                                           float rotaryEndAngle,
                                           juce::Slider& slider)
{
    juce::ignoreUnused(slider);
    const auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(4.f);
    const float cx = bounds.getCentreX();
    const float cy = bounds.getCentreY();
    const float r = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;

    g.setColour(Theme::backgroundDark());
    g.fillEllipse(bounds);

    const float a0 = rotaryStartAngle;
    const float a1 = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
    juce::Path arc;
    arc.addCentredArc(cx, cy, r - 3.f, r - 3.f, 0.f, a0, a1, true);
    g.setColour(Theme::accentPrimary());
    g.strokePath(arc, juce::PathStrokeType(3.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour(Theme::textDisabled());
    juce::Path ring;
    ring.addCentredArc(cx, cy, r - 1.f, r - 1.f, 0.f, rotaryStartAngle, rotaryEndAngle, true);
    g.strokePath(ring, juce::PathStrokeType(1.2f));

    if (slider.isMouseOverOrDragging())
    {
        g.setColour(Theme::accentHot().withAlpha(0.25f));
        g.fillEllipse(bounds.reduced(6.f));
    }

    g.setColour(Theme::textPrimary());
    g.fillEllipse(cx - 3.f, cy - 3.f, 6.f, 6.f);
}

void WolfsDenLookAndFeel::drawLinearSlider(juce::Graphics& g,
                                           int x,
                                           int y,
                                           int width,
                                           int height,
                                           float sliderPos,
                                           float minSliderPos,
                                           float maxSliderPos,
                                           const juce::Slider::SliderStyle style,
                                           juce::Slider& slider)
{
    juce::ignoreUnused(minSliderPos, maxSliderPos);
    if (style == juce::Slider::LinearHorizontal)
    {
        const float cy = (float)y + (float)height * 0.5f;
        juce::Rectangle<float> track((float)x, cy - 3.f, (float)width, 6.f);
        g.setColour(Theme::backgroundDark());
        g.fillRoundedRectangle(track, 3.f);
        juce::Rectangle<float> fill = track.withWidth(juce::jmax(0.f, sliderPos - track.getX()));
        g.setColour(Theme::accentPrimary());
        g.fillRoundedRectangle(fill, 3.f);
        juce::Rectangle<float> th(sliderPos - 4.f, cy - 6.f, 8.f, 12.f);
        g.setColour(Theme::accentHot());
        g.fillRoundedRectangle(th, 2.f);
        if (slider.isMouseOverOrDragging())
        {
            g.setColour(Theme::accentHot().withAlpha(0.22f));
            g.fillEllipse(th.expanded(3.f));
        }
    }
    else
    {
        const float cx = (float)x + (float)width * 0.5f;
        juce::Rectangle<float> track(cx - 3.f, (float)y, 6.f, (float)height);
        g.setColour(Theme::backgroundDark());
        g.fillRoundedRectangle(track, 3.f);
        juce::Rectangle<float> fill = track.withBottom(sliderPos);
        g.setColour(Theme::accentPrimary());
        g.fillRoundedRectangle(fill, 3.f);
        juce::Rectangle<float> th(cx - 6.f, sliderPos - 4.f, 12.f, 8.f);
        g.setColour(Theme::accentHot());
        g.fillRoundedRectangle(th, 2.f);
        if (slider.isMouseOverOrDragging())
        {
            g.setColour(Theme::accentHot().withAlpha(0.22f));
            g.fillEllipse(th.expanded(3.f));
        }
    }
}

void WolfsDenLookAndFeel::drawButtonBackground(juce::Graphics& g,
                                               juce::Button& button,
                                               const juce::Colour&,
                                               bool highlighted,
                                               bool down)
{
    auto r = button.getLocalBounds().toFloat().reduced(1.f);
    juce::Colour fill = button.getToggleState() ? Theme::accentPrimary().withAlpha(0.45f) : Theme::panelSurface();
    if (down)
        fill = fill.brighter(0.08f);
    else if (highlighted)
        fill = fill.brighter(0.04f);
    g.setColour(fill);
    g.fillRoundedRectangle(r, (float)juce::jmin(10, button.getHeight() / 2));

    if (button.getToggleState() || highlighted)
    {
        g.setColour(Theme::accentHot().withAlpha(0.35f));
        g.drawRoundedRectangle(r.reduced(0.5f), (float)juce::jmin(10, button.getHeight() / 2), 1.f);
    }
}

void WolfsDenLookAndFeel::drawToggleButton(juce::Graphics& g,
                                           juce::ToggleButton& button,
                                           bool highlighted,
                                           bool down)
{
    juce::ignoreUnused(down);
    auto r = button.getLocalBounds().toFloat();
    const float ledR = juce::jmin(r.getHeight() - 6.f, 14.f);
    juce::Rectangle<float> led(4.f, r.getCentreY() - ledR * 0.5f, ledR, ledR);

    if (button.getToggleState())
    {
        g.setColour(Theme::success());
        g.fillEllipse(led);
        g.setColour(Theme::success().brighter(0.3f));
        g.drawEllipse(led, 1.f);
    }
    else
    {
        g.setColour(Theme::textDisabled());
        g.fillEllipse(led);
    }

    if (highlighted)
    {
        g.setColour(Theme::accentHot().withAlpha(0.2f));
        g.fillEllipse(led.expanded(2.f));
    }

    g.setColour(Theme::textPrimary());
    g.setFont(Theme::fontValue());
    g.drawText(button.getButtonText(), (int)(led.getRight() + 6.f), 0, button.getWidth(), button.getHeight(), juce::Justification::centredLeft);
}

void WolfsDenLookAndFeel::drawComboBox(juce::Graphics& g,
                                       int width,
                                       int height,
                                       bool isButtonDown,
                                       int buttonX,
                                       int buttonY,
                                       int buttonW,
                                       int buttonH,
                                       juce::ComboBox& box)
{
    juce::ignoreUnused(isButtonDown, buttonX, buttonY, buttonW, buttonH);
    auto r = juce::Rectangle<int>(0, 0, width, height).toFloat().reduced(1.f);
    g.setColour(Theme::panelSurface());
    g.fillRoundedRectangle(r, 4.f);
    g.setColour(Theme::textDisabled());
    g.drawRoundedRectangle(r, 4.f, 1.f);

    g.setColour(Theme::textPrimary());
    g.setFont(Theme::fontValue());
    g.drawFittedText(box.getText(), r.reduced(6.f, 0.f).toNearestInt(), juce::Justification::centredLeft, 2);

    juce::Path p;
    p.addTriangle(r.getRight() - 14.f, r.getCentreY() - 2.f, r.getRight() - 6.f, r.getCentreY() - 2.f, r.getRight() - 10.f, r.getCentreY() + 4.f);
    g.setColour(Theme::textSecondary());
    g.fillPath(p);
}

juce::Font WolfsDenLookAndFeel::getLabelFont(juce::Label& label)
{
    if (label.getFont().getHeight() > 0.1f)
        return label.getFont();
    return Theme::fontLabel();
}

} // namespace wolfsden::ui
