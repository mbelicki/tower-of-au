#pragma once

#include "warp/vec2.h"
#include "warp/vec4.h"

namespace warp {
    class world_t;
    class entity_t;
    class controller_comp_t;
    class graphics_comp_t;
}

warp::controller_comp_t *create_button_controller
    (warp::world_t *world, warp::vec2_t pos, warp::vec2_t size, int msg_type);
warp::graphics_comp_t *create_button_graphics
    (warp::world_t *world, warp::vec2_t size, const char *texture, warp::vec4_t color);

warp::entity_t *create_ui_background(warp::world_t *world, warp::vec4_t color);

warp::entity_t *create_button
    ( warp::world_t *world, warp::vec2_t pos, warp::vec2_t size
    , int msg_type, const char *texture
    );

warp::entity_t *create_text_button
    ( warp::world_t *world, warp::vec2_t pos, warp::vec2_t size
    , int msg_type, const char *text
    );
