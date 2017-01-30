#pragma once

namespace warp {
    class entity_t;
    class world_t;
    class controller_comp_t;
}

warp::controller_comp_t *create_door_controller(warp::world_t *world);
