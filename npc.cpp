#include "npc.h"

#include "warp/vec3.h"
#include "warp/world.h"
#include "warp/entity-helpers.h"

using namespace warp;

extern maybe_t<entity_t *> create_npc
        (vec3_t initial_position, world_t *world) {

    maybe_t<graphics_comp_t *> graphics
        = create_single_model_graphics(world, "npc.obj", "missing.png");
    MAYBE_RETURN(graphics, entity_t *, "Failed to create NPC:");

    entity_t *entity = world->create_entity(initial_position, VALUE(graphics), nullptr, nullptr);
    entity->set_tag("npc");

    return entity;
}
