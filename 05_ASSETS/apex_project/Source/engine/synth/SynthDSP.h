#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace wolfsden::dsp
{

inline constexpr double twoPi = 6.28318530717958647692;

inline double midiNoteToHz(int note) noexcept
{
    return 440.0 * std::pow(2.0, (note - 69) / 12.0);
}

inline double polyBlep(double t, double dt) noexcept
{
    if (t < dt)
    {
        const double x = t / dt;
        return x + x - x * x - 1.0;
    }
    if (t > 1.0 - dt)
    {
        const double x = (t - 1.0) / dt;
        return x * x + x + x + 1.0;
    }
    return 0.0;
}

inline double blepSaw(double phase, double inc) noexcept
{
    double v = 2.0 * phase - 1.0;
    v -= polyBlep(phase, inc);
    return v;
}

inline double blepSquare(double phase, double inc) noexcept
{
    const double sq = phase < 0.5 ? 1.0 : -1.0;
    double v = sq;
    v += polyBlep(phase, inc);
    v -= polyBlep(std::fmod(phase + 0.5, 1.0), inc);
    return v;
}

inline double blepTriangle(double& triState, double phase, double inc) noexcept
{
    triState += 2.0 * blepSquare(phase, inc) * inc;
    if (triState > 1.0)
        triState -= 4.0 * inc;
    if (triState < -1.0)
        triState += 4.0 * inc;
    return triState;
}

struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1 = 0, z2 = 0;

    void reset() noexcept
    {
        z1 = z2 = 0;
    }

    double process(double x) noexcept
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        // Protect against NaN/Inf propagation and runaway state
        if (!(z1 > -1e9 && z1 < 1e9 && z2 > -1e9 && z2 < 1e9))
        {
            z1 = z2 = 0;
            return 0;
        }
        return y;
    }

    void setLowPass(double sr, double fc, double Q) noexcept
    {
        fc = std::clamp(fc, 20.0, sr * 0.45);
        const double w0 = twoPi * fc / sr;
        const double cosw = std::cos(w0);
        const double sinw = std::sin(w0);
        const double alpha = sinw / (2.0 * std::max(0.01, Q));
        const double a0i = 1.0 / (1.0 + alpha);
        b0 = ((1.0 - cosw) * 0.5) * a0i;
        b1 = (1.0 - cosw) * a0i;
        b2 = b0;
        a1 = (-2.0 * cosw) * a0i;
        a2 = (1.0 - alpha) * a0i;
    }

    void setHighPass(double sr, double fc, double Q) noexcept
    {
        fc = std::clamp(fc, 20.0, sr * 0.45);
        const double w0 = twoPi * fc / sr;
        const double cosw = std::cos(w0);
        const double sinw = std::sin(w0);
        const double alpha = sinw / (2.0 * std::max(0.01, Q));
        const double a0i = 1.0 / (1.0 + alpha);
        b0 = ((1.0 + cosw) * 0.5) * a0i;
        b1 = -(1.0 + cosw) * a0i;
        b2 = b0;
        a1 = (-2.0 * cosw) * a0i;
        a2 = (1.0 - alpha) * a0i;
    }

    void setBandPass(double sr, double fc, double Q) noexcept
    {
        fc = std::clamp(fc, 20.0, sr * 0.45);
        const double w0 = twoPi * fc / sr;
        const double cosw = std::cos(w0);
        const double sinw = std::sin(w0);
        const double alpha = sinw / (2.0 * std::max(0.01, Q));
        const double a0i = 1.0 / (1.0 + alpha);
        b0 = alpha * a0i;
        b1 = 0;
        b2 = -b0;
        a1 = (-2.0 * cosw) * a0i;
        a2 = (1.0 - alpha) * a0i;
    }

    void setNotch(double sr, double fc, double Q) noexcept
    {
        fc = std::clamp(fc, 20.0, sr * 0.45);
        const double w0 = twoPi * fc / sr;
        const double cosw = std::cos(w0);
        const double sinw = std::sin(w0);
        const double alpha = sinw / (2.0 * std::max(0.01, Q));
        const double a0i = 1.0 / (1.0 + alpha);
        b0 = a0i;
        b1 = (-2.0 * cosw) * a0i;
        b2 = b0;
        a1 = b1;
        a2 = (1.0 - alpha) * a0i;
    }
};

