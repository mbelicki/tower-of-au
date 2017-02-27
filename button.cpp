#define WARP_DROP_PREFIX
#include "button.h"

#include "warp/world.h"
#include "warp/entity.h"
#include "warp/components.h"
#include "warp/input.h"
#include "warp/math/utils.h"
#include "warp/math/mat4.h"
#include "warp/resources/resources.h"

#include "core.h"
#include "text-label.h"

using namespace warp;

static const float BUTTON_ANIM_TIME = 1.0f;

class button_controller_t final : public controller_impl_i {
    public:
        button_controller_t
                (vec2_t pos, vec2_t size, std::function<void(void)> handler)
            : _pos(pos)
            , _size(size)
            , _handler(handler)
            , _timer(0)
        {}

        dynval_t get_property(const warp_tag_t &) const override {
            return dynval_t::make_null();
        }

        void initialize(entity_t *owner, world_t *world) override {
            _owner = owner;
            _world = world;
        }

        void update(float dt, const input_t &input) override {
            _screen_size = input.screen_size;

            if (_timer > 0) {
                _timer -= dt;
                const float k = ease_elastic(1 - _timer);
                const float scale = lerp(1.4f, 1, k);
                _owner->receive_message(MSG_PHYSICS_SCALE, vec3(scale, scale, scale));
            }
        }

        bool accepts(messagetype_t type) const override {
            return type == MSG_INPUT_GESTURE_DETECTED;
        }

        bool is_touching_button(vec2_t screen_pos) {
            vec2_t pos = vec2_sub(screen_pos, vec2_scale(_screen_size, 0.5f));
            pos.y = -pos.y;

            const vec2_t diff = vec2_sub(pos, _pos);
            return fabs(diff.x) < _size.x * 0.5f 
                && fabs(diff.y) < _size.y * 0.5f;
        }

        void handle_message(const message_t &message) override {
            const messagetype_t type = message.type;
            if (type == MSG_INPUT_GESTURE_DETECTED) {
                const gesture_t g = message.data.get_gesture();
                if (g.kind == GESTURE_TAP 
                        && is_touching_button(g.final_position)) {
                    _handler();
                    _timer = BUTTON_ANIM_TIME;
                }
            }
        }

    private:
        entity_t *_owner;
        world_t *_world;
        vec2_t _screen_size;
        vec2_t _pos, _size;
        std::function<void(void)> _handler;
        float _timer;
};

extern controller_comp_t *create_button_controller
        ( world_t *world, vec2_t pos, vec2_t size
        , std::function<void(void)> handler
        ) {
    controller_comp_t *controller = world->create_controller();
    controller->initialize(new button_controller_t(pos, size, handler));
    return controller;
}

static void fill_texured_quad
        (model_t *model, resources_t *res, const vec2_t size, const char *texture_name) {
    const res_id_t mesh_id = resources_lookup(res, "gen:unit-quad");
    const res_id_t tex_id = resources_load(res, texture_name);

    model_init(model, mesh_id, tex_id);

    float transforms[16];
    mat4_fill_scale(transforms, vec3(size.x, size.y, 1));
    model_change_local_transforms(model, transforms);
}

extern graphics_comp_t *create_button_graphics
        (world_t *world, vec2_t size, const char *texture, vec4_t color) {
    resources_t *res = world->get_resources();

    model_t model;
    fill_texured_quad(&model, res, size, texture);
    model.color = color;

    graphics_comp_t *graphics = world->create_graphics();
    if (graphics == NULL) {
        warp_log_e("Failed to create graphics");
        return NULL;
    }

    graphics->add_model(model);
    graphics->remove_pass_tags();
    graphics->add_pass_tag(WARP_TAG("ui"));
    return graphics;
}

extern entity_t *create_ui_background(world_t *world, vec4_t color) {
    graphics_comp_t *graphics
        = create_button_graphics(world, vec2(1024, 800), "blank.png", color);
    if (graphics == NULL) {
        warp_log_e("Failed to create background graphics.");
        return NULL;
    }

    const vec3_t position = vec3(0, 0, -8);
    entity_t *entity = world->create_entity(position, graphics, nullptr, nullptr);
    entity->set_tag(WARP_TAG("background"));
    return entity;
}

extern entity_t *create_button
        ( world_t *world, vec2_t pos, vec2_t size
        , std::function<void(void)> handler, const char *tex
        ) {
    graphics_comp_t *graphics
            = create_button_graphics(world, size, tex, vec4(1, 1, 1, 1));
    if (graphics == nullptr) {
        warp_log_e("Failed to create button graphics.");
        return nullptr;
    }

    controller_comp_t *controller
        = create_button_controller(world, pos, size, handler);
    if (graphics == nullptr) {
        warp_log_e("Failed to create button controller.");
        return nullptr;
    }

    const vec3_t position = vec3(pos.x, pos.y, 8);
    entity_t *entity = world->create_entity(position, graphics, nullptr, controller);
    entity->set_tag(WARP_TAG("button"));
    return entity;
}

extern entity_t *create_text_button
        ( world_t *world, vec2_t pos, vec2_t size
        , std::function<void(void)> handler, const char *text
        ) {
    graphics_comp_t *graphics
            = create_button_graphics(world, size, "blank.png", vec4(0.8f, 0.8f, 0.8f, 1));
    if (graphics == nullptr) {
        warp_log_e("Failed to create button graphics.");
        return nullptr;
    }

    controller_comp_t *controller
        = create_button_controller(world, pos, size, handler);
    if (graphics == nullptr) {
        warp_log_e("Failed to create button controller.");
        return nullptr;
    }

    warp_font_t *font = get_default_font();
    if (font == NULL) {
        warp_log_e("Failed to create text button, couldn't obtain font.");
        return NULL;
    }

    const float x = pos.x - size.x * 0.5f + 15;
    const float y = pos.y + size.y * 0.5f - 15;

    entity_t *label = create_label(world, font, LABEL_POS_TOP | LABEL_POS_LEFT);
    label->receive_message(MSG_PHYSICS_MOVE, vec3(x, y, 0));
    label->receive_message(CORE_SHOW_POINTER_TEXT, (void *)text);
    label->set_tag(WARP_TAG("button-label"));

    const vec3_t position = vec3(pos.x, pos.y, -2);
    entity_t *entity = world->create_entity(position, graphics, nullptr, controller);
    entity->set_tag(WARP_TAG("text-button"));

    return entity;
}
