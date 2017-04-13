#define WARP_DROP_PREFIX
#include "level_transition.h"

#include "warp/world.h"
#include "warp/renderer.h"
#include "warp/entity-helpers.h"

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
    resources_t *res = world->get_resources();
    const res_id_t id = resources_lookup(res, "cam:main");
    if (id == WARP_RES_ID_INVALID) {
        warp_critical("Failed to get main camera.");
    }
    const camera_t *cam = resources_get_camera(res, id);
    const float ratio = cam->aspect;

    camera_t modified = *cam;
    camera_init(&modified, WARP_PROJ_PERSP
               , vec3(6, 10, 10.4f), quat_from_euler(0, 0, -1.12f)
               , 0.44f, ratio, 7.0f, 19.0f
               );

    camera_resource_change(res, id, &modified);
}

void level_transition_t::configure_renderer(renderer_t *renderer) { 
    _renderer = renderer;
}

static void setup_light(renderer_t *renderer, region_lighting_t *light) {
    light_settings_t settings = *(renderer->get_light_settings());
    settings.sun_color = light->sun_color;
    settings.sun_direction = light->sun_direction;
    settings.ambient_color = light->ambient_color;
    settings.shadow_map_center = vec3(6, 0, 5);
    settings.shadow_map_viewport_size = 15.0f;
    renderer->set_light_settings(&settings);
}

bool level_transition_t::is_entity_kept(const entity_t *entity) const {
    return warp_tag_equals_buffer(&entity->get_tag(), "persistent_data");
}

static entity_t *create_static_mesh
        ( world_t *world
        , const char *mesh, const char *tex
        , vec3_t pos, quat_t rot
        ) {
    graphics_comp_t * graphics = create_single_model_graphics(world, mesh, tex);
    graphics->remove_pass_tags();
    graphics->add_pass_tag(WARP_TAG("ui"));
    entity_t *entity = world->create_entity(pos, graphics, NULL, NULL);
    entity->receive_message(MSG_PHYSICS_ROTATE, rot);
    return entity;
}

void level_transition_t::initialize_state(const warp_tag_t &, world_t *world) {
    reset_camera(world);

    const res_id_t font = get_default_font(world->get_resources());

    const label_flags_t flags = LABEL_LARGE | LABEL_POS_LEFT | LABEL_POS_TOP;
    entity_t *hp = create_label(world, font, flags);
    if (hp != NULL) {
        hp->set_tag(WARP_TAG("health_label"));
    }

    entity_t *ammo = create_label(world, font, flags);
    if (ammo == NULL) {
        ammo->set_tag(WARP_TAG("ammo_label"));
        const vec3_t pos = ammo->get_position();
        ammo->receive_message(MSG_PHYSICS_MOVE, vec3_add(pos, vec3(0, -48, 0)));
        ammo->receive_message(MSG_GRAPHICS_VISIBLITY, 0);
    }

    entity_t *diag = create_label(world, font, LABEL_POS_LEFT);
    if (diag != NULL) {
        diag->set_tag(WARP_TAG("diag_label"));
        diag->receive_message(MSG_PHYSICS_MOVE, vec3(-340, 340, 8));
        diag->receive_message(MSG_PHYSICS_SCALE, vec3(0.8f, 0.8f, 0.8f));
        diag->receive_message(MSG_GRAPHICS_VISIBLITY, 0);
    }

    std::function<void(void)> restart_handler = [=]() {
        world->broadcast_message(CORE_RESTART_LEVEL, 0);
    };
    create_button(world, vec2(410, 280), vec2(60, 60), restart_handler, "reset-button.png");

    std::function<void(void)> reset_handler = [=]() {
        world->broadcast_message(CORE_SAVE_RESET_DEFAULTS, 0);
    };
    create_button(world, vec2(410, 200), vec2(60, 60), reset_handler, "reset-button.png");

    //entity_t *preview
    //    = create_button(world, vec2(320, -220), vec2(256, 256), [](){}, "warp:shadow_map");
    //preview->receive_message(MSG_PHYSICS_ROTATE, quat_from_euler(PI, 0, 0));

    const portal_t *portal = get_saved_portal(world);
    entity_t *core = create_core(world, portal);
    region_lighting_t *light
        = (region_lighting_t *) core->get_property(WARP_TAG("lighting")).get_pointer();
    if (_renderer != NULL) {
        setup_light(_renderer, light);
    }

    create_input_controller(world);
    create_fade_circle(world, 700, 1.2f, false);

    create_static_mesh
        ( world, "gen:unit-cube", "missing.png"
        , vec3(0, 0, 0), quat_from_euler(0, 0, 0.4f)
        );
}
