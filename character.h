#pragma once

#include "warp/maybe.h"
#include "warp/vec3.h"

namespace warp {
    class entity_t;
    class world_t;
    class controller_comp_t;
}

warp::maybe_t<warp::controller_comp_t *> 
    create_character_controller(warp::world_t *world, bool confirm_move);
