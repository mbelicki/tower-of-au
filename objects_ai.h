#pragma once

#include "level_state.h"

bool needs_update(const object_t *obj);
bool pick_next_command
        (command_t *cmd, obj_id_t id, const level_state_t* state, warp_random *rand);
