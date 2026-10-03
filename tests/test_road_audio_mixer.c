#include <assert.h>
#include <stdio.h>
#include "road_audio_runtime.h"

int main(void)
{
    rr_audio_mixer_t mixer;
    static const short a[2] = { 10000, 10000 };
    static const short b[2] = { 6000, 6000 };
    static const short engine[2] = { 12000, -12000 };
    short output[4];
    unsigned int i;
    rr_audio_init(&mixer);
    rr_audio_set_voice(&mixer, 1u, a, 2u, 22050u);
    rr_audio_set_voice(&mixer, 2u, b, 2u, 22050u);
    rr_audio_play(&mixer, 1u);
    rr_audio_play(&mixer, 2u);
    rr_audio_render(&mixer, output, 3u);
    assert(output[0] == 4000 && output[1] == 4000);
    assert(output[2] == 0);
    for (i = 0u; i < RR_AUDIO_EFFECT_CHANNELS; ++i)
        assert(!mixer.effects[i].active);
    rr_audio_play(&mixer, 1u);
    rr_audio_play(&mixer, 2u);
    assert(rr_audio_effect_active(&mixer, 1u));
    assert(rr_audio_effect_active(&mixer, 2u));
    rr_audio_stop_effect(&mixer, 1u);
    assert(!rr_audio_effect_active(&mixer, 1u));
    assert(rr_audio_effect_active(&mixer, 2u));
    rr_audio_render(&mixer, output, 1u);
    assert(output[0] == 1500);
    rr_audio_stop_effect(&mixer, 2u);
    rr_audio_play(&mixer, 19u);
    for (i = 0u; i < RR_AUDIO_EFFECT_CHANNELS; ++i)
        assert(!mixer.effects[i].active);
    rr_audio_set_voice(&mixer, 0u, engine, 2u, 22050u);
    mixer.engine_pitch = 32768u;
    rr_audio_render(&mixer, output, 4u);
    assert(output[0] == 4000 && output[1] == 4000);
    assert(output[2] == -4000 && output[3] == -4000);
    mixer.engine_pitch = 0u;
    rr_audio_render(&mixer, output, 4u);
    for (i = 0u; i < 4u; ++i) assert(output[i] == 0);
    rr_audio_init(&mixer);
    for (i = 2u; i <= 16u; ++i)
        rr_audio_set_voice(&mixer, i, a, 2u, 22050u);
    rr_audio_play_race_event(&mixer, 5u);
    assert(!mixer.effects[0].active && mixer.effects[1].active &&
           mixer.effects[1].voice_index == 5u);
    rr_audio_play_race_event(&mixer, 2u);
    assert(mixer.effects[0].active && mixer.effects[0].voice_index == 2u);
    rr_audio_play_race_event(&mixer, 3u);
    assert(mixer.effects[0].voice_index == 2u);
    mixer.effects[1].active = 0;
    rr_audio_play_race_event(&mixer, 4u);
    assert(mixer.effects[0].voice_index == 4u);
    rr_audio_play_race_event(&mixer, 6u);
    assert(mixer.effects[1].active && mixer.effects[1].voice_index == 6u);
    mixer.effects[0].active = 0;
    rr_audio_play_race_event(&mixer, 7u);
    assert(mixer.effects[0].active && mixer.effects[0].voice_index == 7u);
    rr_audio_play_race_event(&mixer, 14u);
    assert(mixer.effects[0].voice_index == 14u);
    rr_audio_play_race_event(&mixer, 16u);
    assert(mixer.effects[0].voice_index == 16u);
    mixer.effects[0].phase = 65536u;
    rr_audio_play_race_event(&mixer, 16u);
    assert(mixer.effects[0].phase == 65536u);
    rr_audio_play_race_event(&mixer, 26u);
    assert(mixer.effects[1].voice_index == 6u);
    mixer.effects[1].active = 0;
    rr_audio_play_race_event(&mixer, 26u);
    assert(mixer.effects[1].active && mixer.effects[1].voice_index == 2u);
    for (i = 2u; i < RR_AUDIO_EFFECT_CHANNELS; ++i)
        assert(!mixer.effects[i].active);
    puts("Four-channel mixing and original race-event voice arbitration: PASS");
    return 0;
}
