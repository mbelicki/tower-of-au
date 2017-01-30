#pragma once

namespace warp {
    class entity_t;
    class world_t;
}

warp::entity_t *create_fade_circle
        (warp::world_t *world, float out_radius, float duration, bool closing);
