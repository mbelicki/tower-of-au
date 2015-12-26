#include "objects_ai.h"

#include "warp/direction.h"
#include "warp/mathutils.h"

#include "core.h"

using namespace warp;

extern bool needs_update(const object_t *obj) {
    return obj != nullptr
        && obj->type == OBJ_CHARACTER 
        && ((obj->flags & FOBJ_PLAYER_AVATAR) == 0);
}

static bool can_attack_other(const object_t &npc, const object_t &other) {
    const float dx = fabs(npc.position.x - other.position.x);
    const float dz = fabs(npc.position.z - other.position.z);
    if (dx > 1.1f || dz > 1.1f) return false;
    
    const bool close_x = epsilon_compare(dx, 1, 0.05f);
    const bool close_z = epsilon_compare(dz, 1, 0.05f);

    return close_x != close_z; /* xor */
}

static dir_t can_shoot_other
        (const object_t &npc, const object_t &other, const level_state_t *state) {
    if (npc.can_shoot == false || npc.ammo <= 0) return DIR_NONE;

    const float dx = npc.position.x - other.position.x;
    const float dz = npc.position.z - other.position.z;

    const bool zero_x = epsilon_compare(dx, 0, 0.05f);
    const bool zero_z = epsilon_compare(dz, 0, 0.05f);
    if ((zero_x || zero_z) == false) return DIR_NONE;

    const size_t x = round(npc.position.x);
    const size_t z = round(npc.position.z);
    std::function<bool(const tile_t *)> pred = [](const tile_t *t) {
        return t->is_walkable;
    };

    const level_t *level = state->get_current_level();
    if (zero_x) {
        dir_t result = dz > 0 ? DIR_Z_MINUS : DIR_Z_PLUS;
        const size_t dist = round(fabs(dz) - 1);
        if (level->scan_if_all(pred, x, z, result, dist))
            return result;
    } else if (zero_z) {
        dir_t result = dx > 0 ? DIR_X_MINUS : DIR_X_PLUS;
        const size_t dist = round(fabs(dx) - 1);
        if (level->scan_if_all(pred, x, z, result, dist))
            return result;
    }
    return DIR_NONE;
}

static void shuffle(dir_t *array, size_t n, warp_random_t *rand) {
    for (size_t i = 0; i < n - 1; i++) {
        //size_t j = i + rand() / (RAND_MAX / (n - i) + 1);
        const size_t j = i + warp_random_from_range(rand, 0, n - i);
        dir_t tmp = array[j];
        array[j] = array[i];
        array[i] = tmp;
    }
}

static dir_t pick_next_direction
        ( const object_t &npc, const object_t &other
        , const level_state_t *state, warp_random_t *rand
        ) {
    const vec3_t diff = vec3_sub(other.position, npc.position);
    const dir_t dir = vec3_to_dir(diff);
    const vec3_t position = vec3_add(npc.position, dir_to_vec3(dir));
    if (state->can_move_to(position)) {
        return dir;
    } else {
        dir_t directions[4] {
            DIR_X_PLUS, DIR_Z_PLUS, DIR_X_MINUS, DIR_Z_MINUS,
        };
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

extern void pick_next_command
        ( command_t *command, const object_t *obj
        , const level_state_t* state, warp_random_t *rand
        ) {
    if (command == nullptr) {
        warp_log_e("Cannot fill null command.");
        return;
    }
    if (obj == nullptr) {
        warp_log_e("Cannot pick command for null object.");
        return;
    }
    if (state == nullptr) {
        warp_log_e("Cannot pick command with null state.");
        return;
    }

    const object_t *player = state->find_player();
    if (player == nullptr) {
        warp_log_e("Couldn't find player.");
        return;
    }

    command->object = obj;
    
    dir_t shoot_dir = DIR_NONE;
    if (can_attack_other(*obj, *player)) {
        command->command = message_t(CORE_TRY_MOVE, (int)MOVE_LEFT);
    } else if ((shoot_dir = can_shoot_other(*obj, *player, state)) != DIR_NONE) {
        command->command = message_t(CORE_TRY_SHOOT, (int)dir_to_move(shoot_dir));
    } else {
        vec3_t position = vec3_add(obj->position, dir_to_vec3(obj->direction));
        dir_t dir = obj->direction;
        const bool change_dir = warp_random_float(rand) > 0.6f;
        if (change_dir || (state->can_move_to(position) == false)) {
            dir = pick_next_direction(*obj, *player, state, rand);
        }
        command->command = message_t(CORE_TRY_MOVE, (int)dir_to_move(dir));
    }
}
