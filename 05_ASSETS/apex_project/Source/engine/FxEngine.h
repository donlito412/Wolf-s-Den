#pragma once

namespace wolfsden
{

class FxEngine
{
public:
    FxEngine() = default;
    ~FxEngine() = default;

    FxEngine(const FxEngine&) = delete;
    FxEngine& operator=(const FxEngine&) = delete;
};

} // namespace wolfsden
