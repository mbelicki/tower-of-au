#pragma once

#include "level_state.h"

namespace warp {
    class entity_t;
    class world_t;
    class controller_comp_t;
}

warp::controller_comp_t *create_character_controller
    (warp::world_t *world, obj_id_t id, bool confirm_move);
