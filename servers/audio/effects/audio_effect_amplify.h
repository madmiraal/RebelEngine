// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef AUDIO_EFFECT_AMPLIFY_H
#define AUDIO_EFFECT_AMPLIFY_H

#include "servers/audio/audio_effect.h"

class AudioEffectAmplify;

class AudioEffectAmplifyInstance : public AudioEffectInstance {
    REBEL_OBJECT(AudioEffectAmplifyInstance, AudioEffectInstance);
    friend class AudioEffectAmplify;
    Ref<AudioEffectAmplify> base;

    float mix_volume_db;

public:
    void process(
        const AudioFrame* p_src_frames,
        AudioFrame* p_dst_frames,
        int p_frame_count
    ) override;
};

class AudioEffectAmplify : public AudioEffect {
    REBEL_OBJECT(AudioEffectAmplify, AudioEffect);

    friend class AudioEffectAmplifyInstance;
    float volume_db;

protected:
    static void _bind_methods();

public:
    Ref<AudioEffectInstance> instance() override;
    void set_volume_db(float p_volume);
    float get_volume_db() const;

    AudioEffectAmplify();
};

#endif // AUDIO_EFFECT_AMPLIFY_H
