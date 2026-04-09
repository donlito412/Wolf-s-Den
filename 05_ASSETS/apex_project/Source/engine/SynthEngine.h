#pragma once

namespace wolfsden
{

class SynthEngine
{
public:
    SynthEngine() = default;
    ~SynthEngine() = default;

    SynthEngine(const SynthEngine&) = delete;
    SynthEngine& operator=(const SynthEngine&) = delete;
};

} // namespace wolfsden
