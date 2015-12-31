#pragma once

#include "warp/maybe.h"

namespace warp {
    class entity_t;
    class world_t;
}

struct object_t;
struct portal_t;

warp::entity_t *get_persitent_data(warp::world_t *world);
warp::entity_t *create_persitent_data(warp::world_t *world);

const object_t *get_saved_player_state(warp::world_t *world);
const portal_t *get_saved_portal(warp::world_t *world);

void save_player_state(warp::world_t *world, const object_t *player);
void save_portal(warp::world_t *world, const portal_t *portal);

