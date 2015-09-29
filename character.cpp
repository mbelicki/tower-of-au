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

static const float CHAR_MOVE_TIME = 0.1f;
static const float CHAR_BOUNCE_TIME = 0.15f;
static const float CHAR_ATTACK_TIME = 0.3f;

enum character_state_t : unsigned int {
    CHAR_IDLE = 0,
    CHAR_MOVING,
    CHAR_BOUNCING,
    CHAR_ATTACKING,
};

class character_controller_t final : public controller_impl_i {
    public:
        dynval_t get_property(const tag_t &name) const override {
            if (name == "avat.is_idle") {
                return (int)(_state == CHAR_IDLE);
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
                _state = CHAR_IDLE;
                return;
            } 

            _timer -= dt;

            if (_state == CHAR_MOVING) {
                update_movement();
            } else if (_state == CHAR_BOUNCING) {
                update_bouncing();
            } else if (_state == CHAR_ATTACKING) {
                update_attacking();
            }
        }

        bool accepts(messagetype_t type) const override {
            return type == CORE_DO_MOVE
                || type == CORE_DO_ATTACK
                || type == CORE_DO_BOUNCE
                ;
        }

        void handle_message(const message_t &message) override {
            const messagetype_t type = message.type;
            if (type == CORE_DO_MOVE) {
                const vec3_t position = VALUE(message.data.get_vec3());
                start_moving(position);
            } else if (type == CORE_DO_BOUNCE) {
                const vec3_t position = VALUE(message.data.get_vec3());
                start_bouncing(position);
            } else if (type == CORE_DO_ATTACK) {
                const vec3_t position = VALUE(message.data.get_vec3());
                start_attacking(position);
            }
        }

    private:
        entity_t *_owner;
        world_t *_world;
        float _timer;
        character_state_t _state;

        vec3_t _target_pos;
        vec3_t _old_pos;

        void start_moving(vec3_t position) {
            _state = CHAR_MOVING;
            _timer = CHAR_MOVE_TIME;
            _old_pos = _owner->get_position();
            _target_pos = position;
        }

        void start_bouncing(vec3_t position) {
            _state = CHAR_BOUNCING;
            _timer = CHAR_BOUNCE_TIME;
            _old_pos = _owner->get_position();
            _target_pos = position;
        }

        void start_attacking(vec3_t position) {
            _state = CHAR_ATTACKING;
            _timer = CHAR_ATTACK_TIME;
            _old_pos = _owner->get_position();
            _target_pos = position;
        }

        void update_movement() {
            if (_state != CHAR_MOVING) return;

            const float t = _timer / CHAR_MOVE_TIME;
            const vec3_t position = vec3_lerp(_target_pos, _old_pos, t);

            _owner->receive_message(MSG_PHYSICS_MOVE, position);
        }

        void update_bouncing() {
            if (_state != CHAR_BOUNCING) return;

            const float t = _timer / CHAR_BOUNCE_TIME;
            const float k = sin(t * 2 * PI) * 0.15f;
            const vec3_t position = vec3_lerp(_old_pos, _target_pos, k);

            _owner->receive_message(MSG_PHYSICS_MOVE, position);
        }

        void update_attacking() {
            if (_state != CHAR_ATTACKING) return;

            const float t = _timer / CHAR_ATTACK_TIME;
            const float k = sin(t * PI) * 0.5f;
            vec3_t position = vec3_lerp(_old_pos, _target_pos, k);
            position.y = fabs(sin(t * 2 * PI)) * 0.4f;

            _owner->receive_message(MSG_PHYSICS_MOVE, position);
        }
};

extern maybe_t<controller_comp_t *> create_character_controller(world_t *world) {
    controller_comp_t *controller = world->create_controller();
    controller->initialize(new character_controller_t);
    return controller;
}
