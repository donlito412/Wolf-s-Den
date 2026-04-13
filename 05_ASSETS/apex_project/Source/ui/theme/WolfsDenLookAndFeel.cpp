#include "WolfsDenLookAndFeel.h"

namespace wolfsden::ui
{
void WolfsDenLookAndFeel::configureRotarySlider(juce::Slider& s) noexcept
{
    using F = juce::MathConstants<float>;
    // JUCE angles: clockwise from 12 o'clock. Min at bottom-left (~7:30) = 5π/4; 270° sweep; ~90° gap.
    const float a0 = F::pi * 1.25f;             // 225° — SW (not 3π/4, which is SE in this convention)
    const float a1 = a0 + F::pi * 1.5f;         // +270° as angle increases (clockwise along the dial)
    s.setRotaryParameters({ a0, a1, true });
}

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

    const float aStart = rotaryStartAngle;
    const float aEnd = rotaryEndAngle;
    const float aCurr = aStart + sliderPosProportional * (aEnd - aStart);
    const float sweep = aEnd - aStart;

    // True circle (inscribed) — avoids stretched ovals when the slider cell is rectangular
    const float faceD = juce::jmax(4.f, juce::jmin(bounds.getWidth(), bounds.getHeight()) - 10.f);
    const auto face = juce::Rectangle<float>(faceD, faceD).withCentre({ cx, cy });

    g.setColour(Theme::panelSurface().darker(0.03f));
    g.fillEllipse(face);

    g.setColour(Theme::textDisabled().withAlpha(0.45f));
    g.drawEllipse(face, 1.f);

    // JUCE rotary angles are clockwise from 12 o'clock (same as Path::addCentredArc / Point::getPointOnCircumference)
    auto polar = [cx, cy](float rad, float angJuce) -> juce::Point<float>
    {
        return { cx + rad * std::sin(angJuce), cy - rad * std::cos(angJuce) };
    };

    const float tickOuter = r - 2.f;
    const int numTicks = juce::jlimit(9, 17, (int)(r * 0.28f));
    for (int i = 0; i <= numTicks; ++i)
    {
        const float t = (float)i / (float)numTicks;
        const float ang = aStart + t * sweep;
        const bool major = (i % 2 == 0);
        const float len = major ? 4.5f : 2.5f;
        const float inner = tickOuter - len;
        const auto p0 = polar(inner, ang);
        const auto p1 = polar(tickOuter, ang);
        g.setColour(Theme::textDisabled().withAlpha(major ? 0.9f : 0.55f));
        g.drawLine(p0.x, p0.y, p1.x, p1.y, major ? 1.35f : 1.f);
    }

    // Full-range track arc (subtle)
    juce::Path trackArc;
    trackArc.addCentredArc(cx, cy, r - 8.f, r - 8.f, 0.f, aStart, aEnd, true);
    g.setColour(Theme::textDisabled().withAlpha(0.35f));
    g.strokePath(trackArc, juce::PathStrokeType(1.f));

    // Value arc — dotted cyan (like reference MASTER VOL arc)
    if (sliderPosProportional > 0.001f)
    {
        juce::Path valArc;
        valArc.addCentredArc(cx, cy, r - 8.f, r - 8.f, 0.f, aStart, aCurr, true);
        const float dashLens[] = { 2.2f, 3.8f };
        juce::Path dashed;
        juce::PathStrokeType(1.6f).createDashedStroke(dashed, valArc, dashLens, 2, {});
        g.setColour(Theme::accentHot());
        g.strokePath(dashed, juce::PathStrokeType(1.6f));
    }

    // Rim pointer — scale down on small sliders so compact FX / Theory knobs stay readable
    const float dotR = r - 9.f;
    const auto thumb = polar(dotR, aCurr);
    const float thumbSide = juce::jlimit(4.f, 6.f, faceD * 0.11f);
    g.setColour(Theme::accentHot());
    g.fillEllipse(juce::Rectangle<float>(thumbSide, thumbSide).withCentre(thumb));
    g.setColour(Theme::accentPrimary().withAlpha(0.5f));
    g.drawEllipse(juce::Rectangle<float>(thumbSide, thumbSide).withCentre(thumb), 1.f);

    if (slider.isMouseOverOrDragging())
    {
        g.setColour(Theme::accentHot().withAlpha(0.35f));
        g.drawEllipse(face.expanded(1.f), 1.f);
    }
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
    const float hoverA = getHoverAlpha(button, highlighted);

    auto r = button.getLocalBounds().toFloat().reduced(1.f);
    const float cornerR = (float)juce::jmin(10, button.getHeight() / 2);

    // Base fill — blend from panel surface toward highlight as hoverA rises
    juce::Colour fill = button.getToggleState()
                        ? Theme::accentPrimary().withAlpha(0.45f)
                        : Theme::panelSurface();
    if (down)
        fill = fill.brighter(0.12f);
    else if (hoverA > 0.f)
        fill = fill.brighter(0.08f * hoverA);

    g.setColour(fill);
    g.fillRoundedRectangle(r, cornerR);

    // Glow outline — fully animated over 80ms
    const float glowStrength = button.getToggleState() ? 1.f : hoverA;
    if (glowStrength > 0.02f)
    {
        g.setColour(Theme::accentHot().withAlpha(0.35f * glowStrength));
        g.drawRoundedRectangle(r.reduced(0.5f), cornerR, 1.f);
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

    // Do not draw the item text here — ComboBox paints its internal Label on top; drawing
    // here too stacks two strings (FX / Mod / Browse looked “double”).

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

juce::Font WolfsDenLookAndFeel::getComboBoxFont(juce::ComboBox& box)
{
    juce::ignoreUnused(box);
    return Theme::fontValue();
}

// =============================================================================
// 80ms hover animation
// =============================================================================

float WolfsDenLookAndFeel::getHoverAlpha(juce::Component& c, bool hovering)
{
    auto it = hoverMap.find(&c);
    if (it == hoverMap.end())
    {
        // First time seen — register a listener so we can clean up on destruction
        c.addComponentListener(this);
        hoverMap[&c] = { hovering ? 1.f : 0.f, hovering };
        return hoverMap[&c].alpha;
    }

    auto& hs = it->second;
    if (hs.hovering != hovering)
    {
        hs.hovering = hovering;
        if (!isTimerRunning())
            startTimerHz(60);
    }
    return hs.alpha;
}

void WolfsDenLookAndFeel::timerCallback()
{
    // ~80ms at 60fps  →  step ≈ 0.208 per frame
    static constexpr float kStep = 1.f / (0.080f * 60.f);

    bool anyAnimating = false;
    for (auto& [comp, hs] : hoverMap)
    {
        const float target = hs.hovering ? 1.f : 0.f;
        const float delta  = target - hs.alpha;
        if (std::abs(delta) > 0.004f)
        {
            hs.alpha += (delta > 0.f ? kStep : -kStep);
            hs.alpha  = juce::jlimit(0.f, 1.f, hs.alpha);
            comp->repaint();
            anyAnimating = true;
        }
        else
        {
            hs.alpha = target;
        }
    }
    if (!anyAnimating)
        stopTimer();
}

void WolfsDenLookAndFeel::componentBeingDeleted(juce::Component& c)
{
    hoverMap.erase(&c);
    c.removeComponentListener(this);
}

} // namespace wolfsden::ui
