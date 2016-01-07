#include "button.h"

#include "warp/world.h"
#include "warp/entity.h"
#include "warp/components.h"
#include "warp/textures.h"
#include "warp/input.h"

#include "core.h"
#include "text-label.h"

using namespace warp;

static const float BUTTON_ANIM_TIME = 1.0f;

class button_controller_t final : public controller_impl_i {
    public:
        button_controller_t(vec2_t pos, vec2_t size, int msg_type)
            : _pos(pos)
            , _size(size)
            , _msg_type(msg_type)
            , _timer(0)
        {}

        dynval_t get_property(const tag_t &) const override {
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
                const gesture_t g = VALUE(message.data.get_gesture());
                if (g.kind == GESTURE_TAP 
                        && is_touching_button(g.final_position)) {
                    _world->broadcast_message(_msg_type, 0);
                    _timer = BUTTON_ANIM_TIME;
                }
            }
        }

    private:
        entity_t *_owner;
        world_t *_world;
        vec2_t _screen_size;
        vec2_t _pos, _size;
        int _msg_type;
        float _timer;
};

controller_comp_t *create_button_controller
        (world_t *world, vec2_t pos, vec2_t size, int msg_type) {
    controller_comp_t *controller = world->create_controller();
    controller->initialize(new button_controller_t(pos, size, msg_type));
    return controller;
}

static maybeunit_t fill_texured_quad
        ( model_t * const model, const resources_t &resources
        , const vec2_t size, const char * const texture_name
        ) {
    meshmanager_t * const meshes = resources.meshes;
    texturemgr_t * const textures = resources.textures;
    
    maybe_t<mesh_id_t> maybe_quad_id
        = meshes->generate_xy_quad_mesh(vec2(0, 0), size);
    MAYBE_RETURN(maybe_quad_id, unit_t, "Failed to get mesh:");
    mesh_id_t mesh_id = VALUE(maybe_quad_id);

    maybe_t<size_t> maybe_tex_id = textures->add_texture(texture_name);
    MAYBE_RETURN(maybe_tex_id, unit_t, "Failed to get texture:");
    const tex_id_t tex_id = VALUE(maybe_tex_id);

    model->initialize(mesh_id, tex_id);
    return unit;
}

extern graphics_comp_t *create_button_graphics
        (world_t *world, vec2_t size, const char *texture, vec4_t color) {
    const resources_t &res = world->get_resources();

    model_t model;
    maybeunit_t maybe_filled = fill_texured_quad(&model, res, size, texture);
    if (maybe_filled.failed()) {
        maybe_filled.log_failure("Failed to create button graphics");
        return nullptr;
    }

    model.set_color(color);

    graphics_comp_t *graphics = world->create_graphics();
    if (graphics == nullptr) {
        warp_log_e("Failed to create graphics");
        return nullptr;
    }

    graphics->add_model(model);
    graphics->set_pass_tag("ui");
    return graphics;
}

extern entity_t *create_ui_background(world_t *world, vec4_t color) {
    graphics_comp_t *graphics
        = create_button_graphics(world, vec2(1024, 800), "blank.png", color);
    if (graphics == nullptr) {
        warp_log_e("Failed to create background graphics.");
        return nullptr;
    }

    const vec3_t position = vec3(0, 0, -8);
    entity_t *entity = world->create_entity(position, graphics, nullptr, nullptr);
    entity->set_tag("background");
    return entity;
}

extern entity_t *create_button
        (world_t *world, vec2_t pos, vec2_t size, int msg_type, const char *tex) {
    graphics_comp_t *graphics
            = create_button_graphics(world, size, tex, vec4(1, 1, 1, 1));
    if (graphics == nullptr) {
        warp_log_e("Failed to create button graphics.");
        return nullptr;
    }

    controller_comp_t *controller
        = create_button_controller(world, pos, size, msg_type);
    if (graphics == nullptr) {
        warp_log_e("Failed to create button controller.");
        return nullptr;
    }

    const vec3_t position = vec3(pos.x, pos.y, 8);
    entity_t *entity = world->create_entity(position, graphics, nullptr, controller);
    entity->set_tag("button");
    return entity;
}

extern entity_t *create_text_button
        (world_t *world, vec2_t pos, vec2_t size, int msg_type, const char *text) {
    graphics_comp_t *graphics
            = create_button_graphics(world, size, "blank.png", vec4(0.8f, 0.8f, 0.8f, 1));
    if (graphics == nullptr) {
        warp_log_e("Failed to create button graphics.");
        return nullptr;
    }

    controller_comp_t *controller
        = create_button_controller(world, pos, size, msg_type);
    if (graphics == nullptr) {
        warp_log_e("Failed to create button controller.");
        return nullptr;
    }

    font_t *font = get_default_font(world->get_resources());
    if (font == NULL) {
        warp_log_e("Failed to create text button, couldn't obtain font.");
        return nullptr;
    }

    const float x = pos.x - size.x * 0.5f + 15;
    const float y = pos.y + size.y * 0.5f - 15;

    entity_t *label = VALUE(create_label(world, *font, LABEL_POS_TOP | LABEL_POS_LEFT));
    label->receive_message(MSG_PHYSICS_MOVE, vec3(x, y, 0));
    label->receive_message(CORE_SHOW_POINTER_TEXT, (void *)text);
    label->set_tag("button-label");

    const vec3_t position = vec3(pos.x, pos.y, -2);
    entity_t *entity = world->create_entity(position, graphics, nullptr, controller);
    entity->set_tag("text-button");

    return entity;
}
