#pragma once

#include "warp/utils/tag.h"
#include "warp/math/vec3.h"
#include "warp/utils/random.h"
#include "warp/utils/directions.h"
#include "warp/collections/map.h"
#include "warp/resources/resources.h"

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
            (const object_t *obj, const warp_tag_t &def_name, warp::world_t *world);

        object_t *spawn
            ( warp_tag_t name, warp_vec3_t pos
            , warp_dir_t dir, warp_random_t *rand
            , warp::world_t *world
            );

    private:
        const object_def_t *get_definition(warp_tag_t name);

    private:
        warp_map_t _objects;
};
