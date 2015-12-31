#pragma once

#include "warp/vec2.h"

namespace warp {
    class world_t;
    class entity_t;
    class controller_comp_t;
    class graphics_comp_t;
}

warp::controller_comp_t *create_button_controller
    (warp::world_t *world, warp::vec2_t pos, warp::vec2_t size, int msg_type);
warp::graphics_comp_t *create_button_graphics
    (warp::world_t *world, warp::vec2_t size, const char *texture);

warp::entity_t *create_button
    ( warp::world_t *world, warp::vec2_t pos, warp::vec2_t size
    , int msg_type, const char *texture
    );

