#pragma once

namespace wolfsden
{

class MidiPipeline
{
public:
    MidiPipeline() = default;
    ~MidiPipeline() = default;

    MidiPipeline(const MidiPipeline&) = delete;
    MidiPipeline& operator=(const MidiPipeline&) = delete;
};

} // namespace wolfsden
