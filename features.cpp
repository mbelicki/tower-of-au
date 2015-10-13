#include "features.h"

#include "warp/world.h"
#include "warp/entity.h"
#include "warp/components.h"
#include "warp/camera.h"
#include "warp/cameras.h"
#include "warp/direction.h"
#include "warp/entity-helpers.h"

#include "core.h"

using namespace warp;

static const float DOOR_MOVE_TIME = 0.1f;

class door_controller_t final : public controller_impl_i {
    public:
        dynval_t get_property(const tag_t &) const override {
            return dynval_t::make_null();
        }

        void initialize(entity_t *owner, world_t *world) override {
            _owner = owner;
            _world = world;
            _state = false;
            _timer = 0;
        }

        void update(float dt, const input_t &) override { 
            if (_timer <= 0) {
                _timer = 0;
                return;
            } 

            _timer -= dt;
            const float t = _timer / DOOR_MOVE_TIME;

            if (_state) {
                update_opening(t);
            } else {
                update_closing(t);
            }
        }

        bool accepts(messagetype_t type) const override {
            return type == CORE_FEAT_STATE_CHANGE;
        }

        void handle_message(const message_t &message) override {
            const messagetype_t type = message.type;
            if (type == CORE_FEAT_STATE_CHANGE) {
                message.data.get_int().with_value([this](int value) {
                    change_state(value > 0);
                });
            }
        }

    private:
        entity_t *_owner;
        world_t *_world;
        float _timer;
        bool _state;

        void change_state(bool state) {
            _state = state;
            _timer = DOOR_MOVE_TIME;
        }

        void update_closing(float t) {
            const float scale = -ease_cubic(t);
            const vec3_t position = _owner->get_position();
            const vec3_t pos = vec3(position.x, scale, position.z);
            _owner->receive_message(MSG_PHYSICS_MOVE, pos);
        }

        void update_opening(float t) {
            const float scale = -0.9f * ease_cubic(1 - t) + 0.1f;
            const vec3_t position = _owner->get_position();
            const vec3_t pos = vec3(position.x, scale, position.z);
            _owner->receive_message(MSG_PHYSICS_MOVE, pos);
        }
};

extern maybe_t<controller_comp_t *> create_door_controller(world_t *world) {
    controller_comp_t *controller = world->create_controller();
    controller->initialize(new door_controller_t);
    return controller;
}
