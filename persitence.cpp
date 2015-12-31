#include "persitence.h"

#include "warp/world.h"
#include "warp/entity.h"
#include "warp/components.h"
#include "warp/io.h"

#include "libs/parson/parson.h"

#include "region.h"
#include "level_state.h"

using namespace warp;

static const char *SAVE_PATH = "./save.json";

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

            read_data();
        }

        void update(float, const input_t &) override { }

        bool accepts(messagetype_t type) const override {
            return type == CORE_SAVE_PORTAL 
                || type == CORE_SAVE_PLAYER
                || type == CORE_SAVE_TO_FILE
                ;
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
            } else if (type == CORE_SAVE_TO_FILE) {
                save_data();
            }
        }

    private:
        entity_t *_owner;
        world_t *_world;

        portal_t _portal;
        object_t _player;

        JSON_Value *save_portal() {
            JSON_Value *portal_value = json_value_init_object();
            JSON_Object *portal = json_value_get_object(portal_value);
            
            json_object_set_string(portal, "region_name", str_value(_portal.region_name)); 
            json_object_set_number(portal, "tile_x", _portal.tile_x); 
            json_object_set_number(portal, "tile_z", _portal.tile_z); 
            json_object_set_number(portal, "level_x", _portal.level_x); 
            json_object_set_number(portal, "level_z", _portal.level_z); 

            return portal_value;
        }

        void read_portal(JSON_Object *portal) {
            const char *region = json_object_get_string(portal, "region_name");
            if (region != nullptr) {
                str_destroy(_portal.region_name);
                _portal.region_name = str_create(region);
            }
            
            if (json_object_get_value(portal, "tile_x") != nullptr) {
                _portal.tile_x = json_object_get_number(portal, "tile_x");
            }
            if (json_object_get_value(portal, "tile_z") != nullptr) {
                _portal.tile_z = json_object_get_number(portal, "tile_z");
            }
            if (json_object_get_value(portal, "level_x") != nullptr) {
                _portal.level_x = json_object_get_number(portal, "level_x");
            }
            if (json_object_get_value(portal, "level_z") != nullptr) {
                _portal.level_z = json_object_get_number(portal, "level_z");
            }
        }

        JSON_Value *save_vec3(vec3_t v) {
            JSON_Value *vector_value = json_value_init_object();
            JSON_Object *vector = json_value_get_object(vector_value);

            json_object_set_number(vector, "x", v.x);
            json_object_set_number(vector, "y", v.y);
            json_object_set_number(vector, "z", v.z);

            return vector_value;
        }

        vec3_t read_vec3(const JSON_Object *vector) {
            vec3_t v;
            v.x = json_object_get_number(vector, "x");
            v.y = json_object_get_number(vector, "y");
            v.z = json_object_get_number(vector, "z");
            return v;
        }

        JSON_Value *save_player() {
            JSON_Value *player_value = json_value_init_object();
            JSON_Object *player = json_value_get_object(player_value);
            
            json_object_set_number(player, "health", _player.health); 
            json_object_set_number(player, "ammo", _player.ammo); 
            json_object_set_number(player, "flags", _player.flags); 
            json_object_set_number(player, "direction", _player.direction); 
            json_object_set_value(player, "position", save_vec3(_player.position)); 

            return player_value;
        }

        void read_player(JSON_Object *player) {
            if (json_object_get_value(player, "health") != nullptr) {
                _player.health = json_object_get_number(player, "health");
            }
            if (json_object_get_value(player, "ammo") != nullptr) {
                _player.ammo = json_object_get_number(player, "ammo");
            }
            if (json_object_get_value(player, "flags") != nullptr) {
                _player.flags = (object_flags_t) json_object_get_number(player, "flags");
            }
            if (json_object_get_value(player, "direction") != nullptr) {
                _player.direction = (dir_t) json_object_get_number(player, "direction");
            }
            if (json_object_get_value(player, "position") != nullptr) {
                const JSON_Object *vector = json_object_get_object(player, "position");
                _player.position = read_vec3(vector);
            }
        }

        void save_data() {
            JSON_Value *root_value = json_value_init_object();
            JSON_Object *root_object = json_value_get_object(root_value);

            json_object_set_value(root_object, "portal", save_portal());
            json_object_set_value(root_object, "player", save_player());

            char *serialized = json_serialize_to_string_pretty(root_value);
            const size_t size = strnlen(serialized, 4096);
            save_file(SAVE_PATH, serialized, size).log_failure();

            json_free_serialized_string(serialized);
            json_value_free(root_value);
        }

        void read_data() {
            char path[1024]; 
            maybeunit_t path_found = find_path(SAVE_PATH, ".", path, 1024);
            if (path_found.failed()) {
                path_found.log_failure();
                return;
            }

            /* TODO: check if file exists */

            JSON_Value *root_value = json_parse_file(path);
            if (json_value_get_type(root_value) != JSONObject) {
                warp_log_e("Failed to load JSON file: %s.", path);
                return; 
            }
            const JSON_Object *root = json_value_get_object(root_value);
            
            read_portal(json_object_get_object(root, "portal"));
            read_player(json_object_get_object(root, "player"));

            json_value_free(root_value);
        }
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
