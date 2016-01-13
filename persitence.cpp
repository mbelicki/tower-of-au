#include "persitence.h"

#include "warp/world.h"
#include "warp/entity.h"
#include "warp/components.h"
#include "warp/utils/io.h"

#include "libs/parson/parson.h"

#include "region.h"
#include "level_state.h"
#include "version.h"

using namespace warp;

static const char *SAVE_FILENAME = "save.json";
static const uint32_t DEFAULT_SEED = 314;

static bool get_save_path(warp_str_t *out_path) {
    bool result = true;
    const char *dir_path = NULL;
    warp_str_t directory; 

    warp_result_t get_result = get_writable_directory(&directory);
    if (WARP_FAILED(get_result)) {
        warp_result_log("Cannot get directory for game save", &get_result);
        warp_result_destory(&get_result);
        goto cleanup;
    }

    dir_path = warp_str_value(&directory);
    *out_path = warp_str_format("%s/%s-%s", dir_path, PROJECT_NAME, SAVE_FILENAME);

cleanup:
    warp_str_destroy(&directory);
    
    return result;
}

class persistence_controller_t final : public controller_impl_i {
    public:
        persistence_controller_t()
                : _owner(nullptr)
                , _world(nullptr) {
        }

        ~persistence_controller_t() {
            warp_str_destroy(&_portal.region_name);
        }

        dynval_t get_property(const tag_t &name) const override {
            if (name == "portal") {
                return (void *)&_portal;
            } else if (name == "player") {
                return (void *)&_player;
            } else if (name == "seed") {
                return *(int *)&_seed;
            }

            return dynval_t::make_null();
        }

        void set_defaults() {
            /* default portal */
            _portal.region_name = warp_str_create("overworld.json");
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
            _player.max_health = 0;
            _player.ammo = 0;

            /* default seed */
            _seed = DEFAULT_SEED;
        }

        void initialize(entity_t *owner, world_t *world) override {
            _owner = owner;
            _world = world;

            set_defaults();

            read_data();
        }

        void update(float, const input_t &) override { }

        bool accepts(messagetype_t type) const override {
            return type == CORE_SAVE_PORTAL 
                || type == CORE_SAVE_PLAYER
                || type == CORE_SAVE_SEED
                || type == CORE_SAVE_RESET_DEFAULTS
                || type == CORE_SAVE_TO_FILE
                ;
        }

        void handle_message(const message_t &message) override { 
            const messagetype_t type = message.type;
            if (type == CORE_SAVE_PLAYER) {
                maybe_t<void *> maybe_value = message.data.get_pointer();
                if (maybe_value.failed()) {
                    maybe_value.log_failure("Expected CORE_SAVE_PLAYER to contain pointer");
                    return;
                }
                const object_t *player = (object_t *) VALUE(maybe_value);
                _player = *player;
                _player.entity = nullptr;
                warp_log_d("player saved.");
            } else if (type == CORE_SAVE_PORTAL) {
                maybe_t<void *> maybe_value = message.data.get_pointer();
                if (maybe_value.failed()) {
                    maybe_value.log_failure("Expected CORE_SAVE_PORTAL to contain pointer");
                    return;
                }
                const portal_t *portal = (portal_t *) VALUE(maybe_value);
                warp_str_destroy(&_portal.region_name);
                _portal = *portal;
                _portal.region_name = warp_str_copy(&portal->region_name);
            } else if (type == CORE_SAVE_SEED) {
                maybe_t<int> maybe_value = message.data.get_int();
                if (maybe_value.failed()) {
                    maybe_value.log_failure("Expected CORE_SAVE_SEED to contain integer");
                    return;
                }
                const int packed_seed = VALUE(maybe_value);
                _seed = *(uint32_t *)&packed_seed;
            } else if (type == CORE_SAVE_RESET_DEFAULTS) {
                set_defaults();
            } else if (type == CORE_SAVE_TO_FILE) {
                save_data();
            }
        }

    private:
        entity_t *_owner;
        world_t *_world;

        portal_t _portal;
        object_t _player;
        uint32_t _seed;

