#include "bike_spec_runtime.h"

int rr_bike_spec_load(const rr_rsrc_file_t *catalog, unsigned int id,
                      rr_bike_spec_t *spec)
{
    rr_rsrc_record_t record;
    rr_u8 bytes[356];
    rr_u32 tuning, speed, steering;
    unsigned int i;
    if (!spec || id < 1u || id > 15u ||
        !rr_rsrc_find(catalog, RR_RSRC_TAG('S','P','E','C'), id, &record) ||
        record.size != 356u ||
        !rr_rsrc_read(catalog, &record, 0u, bytes, sizeof(bytes)))
        return 0;
    tuning = rr_rsrc_be32(bytes + 24u);
    speed = rr_rsrc_be32(bytes + 28u + 35u * 4u);
    steering = rr_rsrc_be32(bytes + 28u + 38u * 4u);
    if (tuning < 64u || tuning > 512u || speed < 20u || speed > 80u ||
        steering < 16u || steering > 512u)
        return 0;
    spec->tuning_base = tuning;
    spec->collision_profile_id = bytes[0];
    spec->minimum_acceleration = (int)rr_rsrc_be32(bytes + 4u);
    spec->contact_threshold = rr_rsrc_be32(bytes + 8u);
    spec->bounce_scale = rr_rsrc_be32(bytes + 12u);
    spec->impact_strength = rr_rsrc_be32(bytes + 16u);
    spec->impact_scale = rr_rsrc_be32(bytes + 20u);
    spec->forward_speed_factor = speed;
    spec->steering_speed_scale = steering;
    spec->forward_acceleration_divisor =
        rr_rsrc_be32(bytes + 28u + 36u * 4u);
    spec->forward_deceleration_divisor =
        rr_rsrc_be32(bytes + 28u + 37u * 4u);
    spec->braking_factor =
        (int)rr_rsrc_be32(bytes + 28u + 42u * 4u);
    spec->braking_rise_divisor =
        rr_rsrc_be32(bytes + 28u + 43u * 4u);
    spec->braking_fall_divisor =
        rr_rsrc_be32(bytes + 28u + 44u * 4u);
    spec->primary_steering_response =
        rr_rsrc_be32(bytes + 28u + 55u * 4u);
    spec->secondary_steering_response =
        rr_rsrc_be32(bytes + 28u + 56u * 4u);
    spec->steering_velocity_scale =
        rr_rsrc_be32(bytes + 28u + 59u * 4u);
    spec->slide_activation_threshold =
        rr_rsrc_be32(bytes + 28u + 51u * 4u);
    spec->slide_grip_scale_8_8 =
        rr_rsrc_be32(bytes + 28u + 52u * 4u);
    spec->slide_duration_ticks =
        rr_rsrc_be32(bytes + 28u + 53u * 4u);
    spec->surface_heading_scale_8_8 =
        rr_rsrc_be32(bytes + 28u + 61u * 4u);
    spec->alternate_surface_heading_scale_8_8 =
        rr_rsrc_be32(bytes + 28u + 62u * 4u);
    spec->steering_surface_scale =
        rr_rsrc_be32(bytes + 28u + 63u * 4u);
    spec->left_steering_rate_scale_8_8 =
        rr_rsrc_be32(bytes + 28u + 67u * 4u);
    spec->right_steering_rate_scale_8_8 =
        rr_rsrc_be32(bytes + 28u + 68u * 4u);
    spec->maximum_bike_health =
        rr_rsrc_be32(bytes + 28u + 70u * 4u);
    spec->base_engine_pitch =
        (int)rr_rsrc_be32(bytes + 28u + 26u * 4u);
    spec->idle_engine_pitch =
        (int)rr_rsrc_be32(bytes + 28u + 27u * 4u);
    if (!spec->forward_acceleration_divisor ||
        !spec->forward_deceleration_divisor ||
        !spec->braking_rise_divisor || !spec->braking_fall_divisor ||
        !spec->primary_steering_response ||
        !spec->secondary_steering_response ||
        !spec->steering_velocity_scale ||
        !spec->left_steering_rate_scale_8_8 ||
        !spec->right_steering_rate_scale_8_8 ||
        !spec->contact_threshold || !spec->bounce_scale ||
        !spec->impact_strength || !spec->impact_scale ||
        !spec->slide_grip_scale_8_8 ||
        spec->braking_factor >= 0 || spec->braking_factor < -1000 ||
        spec->base_engine_pitch < 0 ||
        spec->base_engine_pitch > 65535 ||
        spec->idle_engine_pitch < 0 ||
        spec->idle_engine_pitch > 65535 ||
        spec->maximum_bike_health < 1000u ||
        spec->maximum_bike_health > 100000u)
        return 0;
    for (i = 0u; i < 6u; ++i) {
        unsigned int at = 28u + (2u + i * 4u) * 4u;
        spec->gear[i].pitch_per_forward_velocity_8_8 =
            (int)rr_rsrc_be32(bytes + at);
        spec->gear[i].acceleration = rr_rsrc_be32(bytes + at + 4u);
        spec->gear[i].upshift = rr_rsrc_be32(bytes + at + 8u);
        spec->gear[i].downshift = (int)rr_rsrc_be32(bytes + at + 12u);
        if (spec->gear[i].pitch_per_forward_velocity_8_8 <= 0 ||
            spec->gear[i].pitch_per_forward_velocity_8_8 > 8192 ||
            !spec->gear[i].acceleration ||
            spec->gear[i].acceleration > 1024u)
            return 0;
    }
    return 1;
}
