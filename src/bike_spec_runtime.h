#ifndef RR_BIKE_SPEC_RUNTIME_H
#define RR_BIKE_SPEC_RUNTIME_H
#include "rsrc_reader.h"

/* Selected big-endian fields from the original 356-byte SPEC record. */
typedef struct rr_bike_spec {
    unsigned int collision_profile_id;
    int minimum_acceleration;
    unsigned int contact_threshold;
    unsigned int bounce_scale;
    unsigned int impact_strength;
    unsigned int impact_scale;
    unsigned int tuning_base;
    unsigned int forward_speed_factor;
    unsigned int steering_speed_scale;
    unsigned int forward_acceleration_divisor;
    unsigned int forward_deceleration_divisor;
    int braking_factor;
    unsigned int braking_rise_divisor;
    unsigned int braking_fall_divisor;
    unsigned int primary_steering_response;
    unsigned int secondary_steering_response;
    unsigned int steering_velocity_scale;
    unsigned int slide_activation_threshold;
    unsigned int slide_grip_scale_8_8;
    unsigned int slide_duration_ticks;
    unsigned int surface_heading_scale_8_8;
    unsigned int alternate_surface_heading_scale_8_8;
    unsigned int steering_surface_scale;
    unsigned int left_steering_rate_scale_8_8;
    unsigned int right_steering_rate_scale_8_8;
    unsigned int maximum_bike_health;
    int base_engine_pitch;
    int idle_engine_pitch;
    struct {
        int pitch_per_forward_velocity_8_8;
        unsigned int acceleration;
        unsigned int upshift;
        int downshift;
    } gear[6];
} rr_bike_spec_t;

int rr_bike_spec_load(const rr_rsrc_file_t *catalog, unsigned int id,
                      rr_bike_spec_t *spec);
#endif
