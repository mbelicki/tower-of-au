#pragma once

#include "warp/maybe.h"
#include "warp/messagequeue.h"

namespace warp {
    class entity_t;
    class world_t;
}

class level_t;

enum core_msgs_t : unsigned int {
    CORE_TRY_MOVE = warp::MSG_CUSTOM,

    CORE_DO_MOVE,
    CORE_DO_ATTACK,
    CORE_DO_BOUNCE,
    CORE_DO_HURT,
    CORE_DO_DIE,

    CORE_MOVE_DONE,
};

enum move_dir_t : int {
    MOVE_UP,
    MOVE_DOWN,
    MOVE_LEFT,
    MOVE_RIGHT,
};

static inline bool operator==
        (const warp::messagetype_t &a, const core_msgs_t &b) {
    return static_cast<int>(a) == static_cast<int>(b);
}

warp::maybe_t<warp::entity_t *> create_core
        (warp::world_t *world, level_t *level);
