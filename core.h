#pragma once

#include "warp/maybe.h"
#include "warp/messagequeue.h"

namespace warp {
    class entity_t;
    class world_t;
}

struct portal_t;

enum core_msgs_t : unsigned int {
    CORE_TRY_MOVE = warp::MSG_CUSTOM,
    CORE_TRY_SHOOT,

    CORE_DO_MOVE,
    CORE_DO_MOVE_IMMEDIATE,
    CORE_DO_ATTACK,
    CORE_DO_BOUNCE,
    CORE_DO_HURT,
    CORE_DO_DIE,

    CORE_MOVE_DONE,

    CORE_FEAT_STATE_CHANGE,
    CORE_BULLET_HIT,
};

enum move_dir_t : int {
    MOVE_NONE,
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
        (warp::world_t *world, const portal_t *start_point);
