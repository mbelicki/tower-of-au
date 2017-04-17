#pragma once

#include <functional>

#include "warp/math/vec2.h"
#include "warp/math/vec4.h"

namespace warp {
    class world_t;
    class entity_t;
    class controller_comp_t;
    class graphics_comp_t;
}

warp::controller_comp_t *create_button_controller
    ( warp::world_t *world, warp_vec2_t pos, warp_vec2_t size
    , std::function<void(void)> handler
    );

warp::graphics_comp_t *create_button_graphics
    (warp::world_t *world, warp_vec2_t size, const char *texture, warp_vec4_t color);

warp::entity_t *create_ui_background(warp::world_t *world, warp_vec4_t color);

warp::entity_t *create_button
    ( warp::world_t *world, warp_vec2_t pos, warp_vec2_t size
    , std::function<void(void)> handler, const char *texture
    );

warp::entity_t *create_text_button
    ( warp::world_t *world, warp_vec2_t pos, warp_vec2_t size
    , std::function<void(void)> handler, const char *text
    , warp_vec4_t background_color
    );
