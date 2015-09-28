#pragma once

#include "warp/maybe.h"
#include "warp/vec3.h"

namespace warp {
    class entity_t;
    class world_t;
}

warp::maybe_t<warp::entity_t *> create_avatar
    (warp::vec3_t initial_position, warp::world_t *world);
