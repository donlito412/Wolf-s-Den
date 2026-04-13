#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "TheoryEngine.h"

namespace wolfsden
{

juce::AudioProcessorValueTreeState::ParameterLayout makeParameterLayout(const TheoryEngine* theory);

} // namespace wolfsden
