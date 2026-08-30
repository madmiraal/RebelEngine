// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef AUDIO_EFFECT_REVERB_H
#define AUDIO_EFFECT_REVERB_H

#include "servers/audio/audio_effect.h"
#include "servers/audio/effects/reverb.h"

class AudioEffectReverb;

class AudioEffectReverbInstance : public AudioEffectInstance {
    REBEL_OBJECT(AudioEffectReverbInstance, AudioEffectInstance);

    Ref<AudioEffectReverb> base;

    float tmp_src[Reverb::INPUT_BUFFER_MAX_SIZE];
    float tmp_dst[Reverb::INPUT_BUFFER_MAX_SIZE];

    friend class AudioEffectReverb;

    Reverb reverb[2];

public:
    void process(
        const AudioFrame* p_src_frames,
        AudioFrame* p_dst_frames,
        int p_frame_count
    ) override;
    AudioEffectReverbInstance();
};

class AudioEffectReverb : public AudioEffect {
    REBEL_OBJECT(AudioEffectReverb, AudioEffect);

    friend class AudioEffectReverbInstance;

    float predelay;
    float predelay_fb;
    float hpf;
    float room_size;
    float damping;
    float spread;
    float dry;
    float wet;

protected:
    static void _bind_methods();

public:
    void set_predelay_msec(float p_msec);
    void set_predelay_feedback(float p_feedback);
    void set_room_size(float p_size);
    void set_damping(float p_damping);
    void set_spread(float p_spread);
    void set_dry(float p_dry);
    void set_wet(float p_wet);
    void set_hpf(float p_hpf);

    float get_predelay_msec() const;
    float get_predelay_feedback() const;
    float get_room_size() const;
    float get_damping() const;
    float get_spread() const;
    float get_dry() const;
    float get_wet() const;
    float get_hpf() const;

    Ref<AudioEffectInstance> instance() override;

    AudioEffectReverb();
};

#endif // AUDIO_EFFECT_REVERB_H
