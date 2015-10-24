#pragma once

#include "warp/maybe.h"

namespace warp {
    class entity_t;
    class world_t;
}

struct portal_t;

warp::maybe_t<warp::entity_t *>
        create_region_token(warp::world_t *world, const portal_t *portal);
