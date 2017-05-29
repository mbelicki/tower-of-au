#pragma once

#include "warp/utils/tag.h"
#include "warp/math/vec3.h"
#include "warp/utils/random.h"
#include "warp/utils/directions.h"
#include "warp/collections/map.h"
#include "warp/resources/resources.h"

#include "level_state.h"

struct object_def_t;
struct object_t;

namespace warp {
    class world_t;
    class entity_t;
}

class object_factory_t {
    public:
        object_factory_t();
        bool load_definitions(const char *filename);
        void load_resources(warp_resources_t *res);

        warp::entity_t *create_object_entity
            ( const object_t *obj, obj_id_t id
            , const warp_tag_t &def_name, warp::world_t *world
            );

        bool spawn
            ( object_t *buffer, warp_tag_t name
            , warp_vec3_t pos, warp_dir_t dir
            , warp_random_t *rand
            );

    private:
        const object_def_t *get_definition(warp_tag_t name);

    private:
        warp_map_t _objects;
};
