#pragma once

#include "warp/helpers.h"
#include "warp/maybe.h"
#include "warp/font.h"

namespace warp {
    class controller_impl_i;
    class entity_t;
    class world_t;
    struct resources_t;
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

warp::maybe_t<warp::font_t *> get_default_font(const warp::resources_t &res);

warp::maybe_t<warp::entity_t *> create_label
        (warp::world_t *world, const warp::font_t &font, label_flags_t flags);


