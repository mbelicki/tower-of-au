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
};

enum move_dir_t : int {
    MOVE_UP,
    MOVE_DOWN,
    MOVE_LEFT,
    MOVE_RIGHT,
};

enum attack_element_t : unsigned char {
    ATTACK_ELEM_FIRE,
    ATTACK_ELEM_WATER,
    ATTACK_ELEM_AIR,
    ATTACK_ELEM_STEEL,
};

static inline bool operator==
        (const warp::messagetype_t &a, const core_msgs_t &b) {
    return static_cast<int>(a) == static_cast<int>(b);
}

warp::maybe_t<warp::entity_t *> create_core
        (warp::world_t *world, level_t *level);
