#include "editor_transitions.h"

#include "warp/world.h"
#include "warp/collections/array.h"
#include "warp/utils/str.h"
#include "warp/utils/io.h"

#include "core.h"

#include "button.h"
#include "text-label.h"
#include "transition_effect.h"

using namespace warp;

void enter_editor_transition_t::configure_renderer(renderer_t *) { }
bool enter_editor_transition_t::is_entity_kept(const entity_t *) const {
    return false;
}

static void create_region_list(world_t *world, const warp_array_t *regions) {
    for (size_t i = 0; i < warp_array_get_size(regions); i++) {
        warp_str_t region = warp_array_get_value(warp_str_t, regions, i);
        const char *name = warp_str_value(&region);
        const float y = 200.0f - 80.0f * i;
        create_text_button(world, vec2(-210, y), vec2(500, 60), 0, name);
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
    create_region_list(world, &regions);

    create_fade_circle(world, 700, 1.2f, false);
    create_ui_background(world, vec4(1, 1, 1, 1));
}
