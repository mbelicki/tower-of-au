#include "character.h"

#include "warp/world.h"
#include "warp/entity.h"
#include "warp/components.h"
#include "warp/camera.h"
#include "warp/cameras.h"
#include "warp/direction.h"
#include "warp/entity-helpers.h"

#include "core.h"

using namespace warp;

static const float AVATAR_MOVE_TIME = 0.1f;

enum avatar_state_t : unsigned int {
    AVAT_IDLE = 0,
    AVAT_MOVING,
};

class avatar_controller_t final : public controller_impl_i {
    public:
        dynval_t get_property(const tag_t &name) const override {
            if (name == "avat.is_idle") {
                return (int)(_state == AVAT_IDLE);
            }
            return dynval_t::make_null();
        }

        void initialize(entity_t *owner, world_t *world) override {
            _owner = owner;
            _world = world;
        }

        void update(float dt, const input_t &) override { 
            if (_timer <= 0) {
                _timer = 0;
                _state = AVAT_IDLE;
                return;
            } 

            _timer -= dt;

            if (_state == AVAT_MOVING) {
                update_movement();
            }
        }

        bool accepts(messagetype_t type) const override {
            return type == CORE_DO_MOVE;
        }

        void handle_message(const message_t &message) override {
            const messagetype_t type = message.type;
            if (type == CORE_DO_MOVE) {
                const vec3_t position = VALUE(message.data.get_vec3());
                start_moving(position);
            }
        }

    private:
        entity_t *_owner;
        world_t *_world;
        float _timer;
        avatar_state_t _state;

        vec3_t _target_pos;
        vec3_t _old_pos;

        void start_moving(vec3_t position) {
            _state = AVAT_MOVING;
            _timer = AVATAR_MOVE_TIME;
            _old_pos = _owner->get_position();
            _target_pos = position;
        }

        void update_movement() {
            if (_state != AVAT_MOVING) return;

            const float t = _timer / AVATAR_MOVE_TIME;
            const vec3_t position = vec3_lerp(_target_pos, _old_pos, t);

            _owner->receive_message(MSG_PHYSICS_MOVE, position);
        }
};

extern maybe_t<controller_comp_t *> create_character_controller(world_t *world) {
    controller_comp_t *controller = world->create_controller();
    controller->initialize(new avatar_controller_t);

    return controller;
}