        JSON_Value *save_portal() {
            JSON_Value *portal_value = json_value_init_object();
            JSON_Object *portal = json_value_get_object(portal_value);
            
            json_object_set_string(portal, "region_name", warp_str_value(&_portal.region_name)); 
            json_object_set_number(portal, "tile_x", _portal.tile_x); 
            json_object_set_number(portal, "tile_z", _portal.tile_z); 
            json_object_set_number(portal, "level_x", _portal.level_x); 
            json_object_set_number(portal, "level_z", _portal.level_z); 

            return portal_value;
        }

        void read_portal(JSON_Object *portal) {
            const char *region = json_object_get_string(portal, "region_name");
            if (region != nullptr) {
                warp_str_destroy(&_portal.region_name);
                _portal.region_name = warp_str_create(region);
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
            json_object_set_number(player, "max_health", _player.max_health); 
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
            if (json_object_get_value(player, "max_health") != nullptr) {
                _player.max_health = json_object_get_number(player, "max_health");
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
            _player.type = OBJ_CHARACTER;
        }

        void save_data() {
            JSON_Value *root_value = json_value_init_object();
            JSON_Object *root_object = json_value_get_object(root_value);

            json_object_set_value(root_object, "portal", save_portal());
            json_object_set_value(root_object, "player", save_player());
            json_object_set_number(root_object, "seed", (double)_seed);

            char *serialized = json_serialize_to_string_pretty(root_value);
            const size_t size = strnlen(serialized, 4096);
            
            warp_str_t save_path;
            get_save_path(&save_path);
            const char *path = warp_str_value(&save_path);

            warp_result_t save_result = save_file(path, serialized, size);
            if (WARP_FAILED(save_result)) {
                warp_result_log("Failed to save game", &save_result);
                warp_result_destory(&save_result);
            }

            json_free_serialized_string(serialized);
            json_value_free(root_value);
            warp_str_destroy(&save_path);
        }

        void read_data() {
            warp_str_t save_path;
            get_save_path(&save_path);
            const char *path = warp_str_value(&save_path);

            JSON_Value *root_value = json_parse_file(path);
            if (json_value_get_type(root_value) != JSONObject) {
                warp_log_e("Failed to load JSON file: %s.", path);
                return; 
            }
            const JSON_Object *root = json_value_get_object(root_value);
            
            read_portal(json_object_get_object(root, "portal"));
            read_player(json_object_get_object(root, "player"));
            _seed = json_object_get_number(root, "seed");

            json_value_free(root_value);
            warp_str_destroy(&save_path);
        }
};

extern entity_t *get_persitent_data(world_t *world) {
    entity_t *data = world->find_entity("persistent_data");
    if (data == nullptr) {
        data = create_persitent_data(world);
    }
    return data;
}

extern entity_t *create_persitent_data(world_t *world) {
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
        maybe_value.log_failure("Expected to get pointer from persistence entity");
        return nullptr;
    }
    return VALUE(maybe_value);
}

extern const object_t *get_saved_player_state(world_t *world) {
    return (const object_t *)get_saved_data(world, "player");
}

extern const portal_t *get_saved_portal(world_t *world) {
    return (const portal_t *)get_saved_data(world, "portal");
}

extern uint32_t get_saved_seed(world_t *world) {
    entity_t *data = get_persitent_data(world);
    maybe_t<int> maybe_value = data->get_property("seed").get_int();
    if (maybe_value.failed()) {
        maybe_value.log_failure("Expected to get int");
        return DEFAULT_SEED;
    }
    const int packed_seed = VALUE(maybe_value);
    return *(uint32_t *)&packed_seed;
}

extern void save_player_state(world_t *world, const object_t *player) {
    entity_t *data = get_persitent_data(world);
    data->receive_message(CORE_SAVE_PLAYER, (void *)player);
}

extern void save_portal(world_t *world, const portal_t *portal) {
    entity_t *data = get_persitent_data(world);
    data->receive_message(CORE_SAVE_PORTAL, (void *)portal);
}

extern void save_random_seed(world_t *world, uint32_t seed) {
    entity_t *data = get_persitent_data(world);
    const int packed_seed = *(int*)&seed;
    data->receive_message(CORE_SAVE_SEED, packed_seed);
}
