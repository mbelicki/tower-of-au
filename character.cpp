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

static const float CHAR_MOVE_TIME   = 0.1f;
static const float CHAR_BOUNCE_TIME = 0.15f;
static const float CHAR_ATTACK_TIME = 0.3f;
static const float CHAR_DYING_TIME  = CHAR_ATTACK_TIME;
static const float CHAR_HURT_TIME   = CHAR_ATTACK_TIME;

enum character_state_t : unsigned int {
    CHAR_IDLE = 0,
    CHAR_MOVING,
    CHAR_BOUNCING,
    CHAR_ATTACKING,
    CHAR_DYING,
    CHAR_HURT,
};

static float get_time_for_state(character_state_t state) {
    switch (state) {
        case CHAR_MOVING:    return CHAR_MOVE_TIME;
        case CHAR_BOUNCING:  return CHAR_BOUNCE_TIME;
        case CHAR_ATTACKING: return CHAR_ATTACK_TIME;
        case CHAR_DYING:     return CHAR_DYING_TIME;
        case CHAR_HURT:      return CHAR_HURT_TIME;
        default: return 0;
    }
}

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
                if (_state == CHAR_DYING) {
                    _world->destroy_later(_owner);
                }
                _timer = 0;
                _state = CHAR_IDLE;
                return;
            } 

            _timer -= dt;
            const float t = _timer / get_time_for_state(_state);

            if (_state == CHAR_MOVING) {
                update_movement(t);
            } else if (_state == CHAR_BOUNCING) {
                update_bouncing(t);
            } else if (_state == CHAR_ATTACKING) {
                update_attacking(t);
            } else if (_state == CHAR_DYING) {
                update_dying(t);
            } else if (_state == CHAR_HURT) {
                update_hurt(t);
            }
        }

        bool accepts(messagetype_t type) const override {
            return type == CORE_DO_MOVE
                || type == CORE_DO_ATTACK
                || type == CORE_DO_BOUNCE
                || type == CORE_DO_HURT
                || type == CORE_DO_DIE
                ;
        }

        void handle_message(const message_t &message) override {
            const messagetype_t type = message.type;
            maybe_t<vec3_t> maybe_pos = message.data.get_vec3();
            if (type == CORE_DO_MOVE) {
                change_state(CHAR_MOVING, VALUE(maybe_pos));
            } else if (type == CORE_DO_BOUNCE) {
                change_state(CHAR_BOUNCING, VALUE(maybe_pos));
            } else if (type == CORE_DO_ATTACK) {
                change_state(CHAR_ATTACKING, VALUE(maybe_pos));
            } else if (type == CORE_DO_DIE) {
                change_state(CHAR_DYING, VALUE(maybe_pos));
            } else if (type == CORE_DO_HURT) {
                change_state(CHAR_HURT, VALUE(maybe_pos));
            }
        }

    private:
        entity_t *_owner;
        world_t *_world;
        float _timer;
        character_state_t _state;

        vec3_t _target_pos;
        vec3_t _old_pos;

        void change_state(character_state_t state, vec3_t target) {
            _state = state;
            _timer = get_time_for_state(state);
            _old_pos = _owner->get_position();
            _target_pos = target;
        }

        void update_movement(float t) {
            const vec3_t position = vec3_lerp(_target_pos, _old_pos, t);
            _owner->receive_message(MSG_PHYSICS_MOVE, position);
        }

        void update_bouncing(float t) {
            const float k = sin(t * 2 * PI) * 0.15f;
            const vec3_t position = vec3_lerp(_old_pos, _target_pos, k);
            _owner->receive_message(MSG_PHYSICS_MOVE, position);
        }

        void update_attacking(float t) {
            const float k = sin(t * PI) * 0.5f;
            vec3_t position = vec3_lerp(_old_pos, _target_pos, k);
            position.y = fabs(sin(t * 2 * PI)) * 0.4f;
            _owner->receive_message(MSG_PHYSICS_MOVE, position);
        }

        void update_dying(float t) {
            const float scale = sin(t * PI * 0.5f);
            const vec3_t scales = vec3(scale, scale, scale);
            _owner->receive_message(MSG_PHYSICS_SCALE, scales);
        }

        void update_hurt(float t) {
            const float scale = 1 - sin(t * PI) * 0.5f;
            const vec3_t scales = vec3(scale, scale, scale);
            _owner->receive_message(MSG_PHYSICS_SCALE, scales);
        }
};

extern maybe_t<controller_comp_t *> create_character_controller(world_t *world) {
    controller_comp_t *controller = world->create_controller();
    controller->initialize(new character_controller_t);
    return controller;
}
