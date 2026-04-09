#pragma once

namespace wolfsden
{

class TheoryEngine
{
public:
    TheoryEngine() = default;
    ~TheoryEngine() = default;

    TheoryEngine(const TheoryEngine&) = delete;
    TheoryEngine& operator=(const TheoryEngine&) = delete;
};

} // namespace wolfsden
