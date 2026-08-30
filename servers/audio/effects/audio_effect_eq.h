// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef AUDIO_EFFECT_EQ_H
#define AUDIO_EFFECT_EQ_H

#include "servers/audio/audio_effect.h"
#include "servers/audio/effects/eq.h"

class AudioEffectEQ;

class AudioEffectEQInstance : public AudioEffectInstance {
    REBEL_OBJECT(AudioEffectEQInstance, AudioEffectInstance);
    friend class AudioEffectEQ;
    Ref<AudioEffectEQ> base;

    Vector<EQ::BandProcess> bands[2];
    Vector<float> gains;

public:
    void process(
        const AudioFrame* p_src_frames,
        AudioFrame* p_dst_frames,
        int p_frame_count
    ) override;
};

class AudioEffectEQ : public AudioEffect {
    REBEL_OBJECT(AudioEffectEQ, AudioEffect);

    friend class AudioEffectEQInstance;

    EQ eq;
    Vector<float> gain;
    Map<StringName, int> prop_band_map;
    Vector<String> band_names;

protected:
    bool _set(const StringName& p_name, const Variant& p_value);
    bool _get(const StringName& p_name, Variant& r_ret) const;
    void _get_property_list(List<PropertyInfo>* p_list) const;

    static void _bind_methods();

public:
    Ref<AudioEffectInstance> instance() override;
    void set_band_gain_db(int p_band, float p_volume);
    float get_band_gain_db(int p_band) const;
    int get_band_count() const;

    AudioEffectEQ(EQ::Preset p_preset = EQ::PRESET_6_BANDS);
};

class AudioEffectEQ6 : public AudioEffectEQ {
    REBEL_OBJECT(AudioEffectEQ6, AudioEffectEQ);

public:
    AudioEffectEQ6() : AudioEffectEQ(EQ::PRESET_6_BANDS) {}
};

class AudioEffectEQ10 : public AudioEffectEQ {
    REBEL_OBJECT(AudioEffectEQ10, AudioEffectEQ);

public:
    AudioEffectEQ10() : AudioEffectEQ(EQ::PRESET_10_BANDS) {}
};

class AudioEffectEQ21 : public AudioEffectEQ {
    REBEL_OBJECT(AudioEffectEQ21, AudioEffectEQ);

public:
    AudioEffectEQ21() : AudioEffectEQ(EQ::PRESET_21_BANDS) {}
};

#endif // AUDIO_EFFECT_EQ_H
