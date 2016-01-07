#include "editor_transitions.h"

#include "warp/world.h"

#include "core.h"

#include "button.h"
#include "text-label.h"
#include "transition_effect.h"

using namespace warp;

void enter_editor_transition_t::configure_renderer(renderer_t *) { }
bool enter_editor_transition_t::is_entity_kept(const entity_t *) const {
    return false;
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

    create_text_button(world, vec2(-210, 200), vec2(500, 60), 0, "hello.json");
    create_fade_circle(world, 700, 1.2f, false);
    create_ui_background(world, vec4(1, 1, 1, 1));
}
