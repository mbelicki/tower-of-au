#pragma once

#include <map>
#include "warp/vec3.h"
#include "warp/tag.h"
#include "warp/direction.h"
#include "warp/utils/random.h"

struct object_def_t;
struct object_t;

namespace warp {
    class world_t;
    struct resources_t;
}

class object_factory_t {
    public:
        object_factory_t();
        ~object_factory_t();
        bool load_definitions(const char *filename);
        void load_resources(const warp::resources_t *res);

        object_t *spawn
            ( warp::tag_t name, warp::vec3_t pos
            , warp::dir_t dir, warp_random_t *rand
            , warp::world_t *world
            );

    private:
        const object_def_t *get_definition(warp::tag_t name);

    private:
        std::map<warp::tag_t, object_def_t *> _objects;
};
