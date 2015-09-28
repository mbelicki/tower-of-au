#pragma once

#include "warp/maybe.h"

namespace warp {
    class entity_t;
    class world_t;
}

warp::maybe_t<warp::entity_t *> create_input_controller(warp::world_t *world);
