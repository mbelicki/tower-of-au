#include "editor_transitions.h"

#include "warp/world.h"
#include "warp/collections/array.h"
#include "warp/utils/str.h"
#include "warp/utils/io.h"

#include "core.h"

#include "button.h"
#include "text-label.h"
#include "transition_effect.h"

#include "region.h"

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
            world->request_state_change("editor-region", 0.01f);
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

void enter_editor_transition_t::initialize_state(const tag_t &, world_t *world) {
    font_t *font = get_default_font(world->get_resources());
    if (font != NULL) {
        create_label(world, *font, LABEL_LARGE | LABEL_POS_LEFT | LABEL_POS_TOP)
                .with_value([](entity_t *e) {
            e->set_tag("title");
            e->receive_message(CORE_SHOW_POINTER_TEXT, (void *)"select a region:");
        });
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

void edit_region_transition_t::initialize_state(const tag_t &, world_t *world) {
    warp_str_t region_name = _state->get_region_name();
    const char *name = warp_str_value(&region_name);
    //region_t *region = load_region(name);   

    font_t *font = get_default_font(world->get_resources());
    if (font != NULL) {
        create_label(world, *font, LABEL_POS_LEFT | LABEL_POS_TOP)
                .with_value([name](entity_t *e) {
            e->set_tag("title");
            e->receive_message(CORE_SHOW_POINTER_TEXT, (void *)name);
        });
    }

    //size_t width = region->get_width();
    //size_t height = region->get_height();
    
    create_ui_background(world, vec4(1, 1, 1, 1));
}
