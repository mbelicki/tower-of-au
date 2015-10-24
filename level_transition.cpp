#include "level_transition.h"

#include "warp/world.h"
#include "warp/cameras.h"
#include "warp/camera.h"

#include "level.h"
#include "region.h"
#include "core.h"
#include "input_controller.h"
#include "persitence.h"
#include "text-label.h"

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

    get_default_font(world->get_resources())
            .with_value([world](font_t *font) {
        create_label(world, *font, LABEL_POS_TOP | LABEL_POS_LEFT)
                .with_value([](entity_t *e){
            e->set_tag("hello_label");
            e->receive_message(CORE_SHOW_TAG_TEXT, tag_t("hello"));
        });
    });
    

    portal_t default_portal;
    default_portal.region_name = "test_00.json";
    default_portal.level_x = 0;
    default_portal.level_z = 0;
    default_portal.tile_x = 6;
    default_portal.tile_z = 5;

    const portal_t *portal = &default_portal;
    entity_t *region_data = world->find_entity("region_data");
    if (region_data != nullptr) {
        region_data->get_property("portal")
                .get_pointer().with_value([&portal](void *ptr) {
            portal = (const portal_t *) ptr;
        });
    }

    create_core(world, portal);
    create_input_controller(world);
}
