#ifndef ROAD_RASH_AUDIO_RUNTIME_H
#define ROAD_RASH_AUDIO_RUNTIME_H

#include "rsrc_reader.h"

typedef struct rr_audio_voice {
    const short *pcm;
    rr_u32 frames;
    rr_u32 source_rate;
} rr_audio_voice_t;

#define RR_AUDIO_EFFECT_CHANNELS 4u
typedef struct rr_audio_effect_channel {
    rr_u32 phase;
    rr_u32 voice_index;
    int active;
} rr_audio_effect_channel_t;

typedef struct rr_audio_mixer {
    rr_audio_voice_t voices[19];
    rr_u32 engine_phase;
    rr_audio_effect_channel_t effects[RR_AUDIO_EFFECT_CHANNELS];
    rr_u32 next_effect_channel;
    rr_u32 engine_pitch;
} rr_audio_mixer_t;

void rr_audio_init(rr_audio_mixer_t *mixer);
void rr_audio_set_voice(rr_audio_mixer_t *mixer, rr_u32 index,
                        const short *pcm, rr_u32 frames, rr_u32 source_rate);
void rr_audio_play(rr_audio_mixer_t *mixer, rr_u32 index);
/* Dispatch the race events currently produced by the 9588 simulation
 * using the original primary/secondary effect-channel arbitration. */
void rr_audio_play_race_event(rr_audio_mixer_t *mixer, rr_u32 event_type);
int rr_audio_effect_active(const rr_audio_mixer_t *mixer, rr_u32 index);
void rr_audio_stop_effect(rr_audio_mixer_t *mixer, rr_u32 index);
void rr_audio_render(rr_audio_mixer_t *mixer, short *output,
                     rr_u32 frames);

#endif
