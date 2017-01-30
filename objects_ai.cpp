#define WARP_DROP_PREFIX
#include "objects_ai.h"

#include "warp/direction.h"
#include "warp/utils/log.h"
#include "warp/math/utils.h"

#include "core.h"

using namespace warp;

extern bool needs_update(const object_t *obj) {
    return obj != nullptr
        && obj->type == OBJ_CHARACTER 
        && ((obj->flags & FOBJ_PLAYER_AVATAR) == 0);
}

static bool can_attack(const object_t *attacker, const object_t *target) {
    if (attacker->flags & FOBJ_CAN_ROTATE) {
        const float dx = fabs(attacker->position.x - target->position.x);
        const float dz = fabs(attacker->position.z - target->position.z);
        if (dx > 1.1f || dz > 1.1f) return false;

        const bool close_x = epsilon_compare(dx, 1, 0.05f);
        const bool close_z = epsilon_compare(dz, 1, 0.05f);

        return close_x != close_z; /* xor */
    } else {
        const vec3_t in_front
            = vec3_add(attacker->position, dir_to_vec3(attacker->direction));
        return vec3_eps_equals(in_front, target->position, 0.1f);
    }
}

static bool can_shoot(const object_t *shooter, const object_t *target) {
    if (shooter == nullptr || target == nullptr) return false;
    if ((shooter->flags & FOBJ_CAN_SHOOT) == 0 || shooter->ammo <= 0) return false;
    return true;
}

static dir_t pick_shooting_direction
        (const object_t *shooter, const object_t *target, const level_state_t *state) {
    if (can_shoot(shooter, target) == false) return DIR_NONE;

    const float dx = shooter->position.x - target->position.x;
    const float dz = shooter->position.z - target->position.z;

    const bool zero_x = epsilon_compare(dx, 0, 0.05f);
    const bool zero_z = epsilon_compare(dz, 0, 0.05f);
    if ((zero_x || zero_z) == false) return DIR_NONE;

    const size_t x = round(shooter->position.x);
    const size_t z = round(shooter->position.z);
    std::function<bool(const tile_t *)> pred = [](const tile_t *t) {
        return t->is_walkable;
    };

    const level_t *level = state->get_current_level();
    dir_t result = DIR_NONE;
    if (zero_x) {
        dir_t maybe_result = dz > 0 ? DIR_Z_MINUS : DIR_Z_PLUS;
        const size_t dist = round(fabs(dz));
        if (level->scan_if_all(pred, x, z, result, dist)) {
            result = maybe_result;
        }
    } else if (zero_z) {
        dir_t maybe_result = dx > 0 ? DIR_X_MINUS : DIR_X_PLUS;
        const size_t dist = round(fabs(dx));
        if (level->scan_if_all(pred, x, z, result, dist)) {
            result = maybe_result;
        }
    }

    if ((shooter->flags & FOBJ_CAN_ROTATE) == 0) {
        if (result != shooter->direction) {
            result = DIR_NONE;
        }
    }

    return result;
}

static void shuffle(dir_t *array, size_t n, warp_random_t *rand) {
    for (size_t i = 0; i < n - 1; i++) {
        const size_t j = i + warp_random_from_range(rand, 0, n - i - 1);
        dir_t tmp = array[j];
        array[j] = array[i];
        array[i] = tmp;
    }
}

static dir_t pick_roam_direction
        ( const object_t &npc, const object_t &other
        , const level_state_t *state, warp_random_t *rand
        ) {
    const vec3_t diff = vec3_sub(other.position, npc.position);
    const dir_t dir = vec3_to_dir(diff);
    const vec3_t position = vec3_add(npc.position, dir_to_vec3(dir));
    dir_t directions[4] {
        DIR_X_PLUS, DIR_Z_PLUS, DIR_X_MINUS, DIR_Z_MINUS,
    };
    if (state->can_move_to(position)) {
        return dir;
    } else {
        shuffle(directions, 4, rand);

        const vec3_t position = npc.position;
        for (size_t i = 0; i < 4; i++) {
            const dir_t dir = directions[i];
            const vec3_t target = vec3_add(position, dir_to_vec3(dir));
            if (state->can_move_to(target))
                return dir;
        }
    }

    return npc.direction;
}

static move_dir_t dir_to_move(dir_t d) {
    switch (d) {
        case DIR_Z_MINUS:
            return MOVE_UP;
        case DIR_X_MINUS:
            return MOVE_LEFT;
        case DIR_Z_PLUS:
            return MOVE_DOWN;
        case DIR_X_PLUS:
            return MOVE_RIGHT;
        default:
            return MOVE_NONE;
    }
}

static bool can_ai_move_to(vec3_t position, const level_state_t *state) {
    const int x = round(position.x);
    const int z = round(position.z);
    if (x < 0 || x >= 13) return false;
    if (z < 0 || z >= 11) return false;
    return state->can_move_to(position);
}

static dir_t pick_move_direction
        (const object_t *obj, const level_state_t* state, warp_random_t *rand) {
    if (obj->flags & FOBJ_NPCMOVE_STILL) {
        return DIR_NONE;
    }

    dir_t dir = obj->direction;
    const vec3_t next_pos = vec3_add(obj->position, dir_to_vec3(dir));
    const bool obstacle_ahead = can_ai_move_to(next_pos, state) == false;

    if (obj->flags & FOBJ_NPCMOVE_ROAM) {
        const bool change_dir = warp_random_float(rand) > 0.6f;
        if (change_dir || obstacle_ahead) {
            const object_t *player = state->find_player();
            dir = pick_roam_direction(*obj, *player, state, rand);
        }
    } else if (obj->flags & FOBJ_NPCMOVE_LINE) {
        if (obstacle_ahead) {
            dir = opposite_dir(dir);
        }
    }

    return dir;
}

static void fill_command
        (command_t *command, const object_t *obj, int type, dir_t dir) {
    command->object = obj;
    command->command = message_t(type, (int)dir_to_move(dir));
}

extern bool pick_next_command
        ( command_t *command, const object_t *obj
        , const level_state_t* state, warp_random_t *rand
        ) {
    if (command == nullptr) {
        warp_log_e("Cannot fill null command.");
        return false;
    }
    if (obj == nullptr) {
        warp_log_e("Cannot pick command for null object.");
        return false;
    }
    if (state == nullptr) {
        warp_log_e("Cannot pick command with null state.");
        return false;
    }
    if (needs_update(obj) == false) {
        return false;
    }

    const object_t *player = state->find_player();
    if (player == nullptr) {
        warp_log_e("Couldn't find player.");
        return false;
    }

    /* try to perform one of the attacks */
    if (can_attack(obj, player)) {
        const vec3_t diff = vec3_sub(player->position, obj->position);
        fill_command(command, obj, CORE_TRY_MOVE, vec3_to_dir(diff));
        return true;
    } else if (can_shoot(obj, player)) {
        const dir_t shoot_dir = pick_shooting_direction(obj, player, state);
        if (shoot_dir != DIR_NONE) {
            fill_command(command, obj, CORE_TRY_SHOOT, shoot_dir);
            return true;
        }
    } 

    /* if none of the attacks succeeded try to move: */
    const dir_t dir = pick_move_direction(obj, state, rand);
    if (dir != DIR_NONE) {
        fill_command(command, obj, CORE_TRY_MOVE, dir);
        return true;
    }

    return false;
}
