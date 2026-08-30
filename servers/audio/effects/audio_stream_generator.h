// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef AUDIO_STREAM_GENERATOR_H
#define AUDIO_STREAM_GENERATOR_H

#include "core/ring_buffer.h"
#include "servers/audio/audio_stream.h"

class AudioStreamGenerator : public AudioStream {
    REBEL_OBJECT(AudioStreamGenerator, AudioStream);

    float mix_rate;
    float buffer_len;

protected:
    static void _bind_methods();

public:
    void set_mix_rate(float p_mix_rate);
    float get_mix_rate() const;

    void set_buffer_length(float p_seconds);
    float get_buffer_length() const;

    Ref<AudioStreamPlayback> instance_playback() override;
    String get_stream_name() const override;

    float get_length() const override;
    AudioStreamGenerator();
};

class AudioStreamGeneratorPlayback : public AudioStreamPlaybackResampled {
    REBEL_OBJECT(AudioStreamGeneratorPlayback, AudioStreamPlaybackResampled);
    friend class AudioStreamGenerator;
    RingBuffer<AudioFrame> buffer;
    int skips;
    bool active;
    float mixed;
    AudioStreamGenerator* generator;

protected:
    void _mix_internal(AudioFrame* p_buffer, int p_frames) override;
    float get_stream_sampling_rate() override;

    static void _bind_methods();

public:
    void start(float p_from_pos = 0.0) override;
    void stop() override;
    bool is_playing() const override;

    int get_loop_count() const override; // times it looped

    float get_playback_position() const override;
    void seek(float p_time) override;

    bool push_frame(const Vector2& p_frame);
    bool can_push_buffer(int p_frames) const;
    bool push_buffer(const PoolVector2Array& p_frames);
    int get_frames_available() const;
    int get_skips() const;

    void clear_buffer();

    AudioStreamGeneratorPlayback();
};
#endif // AUDIO_STREAM_GENERATOR_H
