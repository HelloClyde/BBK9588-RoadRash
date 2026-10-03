#include "road_audio_runtime.h"

void rr_audio_init(rr_audio_mixer_t *mixer)
{
    rr_u32 i;
    for (i = 0u; i < 19u; ++i) {
        mixer->voices[i].pcm = 0;
        mixer->voices[i].frames = 0u;
        mixer->voices[i].source_rate = 0u;
    }
    mixer->engine_phase = 0u;
    for (i = 0u; i < RR_AUDIO_EFFECT_CHANNELS; ++i) {
        mixer->effects[i].phase = 0u;
        mixer->effects[i].voice_index = 0u;
        mixer->effects[i].active = 0;
    }
    mixer->next_effect_channel = 0u;
    mixer->engine_pitch = 0u;
}

void rr_audio_set_voice(rr_audio_mixer_t *mixer, rr_u32 index,
                        const short *pcm, rr_u32 frames, rr_u32 source_rate)
{
    if (index >= 19u) return;
    mixer->voices[index].pcm = pcm;
    mixer->voices[index].frames = frames;
    mixer->voices[index].source_rate = source_rate;
}

void rr_audio_play(rr_audio_mixer_t *mixer, rr_u32 index)
{
    rr_u32 channel;
    rr_u32 i;
    if (index >= 19u || !mixer->voices[index].pcm) return;
    channel = mixer->next_effect_channel;
    for (i = 0u; i < RR_AUDIO_EFFECT_CHANNELS; ++i) {
        rr_u32 probe = (channel + i) % RR_AUDIO_EFFECT_CHANNELS;
        if (!mixer->effects[probe].active) {
            channel = probe;
            break;
        }
    }
    mixer->effects[channel].voice_index = index;
    mixer->effects[channel].phase = 0u;
    mixer->effects[channel].active = 1;
    mixer->next_effect_channel =
        (channel + 1u) % RR_AUDIO_EFFECT_CHANNELS;
}

static void rr_audio_play_on_channel(rr_audio_mixer_t *mixer,
                                      rr_u32 channel, rr_u32 sample)
{
    if (sample >= 19u || !mixer->voices[sample].pcm) return;
    mixer->effects[channel].voice_index = sample;
    mixer->effects[channel].phase = 0u;
    mixer->effects[channel].active = 1;
}

void rr_audio_play_race_event(rr_audio_mixer_t *mixer, rr_u32 event_type)
{
    rr_audio_effect_channel_t *primary = &mixer->effects[0];
    rr_audio_effect_channel_t *secondary = &mixer->effects[1];
    if (event_type >= 2u && event_type <= 4u) {
        /* 3DO checks for a free voice, then always writes the primary. */
        if (!primary->active || !secondary->active)
            rr_audio_play_on_channel(mixer, 0u, event_type);
    } else if (event_type >= 5u && event_type <= 7u) {
        if (!secondary->active)
            rr_audio_play_on_channel(mixer, 1u, event_type);
        else if (!primary->active)
            rr_audio_play_on_channel(mixer, 0u, event_type);
    } else if (event_type == 14u) {
        rr_audio_play_on_channel(mixer, 0u, 14u);
    } else if (event_type == 16u) {
        if (!primary->active || primary->voice_index != 16u)
            rr_audio_play_on_channel(mixer, 0u, 16u);
    } else if (event_type == 26u) {
        if (!secondary->active)
            rr_audio_play_on_channel(mixer, 1u, 2u);
    }
}

int rr_audio_effect_active(const rr_audio_mixer_t *mixer, rr_u32 index)
{
    rr_u32 channel;
    for (channel = 0u; channel < RR_AUDIO_EFFECT_CHANNELS; ++channel)
        if (mixer->effects[channel].active &&
            mixer->effects[channel].voice_index == index) return 1;
    return 0;
}

void rr_audio_stop_effect(rr_audio_mixer_t *mixer, rr_u32 index)
{
    rr_u32 channel;
    for (channel = 0u; channel < RR_AUDIO_EFFECT_CHANNELS; ++channel)
        if (mixer->effects[channel].voice_index == index)
            mixer->effects[channel].active = 0;
}

void rr_audio_render(rr_audio_mixer_t *mixer, short *output, rr_u32 frames)
{
    const rr_audio_voice_t *engine = &mixer->voices[0];
    rr_u32 engine_step = engine->source_rate *
        (mixer->engine_pitch / 22050u) +
        engine->source_rate * (mixer->engine_pitch % 22050u) / 22050u;
    rr_u32 i;
    for (i = 0u; i < frames; ++i) {
        int value = 0;
        rr_u32 channel;
        if (mixer->engine_pitch && engine->pcm && engine->frames) {
            rr_u32 index = mixer->engine_phase >> 16;
            if (index >= engine->frames) {
                mixer->engine_phase %= engine->frames << 16;
                index = mixer->engine_phase >> 16;
            }
            value += (int)engine->pcm[index] / 3;
            mixer->engine_phase += engine_step;
        }
        for (channel = 0u; channel < RR_AUDIO_EFFECT_CHANNELS; ++channel) {
            rr_audio_effect_channel_t *playing = &mixer->effects[channel];
            const rr_audio_voice_t *effect;
            rr_u32 index;
            if (!playing->active) continue;
            effect = &mixer->voices[playing->voice_index];
            index = playing->phase >> 16;
            if (index >= effect->frames) {
                playing->active = 0;
                continue;
            }
            value += (int)effect->pcm[index] / 4;
            playing->phase += effect->source_rate * 65536u / 22050u;
        }
        if (value > 32767) value = 32767;
        if (value < -32768) value = -32768;
        output[i] = (short)value;
    }
}
