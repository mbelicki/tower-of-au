#define WARP_DROP_PREFIX
#include "text-label.h"

#include "warp/math/utils.h"
#include "warp/math/mat4.h"
#include "warp/world.h"
#include "warp/entity.h"
#include "warp/graphics/mesh.h"
#include "warp/graphics/mesh-manager.h"
#include "warp/graphics/texture-manager.h"

#include "core.h"

using namespace warp;

static const char *get_known_text(known_text_t id) {
    switch (id) {
        case TEXT_GAME_OVER:
            return "GAME OVER";
        default:
            return "<unknown text id>";
    }
}

extern warp_font_t *get_default_font() {
    warp_font_t *font = (warp_font_t *) malloc(sizeof *font);
    warp_font_create_from_grid(32, 64, 512, 512, "font.png", font);
    return font;
}

static int label_mesh_id = 0;

class label_controller_t final : public controller_impl_i {
    public:
        label_controller_t
            (const warp_font_t *font, float scale, warp_font_alignment_t align)
              : _font(font)
              , _align(align)
              , _scale(scale)
              , _mesh_id(0)
        {}

        dynval_t get_property(const warp_tag_t &) const override {
            return dynval_t::make_null();
        }

        void initialize(entity_t *owner, world_t *world) override {
            _owner = owner;
            _world = world;
        }

        void update(float, const input_t &) override { }

        bool accepts(messagetype_t type) const override {
            return type == CORE_SHOW_KNOWN_TEXT
                || type == CORE_SHOW_TAG_TEXT
                || type == CORE_SHOW_POINTER_TEXT
                ;
        }

        void handle_message(const message_t &message) override {
            messagetype_t type = message.type;
            if (type == CORE_SHOW_KNOWN_TEXT) {
                const known_text_t id = (known_text_t) message.data.get_int();
                recreate_text_mesh(get_known_text(id));
            } else if (type == CORE_SHOW_TAG_TEXT) {
                const warp_tag_t tag = message.data.get_tag();
                recreate_text_mesh(tag.text);
            } else if (type == CORE_SHOW_POINTER_TEXT) {
                const char *text = (const char *) message.data.get_pointer();
                recreate_text_mesh(text);
            }
        }

        void recreate_text_mesh(const char *str) {
            warp_str_t text = WARP_STR(str);
            const size_t count = warp_font_tesslated_buffer_size(_font, &text);
            vertex_t *vertices = (vertex_t *)malloc(count * sizeof *vertices);

            warp_font_tesselate(_font, vertices, count, &text, _align);
            warp_str_destroy(&text);
            
            if (_mesh_id == 0) {
                add_new_mesh(vertices, count);
            } else {
                mutate_mesh(vertices, count);
            }

            free(vertices);
        }


    private:
        world_t  *_world;
        entity_t *_owner;

        const warp_font_t *_font;
        
        warp_font_alignment_t _align;
        float _scale;
        res_id_t _mesh_id;
        model_t *_model;

        void add_new_mesh(vertex_t *vertices, size_t count) {
            resources_t *res = _world->get_resources();

            warp_str_t name = warp_str_format("label-%d", label_mesh_id++);
            warp_mesh_resource_from_buffer(res, warp_str_value(&name), vertices, count);
            warp_str_destroy(&name);

            const res_id_t tex_id
                = resources_lookup(res, warp_str_value(&_font->texture_name));

            _model = new model_t;
            model_init(_model, _mesh_id, tex_id);
            
            float rot_z[16]; mat4_fill_rotation_z(rot_z, PI * 0.5f);
            float rot_y[16]; mat4_fill_rotation_y(rot_y, PI * 0.5f);
            float transforms[16]; mat4_mul(transforms, rot_y, rot_z);
            mat4_fill_rotation_y(rot_y, PI * -0.5f);
            float final_trans[16]; mat4_mul(final_trans, transforms, rot_y);           
            float scale[16]; mat4_fill_scale(scale, vec3(_scale, _scale, _scale));
            mat4_mul(final_trans, final_trans, scale);

            model_change_local_transforms(_model, final_trans);

            _owner->receive_message(MSG_GRAPHICS_ADD_MODEL, _model);
            const vec3_t pos = _owner->get_position();
            _owner->receive_message(MSG_PHYSICS_MOVE, pos);
        }

        void mutate_mesh(vertex_t *vertices, size_t count) {
            resources_t *res = _world->get_resources();
            warp_mesh_resource_mutate(res, _mesh_id, vertices, count);
        }
};

static vec3_t get_position
        (label_flags_t flags, warp_font_alignment_t *origin, float font_size) {
    vec3_t position = vec3(0, 0, 0);
    *origin = WARP_FONT_ALIGN_CENTER;

    if ((flags & LABEL_PASS_MAIN) != 0) {
        return position;
    }

    if ((flags & LABEL_POS_TOP) != 0) {
        position.y += 350;
        *origin = (warp_font_alignment_t) (WARP_FONT_ALIGN_TOP | WARP_FONT_ALIGN_LEFT);
    }
    if ((flags & LABEL_POS_BOTTOM) != 0) {
        position.y -= 350 - font_size;
        *origin = WARP_FONT_ALIGN_CENTER;
    }
    if ((flags & LABEL_POS_LEFT) != 0) {
        position.x -= 460;
        *origin = (warp_font_alignment_t) (WARP_FONT_ALIGN_TOP | WARP_FONT_ALIGN_LEFT);
    }
    if ((flags & LABEL_POS_RIGHT) != 0) {
        position.x += 360;
        *origin = (warp_font_alignment_t) (WARP_FONT_ALIGN_TOP | WARP_FONT_ALIGN_LEFT);
    }

    return position;
}

