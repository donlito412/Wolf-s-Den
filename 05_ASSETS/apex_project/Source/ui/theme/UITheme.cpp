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
    auto opts = juce::FontOptions()
                    .withHeight(heightPts)
                    .withMetricsKind(juce::TypefaceMetricsKind::legacy);
    if (auto tf = Theme::interTypeface())
        opts = opts.withTypeface(tf);
    return juce::Font(opts);
}
} // namespace

juce::Typeface::Ptr Theme::interTypeface()
{
    static const juce::Typeface::Ptr face = loadInter();
    return face;
}

juce::Font Theme::fontLabel() { return makeFont(11.0f); }
juce::Font Theme::fontValue() { return makeFont(13.0f); }
juce::Font Theme::fontPanelHeader() { return makeFont(15.0f); }
juce::Font Theme::fontPageHeader() { return makeFont(19.0f); }

} // namespace wolfsden::ui
