#pragma once

#include <juce_graphics/juce_graphics.h>

namespace wolfsden::ui
{

/** TASK_009 design tokens (hex → Colour) + embedded Inter typography. */
struct Theme
{
    static juce::Colour backgroundDark() { return juce::Colour(0xff0d0d0f); }
    static juce::Colour backgroundMid() { return juce::Colour(0xff1a1a22); }
    static juce::Colour panelSurface() { return juce::Colour(0xff22222e); }
    static juce::Colour accentPrimary() { return juce::Colour(0xff7b5ea7); }
    static juce::Colour accentHot() { return juce::Colour(0xffc084fc); }
    static juce::Colour accentAlt() { return juce::Colour(0xff38bdf8); }
    static juce::Colour textPrimary() { return juce::Colour(0xfff0f0f5); }
    static juce::Colour textSecondary() { return juce::Colour(0xff8888a8); }
    static juce::Colour textDisabled() { return juce::Colour(0xff444460); }
    static juce::Colour success() { return juce::Colour(0xff4ade80); }
    static juce::Colour warning() { return juce::Colour(0xfffbbf24); }
    static juce::Colour error() { return juce::Colour(0xfff87171); }

    /** Inter (embedded variable font). May be null if binary data fails. */
    static juce::Typeface::Ptr interTypeface();

    static juce::Font fontLabel();
    static juce::Font fontValue();
    static juce::Font fontPanelHeader();
    static juce::Font fontPageHeader();
};

} // namespace wolfsden::ui