extern entity_t *create_label
        (world_t *world, const warp_font_t *font, label_flags_t flags) {
    warp_font_alignment_t align = WARP_FONT_ALIGN_CENTER;
    const vec3_t position = get_position(flags, &align, font->line_height);

    graphics_comp_t *graphics = world->create_graphics();
    const warp_tag_t pass = WARP_TAG((flags & LABEL_PASS_MAIN) != 0 ? "main" : "ui");
    graphics->remove_pass_tags();
    graphics->add_pass_tag(pass);

    controller_comp_t *controller = world->create_controller();
    
    label_controller_t *label_ctrl = new label_controller_t(font, 1, align);
    controller->initialize(label_ctrl);

    return world->create_entity(position, graphics, nullptr, controller);
}

/* TODO: move outside this module */

enum shrink_state_t {
    SHRINK_GROWING,
    SHRINK_STABLE,
    SHRINK_SHRINKING,
};

class shrink_controller_t final : public controller_impl_i {
    public:
        shrink_controller_t()
            : _owner(nullptr)
            , _world(nullptr)
            , _timer(0)
            , _state(SHRINK_GROWING)
        { }

        dynval_t get_property(const warp_tag_t &) const override {
            return dynval_t::make_null();
        }

        void initialize(entity_t *owner, world_t *world) override {
            _owner = owner;
            _world = world;

            change_state(SHRINK_GROWING);
        }

        void update(float dt, const input_t &) override { 
            if (_timer <= 0) {
                if (_state == SHRINK_GROWING) {
                    change_state(SHRINK_STABLE);
                } else if (_state == SHRINK_STABLE) {
                    change_state(SHRINK_SHRINKING);
                } else if (_state == SHRINK_SHRINKING) {
                    _world->destroy_later(_owner);
                } 
                return;
            } 

            _timer -= dt;
            const float t = _timer / get_time_for_state(_state);

            if (_state == SHRINK_SHRINKING) {
                update_dying(t);
            } else if (_state == SHRINK_GROWING) {
                update_rising(t);
            }
        }

        bool accepts(messagetype_t) const override { return false; }
        void handle_message(const message_t &) override { }

    private:
        entity_t *_owner;
        world_t *_world;
        float _timer;
        shrink_state_t _state;

        void change_state(shrink_state_t state) {
            _state = state;
            _timer = get_time_for_state(state);
        }

        float get_time_for_state(shrink_state_t state) {
            switch (state) {
                case SHRINK_GROWING:   return 2.50f;
                case SHRINK_STABLE:    return 0.25f;
                case SHRINK_SHRINKING: return 2.00f;
                default: return 0;
            }
        }

        void update_dying(float t) {
            const float scale = ease_cubic(t);
            const vec3_t scales = vec3(scale, scale, scale);
            _owner->receive_message(MSG_PHYSICS_SCALE, scales);
        }

        void update_rising(float t) {
            const float scale = ease_elastic(1 - t);
            const vec3_t scales = vec3(scale, scale, scale);
            _owner->receive_message(MSG_PHYSICS_SCALE, scales);
        }
};

class ballon_controller_t final : public controller_impl_i {
    public:
        ballon_controller_t(float velocity, float acceleration)
            : _owner(nullptr)
            , _world(nullptr)
            , _velocity(velocity)
            , _acceleration(acceleration)
        { }

        dynval_t get_property(const warp_tag_t &) const override {
            return dynval_t::make_null();
        }

        void initialize(entity_t *owner, world_t *world) override {
            _owner = owner;
            _world = world;
        }

        void update(float dt, const input_t &) override { 
            _velocity += dt * _acceleration;
            const vec3_t pos
                = vec3_add(_owner->get_position(), vec3(0, dt * _velocity, 0));
            _owner->receive_message(MSG_PHYSICS_MOVE, pos);
        }

        bool accepts(messagetype_t) const override { return false; }
        void handle_message(const message_t &) override { }

    private:
        entity_t *_owner;
        world_t *_world;
        float _velocity;
        float _acceleration;
};

entity_t *create_speech_bubble
        ( world_t *world, const warp_font_t *font
        , vec3_t position, const char *text
        ) {
    warp_font_alignment_t align = WARP_FONT_ALIGN_CENTER;

    graphics_comp_t *graphics = world->create_graphics();
    resources_t *res = world->get_resources();
    const res_id_t id = resources_load(res, "speech.obj");
    float trans[16]; mat4_fill_translation(trans, vec3(0, 0, -0.05f));
    model_t model;
    model_init(&model, id, 0);
    model_change_local_transforms(&model, trans);
    graphics->add_model(model);

    controller_comp_t *controller = world->create_controller();
    
    label_controller_t *label_ctrl = new label_controller_t(font, 0.008f, align);
    controller->initialize(label_ctrl);
    controller->add_controller(new shrink_controller_t);
    controller->add_controller(new ballon_controller_t(0.5f, -0.1f));

    entity_t *entity
        = world->create_entity(position, graphics, nullptr, controller);

    label_ctrl->recreate_text_mesh(text);

    entity->receive_message(MSG_PHYSICS_ROTATE, quat_from_euler(0, 0, 0.3f));

    return entity;
}
