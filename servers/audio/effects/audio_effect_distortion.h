// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef AUDIO_EFFECT_DISTORTION_H
#define AUDIO_EFFECT_DISTORTION_H

#include "servers/audio/audio_effect.h"

class AudioEffectDistortion;

class AudioEffectDistortionInstance : public AudioEffectInstance {
    REBEL_OBJECT(AudioEffectDistortionInstance, AudioEffectInstance);
    friend class AudioEffectDistortion;
    Ref<AudioEffectDistortion> base;
    float h[2];

public:
    void process(
        const AudioFrame* p_src_frames,
        AudioFrame* p_dst_frames,
        int p_frame_count
    ) override;
};

class AudioEffectDistortion : public AudioEffect {
    REBEL_OBJECT(AudioEffectDistortion, AudioEffect);

public:
    enum Mode {
        MODE_CLIP,
        MODE_ATAN,
        MODE_LOFI,
        MODE_OVERDRIVE,
        MODE_WAVESHAPE,
    };

    friend class AudioEffectDistortionInstance;
    Mode mode;
    float pre_gain;
    float post_gain;
    float keep_hf_hz;
    float drive;

protected:
    static void _bind_methods();

public:
    Ref<AudioEffectInstance> instance() override;

    void set_mode(Mode p_mode);
    Mode get_mode() const;

    void set_pre_gain(float p_pre_gain);
    float get_pre_gain() const;

    void set_keep_hf_hz(float p_keep_hf_hz);
    float get_keep_hf_hz() const;

    void set_drive(float p_drive);
    float get_drive() const;

    void set_post_gain(float p_post_gain);
    float get_post_gain() const;

    AudioEffectDistortion();
};

VARIANT_ENUM_CAST(AudioEffectDistortion::Mode)

#endif // AUDIO_EFFECT_DISTORTION_H
