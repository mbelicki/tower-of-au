#include "persitence.h"

#include "warp/world.h"
#include "warp/entity.h"
#include "warp/components.h"

#include "region.h"
#include "level_state.h"

using namespace warp;

class persistence_controller_t final : public controller_impl_i {
    public:
        persistence_controller_t()
                : _owner(nullptr)
                , _world(nullptr) {
        }

        ~persistence_controller_t() {
            str_destroy(_portal.region_name);
        }

        dynval_t get_property(const tag_t &name) const override {
            if (name == "portal") {
                return (void *)&_portal;
            } else if (name == "player") {
                return (void *)&_player;
            }

            return dynval_t::make_null();
        }

        void initialize(entity_t *owner, world_t *world) override {
            _owner = owner;
            _world = world;

            /* default portal */
            _portal.region_name = str_create("overworld.json");
            _portal.level_x = 1;
            _portal.level_z = 1;
            _portal.tile_x = 6;
            _portal.tile_z = 5;

            /* default player */
            _player.type = OBJ_NONE;
            _player.entity = nullptr;
            _player.position = vec3(0, 0, 0);
            _player.direction = DIR_NONE;
            _player.flags = FOBJ_NONE;
            _player.health = 0;
            _player.ammo = 0;
        }

        void update(float, const input_t &) override { }

        bool accepts(messagetype_t type) const override {
            return type == CORE_SAVE_PORTAL || type == CORE_SAVE_PLAYER;
        }

        void handle_message(const message_t &message) override { 
            const messagetype_t type = message.type;
            if (type == CORE_SAVE_PLAYER) {
                maybe_t<void *> maybe_value = message.data.get_pointer();
                if (maybe_value.failed()) {
                    maybe_value.log_failure();
                    return;
                }
                const object_t *player = (object_t *) VALUE(maybe_value);
                _player = *player;
                _player.entity = nullptr;
                warp_log_d("player saved.");
            } else if (type == CORE_SAVE_PORTAL) {
                maybe_t<void *> maybe_value = message.data.get_pointer();
                if (maybe_value.failed()) {
                    maybe_value.log_failure();
                    return;
                }
                const portal_t *portal = (portal_t *) VALUE(maybe_value);
                str_destroy(_portal.region_name);
                _portal = *portal;
                _portal.region_name = str_copy(portal->region_name);
            }
        }

    private:
        entity_t *_owner;
        world_t *_world;

        portal_t _portal;
        object_t _player;
};

entity_t *get_persitent_data(world_t *world) {
    entity_t *data = world->find_entity("persistent_data");
    if (data == nullptr) {
        data = create_persitent_data(world);
    }
    return data;
}

entity_t *create_persitent_data(world_t *world) {
    controller_comp_t *controller = world->create_controller();
    controller->initialize(new persistence_controller_t);

    entity_t *entity
        = world->create_entity(vec3(0, 0, 0), nullptr, nullptr, controller);
    entity->set_tag("persistent_data");

    return entity;
}

static void *get_saved_data(world_t *world, const tag_t &name) {
    entity_t *data = get_persitent_data(world);
    maybe_t<void *> maybe_value = data->get_property(name).get_pointer();
    if (maybe_value.failed()) {
        maybe_value.log_failure();
        return nullptr;
    }
    return VALUE(maybe_value);
}

const object_t *get_saved_player_state(world_t *world) {
    return (const object_t *)get_saved_data(world, "player");
}

const portal_t *get_saved_portal(world_t *world) {
    return (const portal_t *)get_saved_data(world, "portal");
}

void save_player_state(world_t *world, const object_t *player) {
    entity_t *data = get_persitent_data(world);
    data->receive_message(CORE_SAVE_PLAYER, (void *)player);
}

void save_portal(world_t *world, const portal_t *portal) {
    entity_t *data = get_persitent_data(world);
    data->receive_message(CORE_SAVE_PORTAL, (void *)portal);
}
