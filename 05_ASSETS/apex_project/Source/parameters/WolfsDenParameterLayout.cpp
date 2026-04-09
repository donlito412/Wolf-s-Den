#include "WolfsDenParameterLayout.h"

#include <utility>
#include <vector>

namespace wolfsden
{
namespace
{
constexpr int kParamVersion = 1;

inline juce::ParameterID pid(const juce::String& s)
{
    return { s, kParamVersion };
}

juce::NormalisableRange<float> logCutoffRange()
{
    return { 20.f, 20000.f, 0.f, 0.25f };
}

void addLayerParams(int layerIndex,
                    std::vector<std::unique_ptr<juce::RangedAudioParameter>>& out,
                    const juce::StringArray& oscChoices,
                    const juce::StringArray& filterTypeChoices,
                    const juce::StringArray& lfoShapeChoices)
{
    const juce::String pfx = "layer" + juce::String(layerIndex) + "_";
    const juce::StringArray filterRouteChoices({ "Serial", "Parallel" });

    out.push_back(std::make_unique<juce::AudioParameterFloat>(
        pid(pfx + "volume"),
        "Layer " + juce::String(layerIndex + 1) + " Volume",
        juce::NormalisableRange<float>(0.f, 1.f, 0.f, 0.5f),
        0.75f));

    out.push_back(std::make_unique<juce::AudioParameterFloat>(
        pid(pfx + "pan"),
        "Layer " + juce::String(layerIndex + 1) + " Pan",
        juce::NormalisableRange<float>(-1.f, 1.f, 0.f, 1.f),
        0.f));

    out.push_back(std::make_unique<juce::AudioParameterChoice>(
        pid(pfx + "osc_type"),
        "Layer " + juce::String(layerIndex + 1) + " Osc Type",
        oscChoices,
        0));

    out.push_back(std::make_unique<juce::AudioParameterInt>(
        pid(pfx + "tune_coarse"),
        "Layer " + juce::String(layerIndex + 1) + " Tune Coarse",
        -24,
        24,
        0));

    out.push_back(std::make_unique<juce::AudioParameterFloat>(
        pid(pfx + "tune_fine"),
        "Layer " + juce::String(layerIndex + 1) + " Tune Fine",
        juce::NormalisableRange<float>(-100.f, 100.f, 0.f, 1.f),
        0.f));

    out.push_back(std::make_unique<juce::AudioParameterFloat>(
        pid(pfx + "filter_cutoff"),
        "Layer " + juce::String(layerIndex + 1) + " Filter Cutoff",
        logCutoffRange(),
        5000.f));

    out.push_back(std::make_unique<juce::AudioParameterFloat>(
        pid(pfx + "filter_resonance"),
        "Layer " + juce::String(layerIndex + 1) + " Filter Resonance",
        juce::NormalisableRange<float>(0.1f, 40.f, 0.f, 0.3f),
        1.f));

    out.push_back(std::make_unique<juce::AudioParameterChoice>(
        pid(pfx + "filter_type"),
        "Layer " + juce::String(layerIndex + 1) + " Filter Type",
        filterTypeChoices,
        0));

    out.push_back(std::make_unique<juce::AudioParameterFloat>(
        pid(pfx + "amp_attack"),
        "Layer " + juce::String(layerIndex + 1) + " Amp Attack",
        juce::NormalisableRange<float>(0.001f, 10.f, 0.f, 0.4f),
        0.01f));

    out.push_back(std::make_unique<juce::AudioParameterFloat>(
        pid(pfx + "amp_decay"),
        "Layer " + juce::String(layerIndex + 1) + " Amp Decay",
        juce::NormalisableRange<float>(0.001f, 10.f, 0.f, 0.4f),
        0.1f));

    out.push_back(std::make_unique<juce::AudioParameterFloat>(
        pid(pfx + "amp_sustain"),
        "Layer " + juce::String(layerIndex + 1) + " Amp Sustain",
        juce::NormalisableRange<float>(0.f, 1.f, 0.f, 0.5f),
        0.8f));

    out.push_back(std::make_unique<juce::AudioParameterFloat>(
        pid(pfx + "amp_release"),
        "Layer " + juce::String(layerIndex + 1) + " Amp Release",
        juce::NormalisableRange<float>(0.001f, 15.f, 0.f, 0.4f),
        0.2f));

    out.push_back(std::make_unique<juce::AudioParameterFloat>(
        pid(pfx + "lfo2_rate"),
        "Layer " + juce::String(layerIndex + 1) + " LFO2 Rate",
        juce::NormalisableRange<float>(0.01f, 40.f, 0.f, 0.35f),
        2.f));

    out.push_back(std::make_unique<juce::AudioParameterFloat>(
        pid(pfx + "lfo2_depth"),
        "Layer " + juce::String(layerIndex + 1) + " LFO2 Depth",
        juce::NormalisableRange<float>(0.f, 1.f, 0.f, 0.5f),
        0.f));

    out.push_back(std::make_unique<juce::AudioParameterChoice>(
        pid(pfx + "lfo2_shape"),
        "Layer " + juce::String(layerIndex + 1) + " LFO2 Shape",
        lfoShapeChoices,
        0));

    out.push_back(std::make_unique<juce::AudioParameterChoice>(
        pid(pfx + "filter_routing"),
        "Layer " + juce::String(layerIndex + 1) + " Filter Routing",
        filterRouteChoices,
        0));

    out.push_back(std::make_unique<juce::AudioParameterInt>(
        pid(pfx + "unison_voices"),
        "Layer " + juce::String(layerIndex + 1) + " Unison Voices",
        1,
        8,
        1));

    out.push_back(std::make_unique<juce::AudioParameterFloat>(
        pid(pfx + "unison_detune"),
        "Layer " + juce::String(layerIndex + 1) + " Unison Detune",
        juce::NormalisableRange<float>(5.f, 80.f, 0.f, 0.4f),
        25.f));

    out.push_back(std::make_unique<juce::AudioParameterFloat>(
        pid(pfx + "unison_spread"),
        "Layer " + juce::String(layerIndex + 1) + " Unison Spread",
        juce::NormalisableRange<float>(0.f, 1.f, 0.f, 0.5f),
        0.5f));
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout makeParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    const juce::StringArray oscChoices(
        { "Sine", "Saw", "Square", "Triangle", "Wavetable", "Granular", "FM", "Sample" });
    const juce::StringArray filterTypeChoices(
        { "LP24", "LP12", "BP", "HP12", "Notch", "HP24", "Comb", "Formant" });
    const juce::StringArray scaleTypeChoices(
        { "Major", "Natural Minor", "Harmonic Minor", "Melodic Minor", "Dorian", "Phrygian", "Lydian",
          "Mixolydian", "Pent Maj", "Pent Min", "Blues", "Whole Tone", "Diminished", "Chromatic" });
    const juce::StringArray chordTypeChoices(
        { "Major", "Minor", "Dim", "Aug", "Sus2", "Sus4", "Maj7", "Min7", "Dom7", "Min7b5", "Dim7",
          "MinMaj7", "Add9", "Maj9", "Min9" });
    const juce::StringArray keysLockChoices(
        { "Off", "Remap", "Mute", "Chord Tones", "Chord Scales" });
    const juce::StringArray arpPatternChoices(
        { "Up", "Down", "Up-Down", "Order", "Chord", "Random" });
    const juce::StringArray lfoShapeChoices({ "Sine", "Triangle", "Saw Up", "Square", "Random", "S&H" });

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        pid("master_volume"),
        "Master Volume",
        juce::NormalisableRange<float>(0.f, 1.f, 0.f, 0.5f),
        0.8f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        pid("master_pitch"),
        "Master Pitch",
        juce::NormalisableRange<float>(-48.f, 48.f, 0.f, 0.4f),
        0.f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        pid("master_pan"),
        "Master Pan",
        juce::NormalisableRange<float>(-1.f, 1.f, 0.f, 1.f),
        0.f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        pid("filter_adsr_attack"),
        "Filter ADSR Attack",
        juce::NormalisableRange<float>(0.001f, 10.f, 0.f, 0.4f),
        0.01f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        pid("filter_adsr_decay"),
        "Filter ADSR Decay",
        juce::NormalisableRange<float>(0.001f, 10.f, 0.f, 0.4f),
        0.1f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        pid("filter_adsr_sustain"),
        "Filter ADSR Sustain",
        juce::NormalisableRange<float>(0.f, 1.f, 0.f, 0.5f),
        0.7f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        pid("filter_adsr_release"),
        "Filter ADSR Release",
        juce::NormalisableRange<float>(0.001f, 15.f, 0.f, 0.4f),
        0.15f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        pid("lfo_rate"),
        "LFO Rate",
        juce::NormalisableRange<float>(0.01f, 40.f, 0.f, 0.35f),
        2.f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        pid("lfo_depth"),
        "LFO Depth",
        juce::NormalisableRange<float>(0.f, 1.f, 0.f, 0.5f),
        0.f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        pid("lfo_shape"), "LFO Shape", lfoShapeChoices, 0));

