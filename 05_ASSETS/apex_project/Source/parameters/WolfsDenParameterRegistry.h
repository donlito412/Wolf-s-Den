#pragma once

#include <cstdint>

namespace wolfsden::pid
{

/** Fixed public registry (uint32). Do not change values after v1.0 — maps to stable JUCE ParameterID strings. */
enum class Registry : uint32_t
{
    master_volume = 0x01000001u,
    master_pitch = 0x01000002u,
    master_pan = 0x01000003u,

    filter_adsr_attack = 0x01000101u,
    filter_adsr_decay = 0x01000102u,
    filter_adsr_sustain = 0x01000103u,
    filter_adsr_release = 0x01000104u,

    lfo_rate = 0x01000201u,
    lfo_depth = 0x01000202u,
    lfo_shape = 0x01000203u,

    theory_scale_root = 0x01000301u,
    theory_scale_type = 0x01000302u,
    theory_chord_type = 0x01000303u,
    theory_voice_leading = 0x01000304u,

    midi_keys_lock_mode = 0x01000401u,
    midi_chord_mode = 0x01000402u,
    midi_arp_on = 0x01000403u,
    midi_arp_rate = 0x01000404u,
    midi_arp_pattern = 0x01000405u,
    midi_arp_latch = 0x01000406u,
    midi_arp_swing = 0x01000407u,
    midi_arp_octaves = 0x01000408u,
    midi_chord_inversion = 0x01000409u,
    midi_arp_sync_ppq = 0x0100040Au,

    perf_xy_physics = 0x01000601u,

    fx_reverb_mix = 0x01000501u,
    fx_delay_mix = 0x01000502u,
    fx_chorus_mix = 0x01000503u,
    fx_compressor = 0x01000504u,

    layer0_volume = 0x02000001u,
    layer0_pan = 0x02000002u,
    layer0_osc_type = 0x02000003u,
    layer0_tune_coarse = 0x02000004u,
    layer0_tune_fine = 0x02000005u,
    layer0_filter_cutoff = 0x02000006u,
    layer0_filter_resonance = 0x02000007u,
    layer0_filter_type = 0x02000008u,
    layer0_amp_attack = 0x02000009u,
    layer0_amp_decay = 0x0200000Au,
    layer0_amp_sustain = 0x0200000Bu,
    layer0_amp_release = 0x0200000Cu,

    layer1_volume = 0x03000001u,
    layer1_pan = 0x03000002u,
    layer1_osc_type = 0x03000003u,
    layer1_tune_coarse = 0x03000004u,
    layer1_tune_fine = 0x03000005u,
    layer1_filter_cutoff = 0x03000006u,
    layer1_filter_resonance = 0x03000007u,
    layer1_filter_type = 0x03000008u,
    layer1_amp_attack = 0x03000009u,
    layer1_amp_decay = 0x0300000Au,
    layer1_amp_sustain = 0x0300000Bu,
    layer1_amp_release = 0x0300000Cu,

    layer2_volume = 0x04000001u,
    layer2_pan = 0x04000002u,
    layer2_osc_type = 0x04000003u,
    layer2_tune_coarse = 0x04000004u,
    layer2_tune_fine = 0x04000005u,
    layer2_filter_cutoff = 0x04000006u,
    layer2_filter_resonance = 0x04000007u,
    layer2_filter_type = 0x04000008u,
    layer2_amp_attack = 0x04000009u,
    layer2_amp_decay = 0x0400000Au,
    layer2_amp_sustain = 0x0400000Bu,
    layer2_amp_release = 0x0400000Cu,

    layer3_volume = 0x05000001u,
    layer3_pan = 0x05000002u,
    layer3_osc_type = 0x05000003u,
    layer3_tune_coarse = 0x05000004u,
    layer3_tune_fine = 0x05000005u,
    layer3_filter_cutoff = 0x05000006u,
    layer3_filter_resonance = 0x05000007u,
    layer3_filter_type = 0x05000008u,
    layer3_amp_attack = 0x05000009u,
    layer3_amp_decay = 0x0500000Au,
    layer3_amp_sustain = 0x0500000Bu,
    layer3_amp_release = 0x0500000Cu,
};

} // namespace wolfsden::pid
