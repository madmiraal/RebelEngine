// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef AUDIO_EFFECT_FILTER_H
#define AUDIO_EFFECT_FILTER_H

#include "servers/audio/audio_effect.h"
#include "servers/audio/audio_filter_sw.h"

class AudioEffectFilter;

class AudioEffectFilterInstance : public AudioEffectInstance {
    REBEL_OBJECT(AudioEffectFilterInstance, AudioEffectInstance);
    friend class AudioEffectFilter;

    Ref<AudioEffectFilter> base;

    AudioFilterSW filter;
    AudioFilterSW::Processor filter_process[2][4];

    template <int S>
    void _process_filter(
        const AudioFrame* p_src_frames,
        AudioFrame* p_dst_frames,
        int p_frame_count
    );

public:
    void process(
        const AudioFrame* p_src_frames,
        AudioFrame* p_dst_frames,
        int p_frame_count
    ) override;

    AudioEffectFilterInstance();
};

class AudioEffectFilter : public AudioEffect {
    REBEL_OBJECT(AudioEffectFilter, AudioEffect);

public:
    enum FilterDB {
        FILTER_6DB,
        FILTER_12DB,
        FILTER_18DB,
        FILTER_24DB,
    };
    friend class AudioEffectFilterInstance;

    AudioFilterSW::Mode mode;
    float cutoff;
    float resonance;
    float gain;
    FilterDB db;

protected:
    static void _bind_methods();

public:
    void set_cutoff(float p_freq);
    float get_cutoff() const;

    void set_resonance(float p_amount);
    float get_resonance() const;

    void set_gain(float p_amount);
    float get_gain() const;

    void set_db(FilterDB p_db);
    FilterDB get_db() const;

    Ref<AudioEffectInstance> instance() override;

    AudioEffectFilter(AudioFilterSW::Mode p_mode = AudioFilterSW::LOWPASS);
};

VARIANT_ENUM_CAST(AudioEffectFilter::FilterDB)

class AudioEffectLowPassFilter : public AudioEffectFilter {
    REBEL_OBJECT(AudioEffectLowPassFilter, AudioEffectFilter);

    void _validate_property(PropertyInfo& property) const override {
        if (property.name == "gain") {
            property.usage = 0;
        }
    }

public:
    AudioEffectLowPassFilter() : AudioEffectFilter(AudioFilterSW::LOWPASS) {}
};

class AudioEffectHighPassFilter : public AudioEffectFilter {
    REBEL_OBJECT(AudioEffectHighPassFilter, AudioEffectFilter);

    void _validate_property(PropertyInfo& property) const override {
        if (property.name == "gain") {
            property.usage = 0;
        }
    }

public:
    AudioEffectHighPassFilter() : AudioEffectFilter(AudioFilterSW::HIGHPASS) {}
};

class AudioEffectBandPassFilter : public AudioEffectFilter {
    REBEL_OBJECT(AudioEffectBandPassFilter, AudioEffectFilter);

    void _validate_property(PropertyInfo& property) const override {
        if (property.name == "gain") {
            property.usage = 0;
        }
    }

public:
    AudioEffectBandPassFilter() : AudioEffectFilter(AudioFilterSW::BANDPASS) {}
};

class AudioEffectNotchFilter : public AudioEffectFilter {
    REBEL_OBJECT(AudioEffectNotchFilter, AudioEffectFilter);

public:
    AudioEffectNotchFilter() : AudioEffectFilter(AudioFilterSW::NOTCH) {}
};

class AudioEffectBandLimitFilter : public AudioEffectFilter {
    REBEL_OBJECT(AudioEffectBandLimitFilter, AudioEffectFilter);

public:
    AudioEffectBandLimitFilter() :
        AudioEffectFilter(AudioFilterSW::BANDLIMIT) {}
};

class AudioEffectLowShelfFilter : public AudioEffectFilter {
    REBEL_OBJECT(AudioEffectLowShelfFilter, AudioEffectFilter);

public:
    AudioEffectLowShelfFilter() : AudioEffectFilter(AudioFilterSW::LOWSHELF) {}
};

class AudioEffectHighShelfFilter : public AudioEffectFilter {
    REBEL_OBJECT(AudioEffectHighShelfFilter, AudioEffectFilter);

public:
    AudioEffectHighShelfFilter() :
        AudioEffectFilter(AudioFilterSW::HIGHSHELF) {}
};

#endif // AUDIO_EFFECT_FILTER_H
