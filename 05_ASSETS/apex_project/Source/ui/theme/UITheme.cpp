#include "UITheme.h"

#include "WolfsDenFonts.h"

namespace wolfsden::ui
{
namespace
{
juce::Typeface::Ptr loadInter()
{
    return juce::Typeface::createSystemTypefaceFor(WolfsDenFonts::InterVariable_ttf, WolfsDenFonts::InterVariable_ttfSize);
}

juce::Font makeFont(float heightPts)
{
    if (auto tf = Theme::interTypeface())
        return juce::Font(juce::FontOptions().withHeight(heightPts).withTypeface(tf));
    return juce::Font(juce::FontOptions().withHeight(heightPts));
}
} // namespace

juce::Typeface::Ptr Theme::interTypeface()
{
    static const juce::Typeface::Ptr face = loadInter();
    return face;
}

juce::Font Theme::fontLabel() { return makeFont(10.0f); }
juce::Font Theme::fontValue() { return makeFont(12.0f); }
juce::Font Theme::fontPanelHeader() { return makeFont(14.0f); }
juce::Font Theme::fontPageHeader() { return makeFont(18.0f); }

} // namespace wolfsden::ui
