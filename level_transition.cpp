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

static void reset_camera(world_t *world) {
    camera_manager_t *cameras = world->get_resources().cameras;

    const maybe_t<const camera_t *> maybe_main_camera
        = cameras->get_camera_for_tag("main");
    const camera_t *main_camera = VALUE(maybe_main_camera);
    const float ratio = main_camera->get_aspect_ratio();

    maybe_t<camera_id_t> camera_id = cameras->create_persp_camera
        ("main", vec3(6, 10, 10.4f), quat_from_euler(0, 0, -1.12f), 0.44f, ratio, 7.0f, 19.0f);
    camera_id.log_failure("Failed to create camera");
}

void level_transition_t::configure_renderer(renderer_t *render) {
    if (_lighting != nullptr) {
        light_settings_t settings = *(render->gett_light_settings());
        settings.sun_color = _lighting->sun_color;
        settings.sun_direction = _lighting->sun_direction;
        settings.ambient_color = _lighting->ambient_color;
        settings.shadow_map_center = vec3(6, 0, 5);
        settings.shadow_map_viewport_size = 15.0f;
        render->set_light_settings(&settings);
    }
}

bool level_transition_t::is_entity_kept(const entity_t *entity) const {
    return entity->get_tag() == "persistent_data";
}

void level_transition_t::initialize_state(const tag_t &, world_t *world) {
    _lighting = nullptr;
    reset_camera(world);

    font_t *font = get_default_font(world->get_resources());
    if (font != NULL) {
        create_label(world, *font, LABEL_LARGE | LABEL_POS_LEFT | LABEL_POS_TOP)
                .with_value([](entity_t *e){
            e->set_tag("health_label");
        });
        create_label(world, *font, LABEL_LARGE | LABEL_POS_LEFT | LABEL_POS_TOP)
                .with_value([](entity_t *e) {
            e->set_tag("ammo_label");
            const vec3_t pos = e->get_position();
            e->receive_message(MSG_PHYSICS_MOVE, vec3_add(pos, vec3(0, -48, 0)));
            e->receive_message(MSG_GRAPHICS_VISIBLITY, 0);
        });
        create_label(world, *font, LABEL_POS_LEFT)
                .with_value([](entity_t *e) {
            e->set_tag("diag_label");
            e->receive_message(MSG_PHYSICS_MOVE, vec3(-340, 340, 8));
            e->receive_message(MSG_PHYSICS_SCALE, vec3(0.8f, 0.8f, 0.8f));
            e->receive_message(MSG_GRAPHICS_VISIBLITY, 0);
        });
    }

    std::function<void(void)> reset_handler = [=]() {
        world->broadcast_message(CORE_RESTART_LEVEL, 0);
    };
    create_button(world, vec2(410, 280), vec2(60, 60), reset_handler, "reset-button.png");

    //entity_t *preview
    //    = create_button(world, vec2(320, -220), vec2(256, 256), [](){}, "warp:shadow_map");
    //preview->receive_message(MSG_PHYSICS_ROTATE, quat_from_euler(PI, 0, 0));


    const portal_t *portal = get_saved_portal(world);
    entity_t *core = create_core(world, portal);
    _lighting = (region_lighting_t *) VALUE(core->get_property("lighting").get_pointer());

    create_input_controller(world);
    create_fade_circle(world, 700, 1.2f, false);
}
