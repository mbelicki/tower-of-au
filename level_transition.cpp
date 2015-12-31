#include "level_transition.h"

#include "warp/world.h"
#include "warp/cameras.h"
#include "warp/camera.h"
#include "warp/renderer.h"

#include "level.h"
#include "region.h"
#include "core.h"
#include "input_controller.h"
#include "persitence.h"
#include "text-label.h"
#include "transition_effect.h"
#include "button.h"

using namespace warp;

static maybeunit_t reset_camera(world_t *world) {
    cameras_mgr_t *cameras = world->get_resources().cameras;

    const maybe_t<const camera_t *> maybe_main_camera
        = cameras->get_camera_for_tag("main");
    const camera_t *main_camera = VALUE(maybe_main_camera);
    const float ratio = main_camera->get_aspect_ratio();

    const maybe_t<camera_id_t> camera_id = cameras->create_persp_camera
        ("main", vec3(6, 10, 10.4f), vec3(1.12f, 0, 0), 0.44f, ratio, 7.0f, 19.0f);
    MAYBE_RETURN(camera_id, unit_t, "Failed to create camera:");

    return unit;
}

void level_transition_t::configure_renderer(renderer_t *render) const {
    light_settings_t settings = *(render->gett_light_settings());
    settings.sun_color = vec3(1.1f, 1.1f, 0.9f);
    settings.ambient_color = vec3(0.2f, 0, 0.4f);
    render->set_light_settings(&settings);
}

bool level_transition_t::is_entity_kept(const entity_t *entity) const {
    return entity->get_tag() == "persistent_data";
}

void level_transition_t::initialize_state
        (const tag_t &new_state, world_t *world) const {
    (void) new_state;
    
    reset_camera(world).log_failure();

    get_default_font(world->get_resources())
            .with_value([world](font_t *font) {
        create_label(world, *font, LABEL_LARGE | LABEL_POS_LEFT | LABEL_POS_TOP)
                .with_value([](entity_t *e){
            e->set_tag("health_label");
        });
        create_label(world, *font, LABEL_LARGE | LABEL_POS_LEFT | LABEL_POS_TOP)
                .with_value([](entity_t *e) {
            e->set_tag("ammo_label");
            const vec3_t pos = e->get_position();
            e->receive_message(MSG_PHYSICS_MOVE, vec3_add(pos, vec3(0, -48, 0)));
        });
    });

    create_button(world, vec2(410, 280), vec2(60, 60), CORE_RESTART_LEVEL, "reset-button.png");

    const portal_t *portal = get_saved_portal(world);
    create_core(world, portal);

    create_input_controller(world);
    create_fade_circle(world, 700, 1.2f, false);
}
