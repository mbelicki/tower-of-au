#pragma once

#include "level_state.h"

struct ai_state_t {
    bool player_seen;
    warp_dir_t start_dir;
    warp_vec3_t start_pos;
    warp_vec3_t last_sighting;
};

void init_ai_state(ai_state_t *state, const object_t *obj);
bool pick_next_command
        ( command_t *cmd, obj_id_t id, ai_state_t *ai_state
        , const level_state_t* state, warp_random *rand
        );
