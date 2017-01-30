#define WARP_DROP_PREFIX
#include "editor_transitions.h"

#include "warp/math/utils.h"
#include "warp/world.h"
#include "warp/collections/array.h"
#include "warp/utils/str.h"
#include "warp/utils/io.h"

#include "core.h"

#include "button.h"
#include "text-label.h"
#include "transition_effect.h"

#include "region.h"
#include "level.h"

using namespace warp;

shared_editor_state_t::shared_editor_state_t() {
    _region_name = warp_str_create(NULL);
}

shared_editor_state_t::~shared_editor_state_t() {
    warp_str_destroy(&_region_name);
}

void enter_editor_transition_t::configure_renderer(renderer_t *) { }
bool enter_editor_transition_t::is_entity_kept(const entity_t *) const {
    return false;
}

static void create_region_list
        ( world_t *world, shared_editor_state_t *state
        , const warp_array_t *regions) {
    for (size_t i = 0; i < warp_array_get_size(regions); i++) {
        warp_str_t region = warp_array_get_value(warp_str_t, regions, i);
        const char *name = warp_str_value(&region);
        const float y = 220.0f - 80.0f * i;

        std::function<void(void)> handler = [=]() {
            world->request_state_change(WARP_TAG("editor-region"), 0.01f);
            state->set_region_name(&region);
        };
        create_text_button(world, vec2(-210, y), vec2(500, 60), handler, name);
    }
}

static void find_all_regions(warp_array_t *regions) {
    warp_str_t directory = warp_str_create("assets/levels");
    list_files_in_directory(&directory, regions, false);
    warp_str_destroy(&directory);
}

void enter_editor_transition_t::initialize_state(const warp_tag_t &, world_t *world) {
    warp_font_t *font = get_default_font();
    if (font != NULL) {
        const label_flags_t flags = LABEL_LARGE | LABEL_POS_LEFT | LABEL_POS_TOP;
        entity_t *label = create_label(world, font, flags);
        if (label != NULL) {
            label->set_tag(WARP_TAG("title"));
            label->receive_message(CORE_SHOW_POINTER_TEXT, (void *)"select a region:");
        }
    }

    warp_array_t regions;
    find_all_regions(&regions);
    create_region_list(world, _state, &regions);

    create_fade_circle(world, 700, 1.2f, false);
    create_ui_background(world, vec4(1, 1, 1, 1));
}

void edit_region_transition_t::configure_renderer(renderer_t *) { }
bool edit_region_transition_t::is_entity_kept(const entity_t *) const {
    return false;
}

static void create_level_list(world_t *world, const region_t *region) {
    const size_t width = region->get_width();
    const size_t height = region->get_height();

    for (size_t i = 0; i < width; i++) {
        for (size_t j = 0; j < height; j++) {
            const float x = -390.0f + 150.0f * i;
            const float y =  220.0f - 130.0f * j;

            std::function<void(void)> handler = [=]() {
                //world->request_state_change("editor-region", 0.01f);
                //state->set_region_name(&region);
            };
            entity_t *button
                = create_button(world, vec2(x, y), vec2(140, 120), handler, "missing.png");
            button->receive_message(MSG_PHYSICS_MOVE, vec3(x, y, 5));

            level_t *level = region->get_level_at(i, j);
            if (level->is_initialized() == false) {
                level->initialize(world, region);
            }

            const quat_t orientation = quat_from_euler(0, 0, PI / -2);

            const vec3_t pos = vec3(x - 120 * 0.5f, y + 100 * 0.5f, 7);
            entity_t *entity = level->get_entity();
            entity->receive_message(MSG_GRAPHICS_REMOVE_PASSES, 0);
            entity->receive_message(MSG_GRAPHICS_ADD_PASS, WARP_TAG("ui"));
            entity->receive_message(MSG_PHYSICS_MOVE, pos);
            entity->receive_message(MSG_PHYSICS_ROTATE, orientation);
            entity->receive_message(MSG_PHYSICS_SCALE, vec3(10, 10, -1));
        }
    }
}

void edit_region_transition_t::initialize_state(const warp_tag_t &, world_t *world) {
    warp_str_t region_name = _state->get_region_name();
    const char *name = warp_str_value(&region_name);
    region_t *region = load_region(name);   

    warp_font_t *font = get_default_font();
    if (font != NULL) {
        const label_flags_t flags = LABEL_POS_LEFT | LABEL_POS_TOP;
        entity_t *label = create_label(world, font, flags);
        if (label != NULL) {
            label->set_tag(WARP_TAG("title"));
            label->receive_message(CORE_SHOW_POINTER_TEXT, (void *)name);
        }
    }
    
    create_level_list(world, region);
    
    create_ui_background(world, vec4(1, 1, 1, 1));
}