struct Adsr
{
    enum class Stage
    {
        idle,
        attack,
        decay,
        sustain,
        release
    } stage = Stage::idle;

    double level = 0;
    double attack = 0.01, decay = 0.1, sustain = 0.8, release = 0.2;
    double sr = 44100;

    void setSampleRate(double sampleRate) noexcept
    {
        sr = sampleRate;
    }

    void setParameters(double a, double d, double s, double r) noexcept
    {
        attack = std::max(0.0005, a);
        decay = std::max(0.0005, d);
        sustain = std::clamp(s, 0.0, 1.0);
        release = std::max(0.0005, r);
    }

    void noteOn() noexcept
    {
        level = 0;
        stage = Stage::attack;
    }

    /** Retrigger attack without snapping level to zero (reduces clicks on voice steal / legato). */
    void noteOnFromLevel(double startLevel) noexcept
    {
        level = std::clamp(startLevel, 0.0, 1.0);
        stage = Stage::attack;
    }

    void noteOff() noexcept
    {
        if (stage != Stage::idle)
            stage = Stage::release;
    }

    void reset() noexcept
    {
        stage = Stage::idle;
        level = 0;
    }

    double process() noexcept
    {
        switch (stage)
        {
            case Stage::idle:
                level = 0;
                break;
            case Stage::attack:
            {
                const double step = 1.0 / std::max(1.0, attack * sr);
                level = std::min(1.0, level + step);
                if (level >= 1.0)
                    stage = Stage::decay;
                break;
            }
            case Stage::decay:
            {
                const double step = (1.0 - sustain) / std::max(1.0, decay * sr);
                level = std::max(sustain, level - step);
                if (level <= sustain + 1e-6)
                    stage = Stage::sustain;
                break;
            }
            case Stage::sustain:
                level = sustain;
                break;
            case Stage::release:
            {
                const double step = level / std::max(1.0, release * sr);
                level = std::max(0.0, level - step);
                if (level <= 1e-8)
                {
                    level = 0;
                    stage = Stage::idle;
                }
                break;
            }
        }
        return level;
    }
};

inline float xorshift32(uint32_t& state) noexcept
{
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return (state & 0xffffff) / 8388608.0f - 1.0f;
}

struct Lfo
{
    double phase = 0;
    double sr = 44100;
    int shape = 0;
    uint32_t noiseState = 0xC0FFEEu;
    double sAndH = 0;

    void setSampleRate(double sampleRate) noexcept
    {
        sr = sampleRate;
    }

    void setShape(int s) noexcept
    {
        shape = s;
    }

    double tick(double hz) noexcept
    {
        const double inc = hz / sr;
        const bool wrapped = (phase + inc) >= 1.0;
        phase += inc;
        if (phase >= 1.0)
            phase -= 1.0;

        if ((shape == 4 || shape == 5) && wrapped)
            sAndH = (double)xorshift32(noiseState);

        switch (shape)
        {
            case 0:
                return std::sin(twoPi * phase);
            case 1:
                return 4.0 * std::abs(phase - 0.5) - 1.0;
            case 2:
                return 2.0 * phase - 1.0; // saw up
            case 3:
                return phase < 0.5 ? 1.0 : -1.0;
            case 4:
                return sAndH; // S&H
            case 5:
                return (double)xorshift32(noiseState); // random per sample
            case 6:
                return 1.0 - 2.0 * phase; // saw down
            case 7:
                return (double)xorshift32(noiseState); // white noise
            default:
                return std::sin(twoPi * phase);
        }
    }
};

} // namespace wolfsden::dsp
