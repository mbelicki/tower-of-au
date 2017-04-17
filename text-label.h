#pragma once

#include "warp/helpers.h"
#include "warp/graphics/font.h"

namespace warp {
    class controller_impl_i;
    class entity_t;
    class world_t;
}

enum label_flags_t {
    LABEL_NONE       = 0,
    LABEL_LARGE      = 1,

    LABEL_POS_TOP    = 2,
    LABEL_POS_BOTTOM = 4,

    LABEL_POS_LEFT   = 8,
    LABEL_POS_RIGHT  = 16,

    LABEL_PASS_MAIN  = 32,
};

WARP_ENABLE_FLAGS(label_flags_t);

warp_res_id_t get_default_font(warp_resources_t *res);
warp_res_id_t get_dialog_font(warp_resources_t *res);

warp::entity_t * create_label
    (warp::world_t *world, warp_res_id_t font, label_flags_t flags);

warp::entity_t *create_speech_bubble
    ( warp::world_t *world, warp_res_id_t font
    , warp_vec3_t pos, const char *text
    );
