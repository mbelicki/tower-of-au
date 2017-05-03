#define WARP_DROP_PREFIX
#include "input_controller.h"

#include "warp/keycodes.h"

#include "warp/input.h"
#include "warp/world.h"
#include "warp/entity.h"
#include "warp/components.h"
#include "warp/math/utils.h"

#include "core.h"
#include "button.h"

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
        dynval_t get_property(const warp_tag_t &) const override {
            return dynval_t::make_null();
        }

        void initialize(entity_t *owner, world_t *world) override {
            _owner = owner;
            _world = world;

            _screen_size = vec2(1, 1);
            
            _is_shooting_enabled = false;
            _owner->receive_message
                (MSG_GRAPHICS_VISIBLITY, (int)_is_shooting_enabled);

            _is_button_touched = false;
            _is_move_touched = false;
            _button_scale = 1;

            _acc_initialized = false;

            _shift_acc = vec2(0, 0);
            _reference_acc = vec2(0, 0);

        }

        void update(float, const input_t &input) override {
            _screen_size = input.screen_size;
            update_camera(vec2(input.acc_x, input.acc_y));

            bool any_touched_button = false;
            bool any_touched_elswhere = false;
            if (input.touch_points != NULL && input.touch_points->size() > 0) {
                for (const touch_point_t *point : *input.touch_points) {
                    const vec2_t start = point->start_position;
                    if (is_touching_button(start) && _is_shooting_enabled) {
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

        bool is_touching_button(vec2_t screen_pos) {
            vec2_t pos = vec2_sub( screen_pos , vec2_scale(_screen_size, 0.5f));
            pos.y = -pos.y;

            const vec2_t diff = vec2_sub(pos, vec3_get_xy(ATTACK_POSITION));
            return fabs(diff.x) < ATTACK_SIZE.x * 0.5f 
                && fabs(diff.y) < ATTACK_SIZE.y * 0.5f;
        }

        void handle_message(const message_t &message) override {
            const messagetype_t type = message.type;
            if (type == CORE_INPUT_ENABLE_SHOOTING) {
                const bool value = (bool) message.data.get_int();
                enable_shooting(value);
            } else if (type == MSG_INPUT_KEY_UP) {
                SDL_Keycode code = message.data.get_int();
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

        bool _is_shooting_enabled;
        bool _is_button_touched;
        bool _is_move_touched;
        float _button_scale;
        gesture_t _button_gesture;
        gesture_t _move_gesture;

        bool _acc_initialized;
        vec2_t _shift_acc;
        vec2_t _reference_acc;

        void enable_shooting(bool enable) {
            _is_shooting_enabled = enable;
            _owner->receive_message(MSG_GRAPHICS_VISIBLITY, (int)enable);
        }

        void update_shift(vec2_t shift, float k) {
            _shift_acc.x = shift.x * k + _shift_acc.x * (1 - k);
            _shift_acc.y = shift.y * k + _shift_acc.y * (1 - k);
            
            k = saturatef(k * 2.0f, 0, 1);
            _reference_acc.x = _shift_acc.x * k + _reference_acc.x * (1 - k);
            _reference_acc.y = _shift_acc.y * k + _reference_acc.y * (1 - k);
        }

        void update_camera(vec2_t shift) {
            if (_acc_initialized == false) {
                _shift_acc = _reference_acc = shift;
                _acc_initialized = true;
            }

            update_shift(shift, 0.005f);
            
            const float max_mod_x = 48;
            const float max_mod_y = 48;
            vec2_t mod = vec2_sub(_shift_acc, _reference_acc);
            mod.x = saturatef(mod.x, -max_mod_x, max_mod_x);
            mod.y = saturatef(mod.y, -max_mod_y, max_mod_y);

            const vec3_t pos = vec3(6, 10, 10.4f);
            const vec3_t rot = vec3(1.12f + mod.y * 0.001f, 0, mod.x * 0.001f);
            const quat_t orientation =  quat_from_euler(rot.z, -rot.y, -rot.x);
            
            resources_t *res = _world->get_resources();
            const res_id_t id = resources_lookup(res, "cam:main");
            warp_camera_resource_change_transfroms(res, id, pos, orientation);
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

entity_t *create_input_controller(world_t *world) {
    graphics_comp_t *graphics
        = create_button_graphics(world, ATTACK_SIZE, "button.png", vec4(1, 1, 1, 1));
    if (graphics == NULL) { 
        warp_log_e("Failed to create input controller graphics.");
        return NULL;
    }

    controller_comp_t *controller = world->create_controller();
    controller->initialize(new input_controller_t);

    entity_t *entity = world->create_entity(ATTACK_POSITION, graphics, NULL, controller);
    entity->set_tag(WARP_TAG("input"));
    return entity;
}
