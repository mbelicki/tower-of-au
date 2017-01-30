#pragma once

#include "warp/utils/result.h"
#include "warp/math/vec3.h"
#include "warp/textures.h"
#include "warp/meshmanager.h"

class level_t;

namespace warp {
    class entity_t;
    class world_t;
}

enum bullet_type_t {
    BULLET_ARROW    = 0,
    BULLET_MAGIC    = 1,
    BULLET_FIREBALL = 2,
};

class bullet_factory_t {
    public:
        bullet_factory_t(warp::world_t *world)
            : _initialized(false)
            , _world(world)
        {}

        void initialize();

        warp::entity_t *create_bullet
            ( warp_vec3_t initial_position, warp_vec3_t velocity
            , bullet_type_t type, const level_t *level
            );

    private:
        bool _initialized;

        warp::mesh_id_t _mesh_id;       
        warp::mesh_id_t _arrow_mesh_id;       

        warp::world_t *_world;
};