    params.push_back(std::make_unique<juce::AudioParameterInt>(
        pid("theory_scale_root"), "Scale Root", 0, 11, 0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        pid("theory_scale_type"), "Scale Type", scaleTypeChoices, 0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        pid("theory_chord_type"), "Chord Type", chordTypeChoices, 0));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        pid("theory_voice_leading"), "Voice Leading", false));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        pid("midi_keys_lock_mode"), "Keys Lock Mode", keysLockChoices, 0));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        pid("midi_chord_mode"), "Chord Mode", false));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        pid("midi_arp_on"), "Arpeggiator", false));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        pid("midi_arp_rate"),
        "Arp Rate",
        juce::NormalisableRange<float>(0.25f, 32.f, 0.f, 0.45f),
        4.f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        pid("midi_arp_pattern"), "Arp Pattern", arpPatternChoices, 0));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        pid("midi_arp_latch"), "Arp Latch", false));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        pid("midi_arp_swing"),
        "Arp Swing",
        juce::NormalisableRange<float>(0.f, 0.5f, 0.f, 1.f),
        0.f));

    params.push_back(std::make_unique<juce::AudioParameterInt>(
        pid("midi_arp_octaves"), "Arp Octaves", 1, 4, 1));

    params.push_back(std::make_unique<juce::AudioParameterInt>(
        pid("midi_chord_inversion"), "Chord Inversion", 0, 3, 0));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        pid("midi_arp_sync_ppq"), "Arp Sync to Transport", true));

    for (int si = 0; si < 32; ++si)
    {
        const juce::String idx = juce::String(si).paddedLeft('0', 2);
        const juce::String pfx = "midi_arp_s" + idx + "_";
        const juce::String lab = "Arp S" + juce::String(si + 1);
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            pid(pfx + "on"), lab + " On", true));
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            pid(pfx + "vel"), lab + " Vel", 1, 127, 100));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            pid(pfx + "dur"),
            lab + " Dur",
            juce::NormalisableRange<float>(0.1f, 2.f, 0.f, 0.5f),
            1.f));
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            pid(pfx + "trn"), lab + " Trn", -24, 24, 0));
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            pid(pfx + "rkt"), lab + " Rkt", 1, 4, 1));
    }

    const juce::StringArray xyPhysicsChoices({ "Direct", "Inertia", "Chaos" });
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        pid("perf_xy_physics"), "XY Physics", xyPhysicsChoices, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        pid("fx_reverb_mix"),
        "Reverb Mix",
        juce::NormalisableRange<float>(0.f, 1.f, 0.f, 0.5f),
        0.f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        pid("fx_delay_mix"),
        "Delay Mix",
        juce::NormalisableRange<float>(0.f, 1.f, 0.f, 0.5f),
        0.f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        pid("fx_chorus_mix"),
        "Chorus Mix",
        juce::NormalisableRange<float>(0.f, 1.f, 0.f, 0.5f),
        0.f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        pid("fx_compressor"),
        "Master Compressor",
        juce::NormalisableRange<float>(0.f, 1.f, 0.f, 0.5f),
        0.f));

    const juce::StringArray fxTypeNames(
        { "Off",
          "Compressor",
          "Limiter",
          "Gate",
          "Parametric EQ (4-band)",
          "HPF (12 dB/oct)",
          "LPF (12 dB/oct)",
          "Soft Clip",
          "Hard Clip",
          "Bit Crusher",
          "Waveshaper",
          "Chorus",
          "Flanger",
          "Phaser",
          "Vibrato",
          "Tremolo",
          "Auto-Pan",
          "Stereo Delay",
          "Ping-Pong Delay",
          "Reverb (algorithmic hall)",
          "Reverb (bright / plate-style)",
          "Reverb (spring-style decay)",
          "Stereo width",
          "Mono blend (L+R)" });

    for (int c = 0; c < 4; ++c)
    {
        const int defIdx = c == 0 ? 19 : (c == 1 ? 17 : (c == 2 ? 11 : 1));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            pid("fx_c" + juce::String(c) + "_type"),
            "FX Common " + juce::String(c + 1) + " Type",
            fxTypeNames,
            defIdx));
    }

    for (int L = 0; L < 4; ++L)
        for (int s = 0; s < 4; ++s)
        {
            params.push_back(std::make_unique<juce::AudioParameterChoice>(
                pid("fx_l" + juce::String(L) + "_s" + juce::String(s) + "_type"),
                "FX L" + juce::String(L + 1) + " S" + juce::String(s + 1) + " Type",
                fxTypeNames,
                0));
            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                pid("fx_l" + juce::String(L) + "_s" + juce::String(s) + "_mix"),
                "FX L" + juce::String(L + 1) + " S" + juce::String(s + 1) + " Mix",
                juce::NormalisableRange<float>(0.f, 1.f, 0.f, 0.5f),
                0.f));
        }

    const int defMasterTypes[4] = { 2, 0, 0, 22 };
    const float defMasterMixes[4] = { 1.f, 0.f, 0.f, 0.35f };
    for (int m = 0; m < 4; ++m)
    {
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            pid("fx_m" + juce::String(m) + "_type"),
            "FX Master " + juce::String(m + 1) + " Type",
            fxTypeNames,
            defMasterTypes[m]));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            pid("fx_m" + juce::String(m) + "_mix"),
            "FX Master " + juce::String(m + 1) + " Mix",
            juce::NormalisableRange<float>(0.f, 1.f, 0.f, 0.5f),
            defMasterMixes[m]));
    }

    static const char* eqBandSuffix[4] = { " Low shelf (dB)", " Low-mid (dB)", " High-mid (dB)", " High shelf (dB)" };
    for (int si = 0; si < 24; ++si)
    {
        const juce::String idx = juce::String(si).paddedLeft('0', 2);
        const juce::String pfx = "fx_s" + idx + "_eq";
        for (int b = 0; b < 4; ++b)
        {
            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                pid(pfx + juce::String(b)),
                "FX slot " + juce::String(si + 1) + eqBandSuffix[(size_t)b],
                juce::NormalisableRange<float>(-18.f, 18.f, 0.01f, 0.5f),
                0.f));
        }
    }

    for (int L = 0; L < 4; ++L)
        addLayerParams(L, params, oscChoices, filterTypeChoices, lfoShapeChoices);

    return { params.begin(), params.end() };
}

} // namespace wolfsden
