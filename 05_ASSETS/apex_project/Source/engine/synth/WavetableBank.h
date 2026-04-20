#pragma once

#include <cmath>

namespace wolfsden
{

constexpr int kWavetableSize = 256;

// Internal helper for generating mathematical tables at runtime
template<typename Func>
std::array<double, kWavetableSize> makeWavetable(Func f)
{
    std::array<double, kWavetableSize> table{};
    for (size_t i = 0; i < (size_t)kWavetableSize; ++i)
    {
        double phase = static_cast<double>(i) / static_cast<double>(kWavetableSize);
        table[i] = f(phase);
    }
    // Normalize to [-1, 1]
    double peak = 0.0;
    for (size_t i = 0; i < (size_t)kWavetableSize; ++i)
    {
        if (std::abs(table[i]) > peak)
            peak = std::abs(table[i]);
    }
    if (peak > 0.0)
    {
        for (size_t i = 0; i < (size_t)kWavetableSize; ++i)
            table[i] /= peak;
    }
    return table;
}

// 1. Sine
const auto kWT_Sine = makeWavetable([](double p) {
    return std::sin(2.0 * M_PI * p);
});

// 2. Saw (simple alias for generation, PolyBLEP applied in DSP)
const auto kWT_Saw = makeWavetable([](double p) {
    return 2.0 * p - 1.0;
});

// 3. Square
const auto kWT_Square = makeWavetable([](double p) {
    return p < 0.5 ? 1.0 : -1.0;
});

// 4. Triangle
const auto kWT_Triangle = makeWavetable([](double p) {
    return 2.0 * std::abs(2.0 * p - 1.0) - 1.0;
});

// 5. Soft Saw (Saw with 1st order LP filter equivalent applied via Fourier series)
const auto kWT_SoftSaw = makeWavetable([](double p) {
    double sum = 0.0;
    for (int n = 1; n <= 10; ++n)
        sum += std::sin(2.0 * M_PI * n * p) / (n * n); // 1/n^2 gives softer edge
    return sum;
});

// 6. Fat Square (Square with slightly unequal duty cycle 45/55)
const auto kWT_FatSquare = makeWavetable([](double p) {
    return p < 0.45 ? 1.0 : -1.0;
});

// 7. Organ (Sine + 2nd + 3rd harmonic)
const auto kWT_Organ = makeWavetable([](double p) {
    return std::sin(2.0 * M_PI * p) +
           0.5 * std::sin(2.0 * M_PI * 2.0 * p) +
           0.25 * std::sin(2.0 * M_PI * 3.0 * p);
});

// 8. Electric Piano (Sine + inharmonic partials)
const auto kWT_ElectricPiano = makeWavetable([](double p) {
    return std::sin(2.0 * M_PI * p) +
           0.4 * std::sin(2.0 * M_PI * 4.0 * p) +
           0.2 * std::sin(2.0 * M_PI * 7.0 * p) +
           0.1 * std::sin(2.0 * M_PI * 14.0 * p);
});

// 9. Strings (Rich overtone stack 1/n)
const auto kWT_Strings = makeWavetable([](double p) {
    double sum = 0.0;
    for (int n = 1; n <= 8; ++n)
        sum += std::sin(2.0 * M_PI * n * p) / n;
    return sum;
});

// 10. Brass (Odd partials dominant, 1/n amplitude)
const auto kWT_Brass = makeWavetable([](double p) {
    double sum = 0.0;
    for (int n = 1; n <= 9; n += 2)
        sum += std::sin(2.0 * M_PI * n * p) / n;
    return sum;
});

// 11. Vocal Aah (Formant approximation)
const auto kWT_VocalAah = makeWavetable([](double p) {
    return std::sin(2.0 * M_PI * p) +
           0.8 * std::sin(2.0 * M_PI * 2.0 * p) +
           0.6 * std::sin(2.0 * M_PI * 4.0 * p) +
           0.3 * std::sin(2.0 * M_PI * 5.0 * p);
});

// 12. Vocal Eeh (Higher formant center)
const auto kWT_VocalEeh = makeWavetable([](double p) {
    return std::sin(2.0 * M_PI * p) +
           0.4 * std::sin(2.0 * M_PI * 3.0 * p) +
           0.7 * std::sin(2.0 * M_PI * 6.0 * p) +
           0.2 * std::sin(2.0 * M_PI * 8.0 * p);
});

// 13. Bell (Inharmonic partials)
const auto kWT_Bell = makeWavetable([](double p) {
    return std::sin(2.0 * M_PI * p) +
           0.8 * std::sin(2.0 * M_PI * 2.756 * p) +
           0.5 * std::sin(2.0 * M_PI * 5.404 * p) +
           0.2 * std::sin(2.0 * M_PI * 8.933 * p);
});

// 14. Digital (Clipped sine)
const auto kWT_Digital = makeWavetable([](double p) {
    double s = std::sin(2.0 * M_PI * p) * 2.0;
    return std::max(-1.0, std::min(1.0, s));
});

// 15. Noise Sine (Sine + high frequency noise)
const auto kWT_NoiseSine = makeWavetable([](double p) {
    // Pseudo-random deterministic noise based on phase
    double noise = std::sin(2.0 * M_PI * 137.0 * p) * 0.1;
    return std::sin(2.0 * M_PI * p) + noise;
});

// 16. Pulse 25
const auto kWT_Pulse25 = makeWavetable([](double p) {
    return p < 0.25 ? 1.0 : -1.0;
});

// 17. Pulse 12
const auto kWT_Pulse12 = makeWavetable([](double p) {
    return p < 0.12 ? 1.0 : -1.0;
});

// 18. Half Sine
const auto kWT_HalfSine = makeWavetable([](double p) {
    return std::abs(std::sin(2.0 * M_PI * p));
});

// 19. Staircase (Quantized saw)
const auto kWT_Staircase = makeWavetable([](double p) {
    return std::floor((2.0 * p - 1.0) * 4.0) / 4.0;
});

// 20. Additive Sweep
const auto kWT_AdditiveSweep = makeWavetable([](double p) {
    double sum = 0.0;
    for (int n = 1; n <= 15; n += 2)
        sum += std::sin(2.0 * M_PI * n * p) / n;
    return sum;
});


struct WavetableEntry
{
    const char* name;
    const double* data;
    int len;
};

constexpr WavetableEntry kFactoryWavetables[] = {
    { "Sine", kWT_Sine.data(), kWavetableSize },
    { "Saw", kWT_Saw.data(), kWavetableSize },
    { "Square", kWT_Square.data(), kWavetableSize },
    { "Triangle", kWT_Triangle.data(), kWavetableSize },
    { "Soft Saw", kWT_SoftSaw.data(), kWavetableSize },
    { "Fat Square", kWT_FatSquare.data(), kWavetableSize },
    { "Organ", kWT_Organ.data(), kWavetableSize },
    { "Electric Piano", kWT_ElectricPiano.data(), kWavetableSize },
    { "Strings", kWT_Strings.data(), kWavetableSize },
    { "Brass", kWT_Brass.data(), kWavetableSize },
    { "Vocal Aah", kWT_VocalAah.data(), kWavetableSize },
    { "Vocal Eeh", kWT_VocalEeh.data(), kWavetableSize },
    { "Bell", kWT_Bell.data(), kWavetableSize },
    { "Digital", kWT_Digital.data(), kWavetableSize },
    { "Noise Sine", kWT_NoiseSine.data(), kWavetableSize },
    { "Pulse 25", kWT_Pulse25.data(), kWavetableSize },
    { "Pulse 12", kWT_Pulse12.data(), kWavetableSize },
    { "Half Sine", kWT_HalfSine.data(), kWavetableSize },
    { "Staircase", kWT_Staircase.data(), kWavetableSize },
    { "Additive Sweep", kWT_AdditiveSweep.data(), kWavetableSize },
};

constexpr int kNumFactoryWavetables = 20;

} // namespace wolfsden