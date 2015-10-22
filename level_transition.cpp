#include "level_transition.h"

#include "warp/world.h"
#include "warp/cameras.h"
#include "warp/camera.h"

#include "level.h"
#include "region.h"
#include "core.h"
#include "input_controller.h"
#include "persitence.h"

using namespace warp;

static maybeunit_t reset_camera(world_t *world) {
    cameras_mgr_t *cameras = world->get_resources().cameras;

    const maybe_t<const camera_t *> maybe_main_camera
        = cameras->get_camera_for_tag("main");
    const camera_t *main_camera = VALUE(maybe_main_camera);
    const float ratio = main_camera->get_aspect_ratio();

    const maybe_t<camera_id_t> camera_id = cameras->create_persp_camera
        ("main", vec3(6, 9.5f, 12.5f), vec3(0.95f, 0, 0), 0.39f, ratio, 7.0f, 19.0f);
    MAYBE_RETURN(camera_id, unit_t, "Failed to create camera:");

    return unit;
}

bool level_transition_t::is_entity_kept(const entity_t *entity) const {
    return entity->get_tag() == "region_data";
}

void level_transition_t::initialize_state
        (const tag_t &new_state, world_t *world) const {
    (void) new_state;
    
    maybeunit_t reset_result = reset_camera(world);
    if (reset_result.failed()) {
        printf("%s\n", reset_result.get_message().c_str());
    }

    const portal_t *portal = nullptr;
    entity_t *region_data = world->find_entity("region_data");
    if (region_data != nullptr) {
        region_data->get_property("portal")
                .get_pointer().with_value([&portal](void *ptr) {
            portal = (const portal_t *) ptr;
        });
    } 

    region_t *region;
    maybe_t<region_t *> maybe_region
        = load_region(portal == nullptr ? "test_00.json" : portal->region_name);
    if (maybe_region.failed()) {
        printf("%s\n", maybe_region.get_message().c_str());
        region = generate_random_region(nullptr);
    } else {
        region = VALUE(maybe_region);
    }
    region->initialize(world);

    create_core(world, region);
    create_input_controller(world);
}
