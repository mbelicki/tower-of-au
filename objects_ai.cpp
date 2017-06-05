#define WARP_DROP_PREFIX
#include "objects_ai.h"

#include "warp/utils/log.h"
#include "warp/math/utils.h"
#include "warp/utils/directions.h"

#include "core.h"

using namespace warp;

extern void init_ai_state(ai_state_t *state, const object_t *obj) {
    state->player_seen = false;
    state->start_dir = obj->direction;
    state->start_pos = obj->position;
    state->last_sighting = state->start_pos;
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
    if (shooter == NULL || target == NULL) return false;
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
        ( const object_t *npc, const object_t *other
        , const level_state_t *state, warp_random_t *rand
        ) {
    const vec3_t diff = vec3_sub(other->position, npc->position);
    const dir_t dir = vec3_to_dir(diff);
    const vec3_t position = vec3_add(npc->position, dir_to_vec3(dir));
    dir_t directions[4] {
        DIR_X_PLUS, DIR_Z_PLUS, DIR_X_MINUS, DIR_Z_MINUS,
    };
    if (state->can_move_to(position)) {
        return dir;
    } else {
        shuffle(directions, 4, rand);

        const vec3_t position = npc->position;
        for (size_t i = 0; i < 4; i++) {
            const dir_t dir = directions[i];
            const vec3_t target = vec3_add(position, dir_to_vec3(dir));
            if (state->can_move_to(target))
                return dir;
        }
    }

    return npc->direction;
}

static dir_t pick_direction(const object_t *npc, vec3_t pos) {
    if (vec3_eps_equals(npc->position, pos, 0.01f)) {
        return DIR_NONE;
    }
    return vec3_to_dir(vec3_sub(pos, npc->position));
}

static move_dir_t dir_to_move(dir_t d) {
    switch (d) {
        case DIR_Z_MINUS: return MOVE_UP;
        case DIR_X_MINUS: return MOVE_LEFT;
        case DIR_Z_PLUS:  return MOVE_DOWN;
        case DIR_X_PLUS:  return MOVE_RIGHT;
        default:          return MOVE_NONE;
    }
}

static bool can_ai_move_to(vec3_t position, const level_state_t *state) {
    const int x = round(position.x);
    const int z = round(position.z);
    if (x < 0 || x >= 13) return false;
    if (z < 0 || z >= 11) return false;
    return state->can_move_to(position);
}

static void update_ai
        (ai_state_t *ai_state, const object_t *obj) {
    if (vec3_eps_equals(obj->position, ai_state->last_sighting, 0.01f)) {
        ai_state->player_seen = false;
    }
}

static bool can_see_through(vec3_t pos, const level_t *level) {
    const int x = round(pos.x);
    const int z = round(pos.z);
    const tile_t *tile = level->get_tile_at(x, z);
    /* TODO: this test is tile is walkable, there should be a separate
     * see-through flag for tiles */
    if (tile == NULL || tile->is_walkable == false) {
        return false;
    }
    return true;
}

static void look_ahead
        (ai_state_t *ai_state, const object_t *obj, const level_state_t* st) {
    const level_t *level = st->get_current_level();
    const int sight_range = level->get_width();
    const vec3_t d = dir_to_vec3(obj->direction);
    for (int i = 1; i < sight_range; i++) {
        const vec3_t p = vec3_add(obj->position, vec3_scale(d, i));
        if (can_see_through(p, level) == false) {
            return;
        }
        const obj_id_t id = st->object_at_position(p);
        if (st->has_object_flag(id, FOBJ_PLAYER_AVATAR) == false) {
            /* not a player, blocks vision, no need to scan further */
            continue;
        }
        /* a player otherwise */
        ai_state->player_seen = true;
        ai_state->last_sighting = p;
        return;
    }
}

static dir_t pick_move_direction
        ( ai_state_t *ai_state, const object_t *obj
        , const level_state_t* state, warp_random_t *rand
        ) {
    if (obj->flags & FOBJ_NPCMOVE_STILL) {
        return DIR_NONE;
    }

    dir_t dir = obj->direction;
    const vec3_t next_pos = vec3_add(obj->position, dir_to_vec3(dir));
    const bool obstacle_ahead = can_ai_move_to(next_pos, state) == false;

    if (obj->flags & FOBJ_NPCMOVE_ROAM) {
        const bool change_dir = warp_random_float(rand) > 0.6f;
        if (change_dir || obstacle_ahead) {
            obj_id_t player_id = state->find_player();
            const object_t *player = state->get_object(player_id);
            dir = pick_roam_direction(obj, player, state, rand);
        }
    } else if (obj->flags & FOBJ_NPCMOVE_SENTERY) {
        if (ai_state->player_seen) {
            dir = pick_direction(obj, ai_state->last_sighting);  
        } else {
            dir = pick_direction(obj, ai_state->start_pos);
        }
    } else if (obj->flags & FOBJ_NPCMOVE_LINE) {
        if (obstacle_ahead) {
            dir = opposite_dir(dir);
        }
    }

    return dir;
}

static dir_t pick_rotate_direction(ai_state_t *ai_state, const object_t *obj) {
    if (obj->flags & FOBJ_NPCMOVE_SENTERY
            && vec3_eps_equals(obj->position, ai_state->start_pos, 0.01f)) {
        return ai_state->start_dir;
    }
    return DIR_NONE;
}

static void fill_command
        ( command_t *command
        , const obj_id_t id, command_type_t type, dir_t dir) {
    command->object_id = id;
    command->type = type;
    command->direction = dir_to_move(dir);
}

extern bool pick_next_command
        ( command_t *command, obj_id_t id, ai_state_t *ai_state
        , const level_state_t* st, warp_random_t *rand
        ) {
    if (command == NULL) {
        warp_log_e("Cannot fill null command.");
        return false;
    }
    if (st == NULL) {
        warp_log_e("Cannot pick command with null state.");
        return false;
    }
    if (id == OBJ_ID_INVALID) {
        warp_log_e("Cannot pick command for invalid object.");
        return false;
    }
    if (st->is_object_valid(id) == false) {
        return false;
    }

    obj_id_t player_id = st->find_player();
    if (player_id == OBJ_ID_INVALID) {
        warp_log_e("Couldn't find player.");
        return false;
    }
    const object_t *player = st->get_object(player_id);
    const object_t *obj = st->get_object(id);

    /* try to perform one of the attacks */
    if (can_attack(obj, player)) {
        const vec3_t diff = vec3_sub(player->position, obj->position);
        fill_command(command, id, CMD_MOVE, vec3_to_dir(diff));
        return true;
    } else if (can_shoot(obj, player)) {
        const dir_t shoot_dir = pick_shooting_direction(obj, player, st);
        if (shoot_dir != DIR_NONE) {
            fill_command(command, id, CMD_SHOOT, shoot_dir);
            return true;
        }
    }
    update_ai(ai_state, obj);
    look_ahead(ai_state, obj, st);
    /* if none of the attacks succeeded try to move: */
    const dir_t move_dir = pick_move_direction(ai_state, obj, st, rand);
    if (move_dir != DIR_NONE) {
        fill_command(command, id, CMD_MOVE, move_dir);
        return true;
    }
    const dir_t rot_dir = pick_rotate_direction(ai_state, obj);
    if (rot_dir != DIR_NONE) {
        fill_command(command, id, CMD_ROTATE, rot_dir);
        return true;
    }

    return false;
}
