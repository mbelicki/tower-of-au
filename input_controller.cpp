#include "input_controller.h"

#include "warp/keycodes.h"

#include "warp/input.h"
#include "warp/world.h"
#include "warp/entity.h"
#include "warp/components.h"
#include "warp/textures.h"

#include "core.h"

using namespace warp;

static const vec2_t ATTACK_POSITION = vec2(-200, -300);
static const vec2_t ATTACK_SIZE = vec2(80, 80);

class input_controller_t final : public controller_impl_i {
    public:
        dynval_t get_property(const tag_t &) const override {
            return dynval_t::make_null();
        }

        void initialize(entity_t *owner, world_t *world) override {
            _owner = owner;
            _world = world;
        }

        void update(float, const input_t &input) override {
            _screen_size = input.screen_size;
        }

        bool accepts(messagetype_t type) const override {
            return type == MSG_INPUT_GESTURE_DETECTED
                || type == MSG_INPUT_KEYUP
                ;
        }

        void handle_message(const message_t &message) override {
            const messagetype_t type = message.type;
            if (type == MSG_INPUT_GESTURE_DETECTED) {
                const gesture_t gesture = VALUE(message.data.get_gesture());
                if (gesture.kind == GESTURE_UP_SWIPE) {
                    move(MOVE_UP);
                } else if (gesture.kind == GESTURE_DOWN_SWIPE) {
                    move(MOVE_DOWN);
                } else if (gesture.kind == GESTURE_LEFT_SWIPE) {
                    move(MOVE_LEFT);
                } else if (gesture.kind == GESTURE_RIGHT_SWIPE) {
                    move(MOVE_RIGHT);
                }
            } else if (type == MSG_INPUT_KEYUP) {
                SDL_Keycode code = VALUE(message.data.get_int());
                if (code == SDLK_w) {
                    move(MOVE_UP);
                } else if (code == SDLK_s) {
                    move(MOVE_DOWN);
                } else if (code == SDLK_a) {
                    move(MOVE_LEFT);
                } else if (code == SDLK_d) {
                    move(MOVE_RIGHT);
                }
            }
        }

    private:
        entity_t *_owner;
        world_t *_world;
        vec2_t _screen_size;
        
        void move(move_dir_t direction) {
            _world->broadcast_message(CORE_TRY_MOVE, (int)direction);
        }

        /* TODO: should not be needed later... */
        vec2_t normalize_screen_position(vec2_t mouse_pos) {
            const float x = mouse_pos.x - _screen_size.x * 0.5f;
            const float y = mouse_pos.y - _screen_size.y * 0.5f;
            return vec2(x, y);
        }
};

maybe_t<entity_t *> create_input_controller(world_t *world) {
    controller_comp_t *controller = world->create_controller();
    controller->initialize(new input_controller_t);

    entity_t *entity = world->create_entity(vec3(0, 0, 0), nullptr, nullptr, controller);

    return entity;
}
