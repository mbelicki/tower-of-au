#pragma once

#include "level_state.h"

bool needs_update(const object_t *obj);
void pick_next_command
        (command_t *cmd, const object_t *obj, const level_state_t* state, warp_random *rand);