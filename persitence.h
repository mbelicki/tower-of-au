#pragma once

#include <stdint.h>

namespace warp {
    class entity_t;
    class world_t;
}

struct object_t;
struct portal_t;

typedef struct warp_map warp_map_t;

warp::entity_t *get_persitent_data(warp::world_t *world);
warp::entity_t *create_persitent_data(warp::world_t *world);

const object_t *get_saved_player_state(warp::world_t *world);
const portal_t *get_saved_portal(warp::world_t *world);
uint32_t get_saved_seed(warp::world_t *world);
const warp_map_t *get_saved_facts(warp::world_t *world);

void save_player_state(warp::world_t *world, const object_t *player);
void save_portal(warp::world_t *world, const portal_t *portal);
void save_random_seed(warp::world_t *world, uint32_t seed);
void save_facts(warp::world_t *world, const warp_map_t *facts);

