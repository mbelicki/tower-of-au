#include "level_transition.h"

#include "level.h"
#include "region.h"
#include "core.h"
#include "input_controller.h"

#include "warp/world.h"
#include "warp/cameras.h"
#include "warp/camera.h"

using namespace warp;

static maybeunit_t reset_camera(world_t *world) {
    cameras_mgr_t *cameras = world->get_resources().cameras;

    const maybe_t<const camera_t *> maybe_main_camera
        = cameras->get_camera_for_tag("main");
    const camera_t *main_camera = VALUE(maybe_main_camera);
    const float ratio = main_camera->get_aspect_ratio();

    const maybe_t<camera_id_t> camera_id = cameras->create_persp_camera
        ("main", vec3(6, 9.5f, 12.5f), vec3(0.95f, 0, 0), 0.39f, ratio, 4.0f, 18.0f);
    MAYBE_RETURN(camera_id, unit_t, "Failed to create camera:");

    return unit;
}

bool level_transition_t::is_entity_kept(const entity_t *) const {
    return false;
}

void level_transition_t::initialize_state
        (const tag_t &new_state, world_t *world) const {
    (void) new_state;
    
    maybeunit_t reset_result = reset_camera(world);
    if (reset_result.failed()) {
        printf("%s\n", reset_result.get_message().c_str());
    }

    region_t *region = generate_random_region(nullptr);
    region->initialize(world);

    create_core(world, region);
    create_input_controller(world);
}
