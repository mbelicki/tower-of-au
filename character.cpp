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

static const float MOVE_MOVE_TIME   = 0.1f;
static const float MOVE_BOUNCE_TIME = 0.15f;
static const float MOVE_ATTACK_TIME = 0.3f;

static const float HEAL_DYING_TIME  = MOVE_ATTACK_TIME;
static const float HEAL_HURT_TIME   = MOVE_ATTACK_TIME;

enum movement_state_t : unsigned int {
    MOVE_IDLE = 0,
    MOVE_MOVING,
    MOVE_BOUNCING,
    MOVE_ATTACKING,
};

enum health_state_t : unsigned int {
    HEAL_IDLE = 0,
    HEAL_DYING,
    HEAL_HURT,
};

static float get_time_for_char_state(movement_state_t state) {
    switch (state) {
        case MOVE_MOVING:    return MOVE_MOVE_TIME;
        case MOVE_BOUNCING:  return MOVE_BOUNCE_TIME;
        case MOVE_ATTACKING: return MOVE_ATTACK_TIME;
        default: return 0;
    }
}

static float get_time_for_heal_state(health_state_t state) {
    switch (state) {
        case HEAL_DYING: return HEAL_DYING_TIME;
        case HEAL_HURT:  return HEAL_HURT_TIME;
        default: return 0;
    }
}

class movement_controller_t final : public controller_impl_i {
    public:
        movement_controller_t(bool confirm_move)
            : _confirm(confirm_move)
        { }

        dynval_t get_property(const tag_t &name) const override {
            if (name == "avat.is_idle") {
                return (int)(_state == MOVE_IDLE);
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
                if (_state != MOVE_IDLE && _confirm) {
                    _world->broadcast_message(CORE_MOVE_DONE, 0);
                }
                _state = MOVE_IDLE;
                return;
            } 

            _timer -= dt;
            const float t = _timer / get_time_for_char_state(_state);

            if (_state == MOVE_MOVING) {
                update_movement(t);
            } else if (_state == MOVE_BOUNCING) {
                update_bouncing(t);
            } else if (_state == MOVE_ATTACKING) {
                update_attacking(t);
            }
        }

        bool accepts(messagetype_t type) const override {
            return type == CORE_DO_MOVE
                || type == CORE_DO_MOVE_IMMEDIATE
                || type == CORE_DO_ATTACK
                || type == CORE_DO_BOUNCE
                ;
        }

        void handle_message(const message_t &message) override {
            const messagetype_t type = message.type;
            const maybe_t<vec3_t> maybe_pos = message.data.get_vec3();
            if (type == CORE_DO_MOVE) {
                change_state(MOVE_MOVING, VALUE(maybe_pos));
            } else if (type == CORE_DO_MOVE_IMMEDIATE) {
                move_immediate(VALUE(maybe_pos));
            } else if (type == CORE_DO_BOUNCE) {
                change_state(MOVE_BOUNCING, VALUE(maybe_pos));
            } else if (type == CORE_DO_ATTACK) {
                change_state(MOVE_ATTACKING, VALUE(maybe_pos));
            }
        }

    private:
        entity_t *_owner;
        world_t *_world;
        float _timer;
        movement_state_t _state;
        bool _confirm;

        vec3_t _target_pos;
        vec3_t _old_pos;

        void move_immediate(vec3_t pos) {
            _owner->receive_message(MSG_PHYSICS_MOVE, pos);
            _target_pos = pos;
            _old_pos = pos;
            _timer = 0;
        }

        void change_state(movement_state_t state, vec3_t target) {
            _state = state;
            _timer = get_time_for_char_state(state);
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

};

class health_controller_t final : public controller_impl_i {
    public:
        dynval_t get_property(const tag_t &) const override {
            return dynval_t::make_null();
        }

        void initialize(entity_t *owner, world_t *world) override {
            _owner = owner;
            _world = world;
        }

        void update(float dt, const input_t &) override { 
            if (_timer <= 0) {
                if (_state == HEAL_DYING) {
                    _world->destroy_later(_owner);
                }
                _timer = 0;
                _state = HEAL_IDLE;
                return;
            } 

            _timer -= dt;
            const float t = _timer / get_time_for_heal_state(_state);

            if (_state == HEAL_DYING) {
                update_dying(t);
            } else if (_state == HEAL_HURT) {
                update_hurt(t);
            }
        }

        bool accepts(messagetype_t type) const override {
            return type == CORE_DO_HURT
                || type == CORE_DO_DIE
                ;
        }

        void handle_message(const message_t &message) override {
            const messagetype_t type = message.type;
            if (type == CORE_DO_DIE) {
                change_state(HEAL_DYING);
            } else if (type == CORE_DO_HURT) {
                change_state(HEAL_HURT);
            }
        }

    private:
        entity_t *_owner;
        world_t *_world;
        float _timer;
        health_state_t _state;

        void change_state(health_state_t state) {
            _state = state;
            _timer = get_time_for_heal_state(state);
        }

        void update_dying(float t) {
            const float scale = sin(t * PI * 0.5f);
            const vec3_t scales = vec3(scale, scale, scale);
            _owner->receive_message(MSG_PHYSICS_SCALE, scales);
        }

        void update_hurt(float t) {
            const float scale = 1 - sin(t * PI) * 0.3f;
            const vec3_t scales = vec3(scale, scale, scale);
            _owner->receive_message(MSG_PHYSICS_SCALE, scales);
        }
};

extern maybe_t<controller_comp_t *> create_character_controller
        (world_t *world, bool confirm_move) {
    controller_comp_t *controller = world->create_controller();
    controller->initialize(new movement_controller_t(confirm_move));
    controller->add_controller(new health_controller_t);
    return controller;
}

