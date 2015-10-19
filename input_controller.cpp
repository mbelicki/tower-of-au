#include "input_controller.h"

#include "warp/keycodes.h"

#include "warp/camera.h"
#include "warp/cameras.h"
#include "warp/input.h"
#include "warp/world.h"
#include "warp/entity.h"
#include "warp/components.h"
#include "warp/textures.h"

#include "core.h"

using namespace warp;

static const vec3_t ATTACK_POSITION = vec3(-300, -200, 8);
static const vec2_t ATTACK_SIZE = vec2(120, 120);

static move_dir_t gesture_kind_to_move(gesture_kind_t kind) {
    switch (kind) {
        case GESTURE_UP_SWIPE:
            return MOVE_UP;
        case GESTURE_LEFT_SWIPE:
            return MOVE_LEFT;
        case GESTURE_DOWN_SWIPE:
            return MOVE_DOWN;
        case GESTURE_RIGHT_SWIPE:
            return MOVE_RIGHT;
        default:
            return MOVE_NONE;
    }
}

class input_controller_t final : public controller_impl_i {
    public:
        dynval_t get_property(const tag_t &) const override {
            return dynval_t::make_null();
        }

        void initialize(entity_t *owner, world_t *world) override {
            _owner = owner;
            _world = world;

            _is_button_touched = false;
            _is_move_touched = false;
            _button_scale = 1;

            _acc_initialized = false;
        }

        void update(float, const input_t &input) override {
            _screen_size = input.screen_size;
            update_camera(vec2(input.acc_x, input.acc_y));

            bool any_touched_button = false;
            bool any_touched_elswhere = false;
            if (input.touch_points->size() > 0) {
                for (const touch_point_t *point : *input.touch_points) {
                    const vec2_t start = point->start_position;
                    if (is_touching_button(start)) {
                        any_touched_button = true;
                        _button_gesture = detect_gesture(*point);
                    } else {
                        any_touched_elswhere = true;
                        _move_gesture = detect_gesture(*point);
                    }
                }
            }

            if (_is_button_touched && (any_touched_button == false)) {
                shoot(gesture_kind_to_move(_button_gesture.kind));
            }
            _is_button_touched = any_touched_button;

            if (_is_move_touched && (any_touched_elswhere == false)) {
                move(gesture_kind_to_move(_move_gesture.kind));
            }
            _is_move_touched = any_touched_elswhere;

            if (_is_button_touched) {
                if (_button_scale < 3.0f) {
                    _button_scale += _button_scale * 0.1f;
                }
            } else {
                if (_button_scale > 1.0f) {
                    _button_scale -= _button_scale * 0.1f;
                }
            }

            if (_button_scale > 1.0f) {
                _owner->receive_message
                    (MSG_PHYSICS_SCALE, vec3(_button_scale, _button_scale, _button_scale));
            }
        }

        bool accepts(messagetype_t type) const override {
            return type == MSG_INPUT_KEYUP;
        }

        bool is_touching_button(vec2_t screen_pos) {
            vec2_t pos = vec2_sub( screen_pos , vec2_scale(_screen_size, 0.5f));
            pos.y = -pos.y;

            const vec2_t diff = vec2_sub(pos, ATTACK_POSITION.get_xy());
            return diff.x < ATTACK_SIZE.x * 0.5f 
                && diff.y < ATTACK_SIZE.y * 0.5f;
        }

        void handle_message(const message_t &message) override {
            const messagetype_t type = message.type;
            if (type == MSG_INPUT_KEYUP) {
                SDL_Keycode code = VALUE(message.data.get_int());
                if (code == SDLK_w) {
                    move(MOVE_UP);
                } else if (code == SDLK_s) {
                    move(MOVE_DOWN);
                } else if (code == SDLK_a) {
                    move(MOVE_LEFT);
                } else if (code == SDLK_d) {
                    move(MOVE_RIGHT);
                } else if (code == SDLK_LEFT) {
                    shoot(MOVE_LEFT);
                } else if (code == SDLK_RIGHT) {
                    shoot(MOVE_RIGHT);
                } else if (code == SDLK_UP) {
                    shoot(MOVE_UP);
                } else if (code == SDLK_DOWN) {
                    shoot(MOVE_DOWN);
                }
            }
        }

    private:
        entity_t *_owner;
        world_t *_world;
        vec2_t _screen_size;

        bool _is_button_touched;
        bool _is_move_touched;
        float _button_scale;
        gesture_t _button_gesture;
        gesture_t _move_gesture;

        bool _acc_initialized;
        vec2_t _shift_acc;
        vec2_t _reference_acc;

        void update_shift(vec2_t shift, float k) {
            _shift_acc.x = shift.x * k + _shift_acc.x * (1 - k);
            _shift_acc.y = shift.y * k + _shift_acc.y * (1 - k);
            
            k = saturate<float>(k * 2.0f, 0, 1);
            _reference_acc.x = _shift_acc.x * k + _reference_acc.x * (1 - k);
            _reference_acc.y = _shift_acc.y * k + _reference_acc.y * (1 - k);
        }

        void update_camera(vec2_t shift) {
            if (_acc_initialized == false) {
                _shift_acc = _reference_acc = shift;
                _acc_initialized = true;
            }

            update_shift(shift, 0.005f);
            
            vec2_t mod = vec2_sub(_shift_acc, _reference_acc);
            mod.x = saturate(mod.x, -48.0f, 48.0f);
            mod.y = saturate(mod.y, -48.0f, 48.0f);

            const vec3_t pos = vec3(6, 9.5f, 12.5f);
            const vec3_t rot = vec3(0.95f + mod.y * 0.001f, 0, mod.x * 0.001f);

            cameras_mgr_t *cameras = _world->get_resources().cameras;
            const maybe_t<camera_id_t> id = cameras->get_id_for_tag("main");
            cameras->change_transforms(VALUE(id), pos, rot);
        }
        
        void move(move_dir_t direction) {
            if (direction != MOVE_NONE) {
                _world->broadcast_message(CORE_TRY_MOVE, (int)direction);
            }
        }

        void shoot(move_dir_t direction) {
            if (direction != MOVE_NONE) {
                _world->broadcast_message(CORE_TRY_SHOOT, (int)direction);
            }
        }
};

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

static maybe_t<graphics_comp_t *> create_button_graphics(world_t *world) {
    model_t model;
    const resources_t &res = world->get_resources();
    maybeunit_t maybe_filled
        = fill_texured_quad(&model, res, ATTACK_SIZE, "button.png");
    MAYBE_RETURN(maybe_filled, graphics_comp_t *, "Failed to create quad:");

    graphics_comp_t *graphics = world->create_graphics();
    if (graphics == nullptr)
        return nothing<graphics_comp_t *>("Failed to create graphics.");
    graphics->add_model(model);
    graphics->set_pass_tag("ui");
    return graphics;
}

maybe_t<entity_t *> create_input_controller(world_t *world) {
    maybe_t<graphics_comp_t *> graphics = create_button_graphics(world);
    MAYBE_RETURN(graphics, entity_t *, "Failed to create graphics:");

    controller_comp_t *controller = world->create_controller();
    controller->initialize(new input_controller_t);

    entity_t *entity = world->create_entity(ATTACK_POSITION, VALUE(graphics), nullptr, controller);

    return entity;
}
