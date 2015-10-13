#include "core.h"

#include <stdlib.h>
#include <time.h>
#include <vector>

#include "warp/world.h"
#include "warp/entity.h"
#include "warp/components.h"
#include "warp/direction.h"
#include "warp/entity-helpers.h"

#include "random.h"
#include "level.h"
#include "character.h"

using namespace warp;

struct object_t {
    object_type_t type;
    entity_t *entity;

    vec3_t position;
    dir_t direction;

    int health;
    bool hold;
    bool attacked;
};

enum feat_state_t : int {
    FSTATE_INACTIVE = 0,
    FSTATE_ACTIVE = 1,
};

struct feature_t {
    feature_type_t type;
    entity_t *entity;

    vec3_t position;
    size_t target_id;
    feat_state_t state;
};

static dir_t vec3_to_dir(const vec3_t v) {
    const vec3_t absolutes = vec3(fabs(v.x), fabs(v.y), fabs(v.z));
    if (absolutes.x > absolutes.y) {
        if (absolutes.x > absolutes.z) {
            return signum(v.x) > 0 ? DIR_X_PLUS : DIR_X_MINUS;
        } else {
            return signum(v.z) > 0 ? DIR_Z_PLUS : DIR_Z_MINUS;
        }
    } else {
        if (absolutes.y > absolutes.z) {
            return signum(v.y) > 0 ? DIR_Y_PLUS : DIR_Y_MINUS;
        } else {
            return signum(v.z) > 0 ? DIR_Z_PLUS : DIR_Z_MINUS;
        }
    }
}

static void shuffle(dir_t *array, size_t n) {
    for (size_t i = 0; i < n - 1; i++) {
        size_t j = i + rand() / (RAND_MAX / (n - i) + 1);
        dir_t tmp = array[j];
        array[j] = array[i];
        array[i] = tmp;
    }
}

static maybe_t<entity_t *> create_button_entity
        (vec3_t position, world_t *world) {
    maybe_t<graphics_comp_t *> graphics
        = create_single_model_graphics(world, "button.obj", "missing.png");

    return world->create_entity(position, VALUE(graphics), nullptr, nullptr);
}

static maybe_t<entity_t *> create_door_entity
        (vec3_t position, world_t *world) {
    maybe_t<graphics_comp_t *> graphics
        = create_single_model_graphics(world, "door.obj", "missing.png");

    return world->create_entity(position, VALUE(graphics), nullptr, nullptr);
}

static maybe_t<entity_t *> create_character_entity
        (vec3_t position, world_t *world, bool confirm_moves) {
    maybe_t<graphics_comp_t *> graphics
        = create_single_model_graphics(world, "npc.obj", "missing.png");
    maybe_t<controller_comp_t *> controller
        = create_character_controller(world, confirm_moves);

    return world->create_entity(position, VALUE(graphics), nullptr, VALUE(controller));
}

static maybe_t<entity_t *> create_boulder_entity
        (vec3_t position, world_t *world) {
    maybe_t<graphics_comp_t *> graphics
        = create_single_model_graphics(world, "boulder.obj", "missing.png");
    maybe_t<controller_comp_t *> controller
        = create_character_controller(world, false);

    return world->create_entity(position, VALUE(graphics), nullptr, VALUE(controller));
}

static dir_t get_move_direction(move_dir_t move_dir) {
    switch (move_dir) {
        case MOVE_UP:
            return DIR_Z_MINUS;
        case MOVE_DOWN:
            return DIR_Z_PLUS;
        case MOVE_LEFT:
            return DIR_X_MINUS;
        case MOVE_RIGHT:
            return DIR_X_PLUS;
    }
}

static vec3_t move_character(const object_t &character, move_dir_t dir) {
    const dir_t direction = get_move_direction(dir);
    const vec3_t step = dir_to_vec3(direction);
    return vec3_add(character.position, step);
}

static int calculate_damage(const object_t &target) {
    return target.type == OBJ_BOULDER ? 0 : 1;
}

static bool is_idle(const object_t &character) {
    const dynval_t is_idle = character.entity->get_property("avat.is_idle");
    return VALUE(is_idle.get_int()) == 1;
}

static feature_t *create_feature
        (world_t *world, feature_type_t type, size_t target, size_t x, size_t z) {
    const vec3_t pos = vec3(x, 0, z);
    const maybe_t<entity_t *> entity 
        = type == FEAT_BUTTON 
        ? create_button_entity(pos, world)
        : create_door_entity(pos, world)
        ;

    feature_t *feat = new (std::nothrow) feature_t;
    feat->type = type;
    feat->position = pos;
    feat->target_id = target;
    feat->entity = VALUE(entity);
    feat->entity->set_tag("feature");
    feat->state = FSTATE_INACTIVE;

    return feat;
}

static object_t *create_object
        (world_t *world, object_type_t type, dir_t dir, size_t x, size_t z) {
    const vec3_t pos = vec3(x, 0, z);
    const maybe_t<entity_t *> entity
        = type == OBJ_CHARACTER
        ? create_character_entity(pos, world, false)
        : create_boulder_entity(pos, world)
        ;

    object_t *object = new (std::nothrow) object_t;
    object->type = type;
    object->position = pos;
    object->direction = dir;
    object->health = 2;

    object->entity = VALUE(entity);
    object->entity->set_tag("object");

    return object;
}

static void spawn_objects
        ( const level_t &level
        , object_t **objs, feature_t **feats
        , world_t *world, random_t *random
        ) {
    const size_t width = level.get_width();
    const size_t height = level.get_height();

    for (size_t i = 0; i < width * height; i++) {
        objs[i] = nullptr;
        feats[i] = nullptr;
    }

    dir_t directions[4] { 
        DIR_X_PLUS, DIR_Z_PLUS, DIR_X_MINUS, DIR_Z_MINUS,
    };

    for (size_t i = 0; i < width; i++) {
        for (size_t j = 0; j < height; j++) {
            maybe_t<const tile_t *> maybe_tile = level.get_tile_at(i, j);
            if (maybe_tile.failed()) continue;
            
            const size_t index = i + width * j;
            const tile_t *tile = VALUE(maybe_tile);
            const object_type_t obj_type = tile->spawned_object;
            const feature_type_t feat_type = tile->feature;

            if (obj_type != OBJ_NONE || tile->spawn_probablity > 0) {
                const float r = random->uniform_zero_to_one();
                if (r <= tile->spawn_probablity) {
                    const dir_t dir = directions[random->uniform_from_range(0, 3)];
                    objs[index] = create_object(world, obj_type, dir, i, j);
                }
            }

            if (feat_type != FEAT_NONE) {
                const size_t target = tile->feat_target_id;
                feats[index] = create_feature(world, feat_type, target, i, j);
            }
        }
    }
}

class core_controller_t final : public controller_impl_i {
    public:
        core_controller_t(level_t *initial_level)
            : _level(initial_level)
            , _objects(nullptr)
        {
        }

        ~core_controller_t() {
            for (size_t i = 0; i < _tiles_count; i++) {
                delete _objects[i];
                delete _features[i];
            }
            delete _objects;
            delete _features;
        }

        dynval_t get_property(const tag_t &) const override {
            return dynval_t::make_null();
        }

        void initialize(entity_t *owner, world_t *world) override {
            _owner = owner;
            _world = world;

            maybe_t<entity_t *> avatar 
                = create_character_entity(vec3(8, 0, 8), world, true);
            _player.entity = VALUE(avatar);
            _player.position = vec3(8, 0, 8);
            _player.direction = DIR_Z_MINUS;
            _player.health = 2;

            const size_t width  = _level->get_width();
            const size_t height = _level->get_height();
            _tiles_count = width * height;
            _objects  = new (std::nothrow) object_t  * [_tiles_count];
            _features = new (std::nothrow) feature_t * [_tiles_count];

            spawn_objects(*_level, _objects, _features, world, &_random);
        }

        void update(float, const input_t &) override { }

        bool accepts(messagetype_t type) const override {
            return type == CORE_TRY_MOVE
                || type == CORE_MOVE_DONE
                ;
        }

        void handle_message(const message_t &message) override {
            const messagetype_t type = message.type;
            if (type == CORE_MOVE_DONE) {
                next_turn();
                const size_t x = round(_player.position.x);
                const size_t z = round(_player.position.z);
                _level->get_tile_at(x, z).with_value([this](const tile_t *tile) {
                    if (tile->is_stairs) this->next_level();
                });
            } else if (is_idle(_player)) {
                if (type == CORE_TRY_MOVE) {
                    const int direction = VALUE(message.data.get_int());
                    handle_move((move_dir_t)direction);
                }
            }
        }

    private:
        entity_t *_owner;
        world_t *_world;

        level_t *_level;

        object_t _player;
        object_t  **_objects;
        feature_t **_features;
        size_t _tiles_count;

        random_t _random;

        void next_level() {
            maybe_t<entity_t *> level = _world->find_entity("level");
            if (level.failed()) return;
            _world->destroy_later(VALUE(level));

            std::vector<entity_t *> buffer;
            _world->find_all_entities("object", &buffer);
            for (entity_t *npc : buffer) {
                _world->destroy_later(npc);
            }
            _world->find_all_entities("feature", &buffer);
            for (entity_t *npc : buffer) {
                _world->destroy_later(npc);
            }

            delete _level;
            _level = generate_random_level(&_random);
            _level->initialize(_world);

            for (size_t i = 0; i < _tiles_count; i++) {
                delete _objects[i];
                delete _features[i];
            }

            spawn_objects(*_level, _objects, _features, _world, &_random);
        }

        bool can_move_to(vec3_t new_position, vec3_t old_position) {
            const size_t x = round(new_position.x);
            const size_t z = round(new_position.z);

            maybe_t<const tile_t *> maybe_tile = _level->get_tile_at(x, z);
            if (maybe_tile.failed()) return false;

            const tile_t *tile = VALUE(maybe_tile);
            if (tile->is_stairs) {
                return old_position.x > new_position.x;
            }
            if (tile->is_walkable == false) return false;
            
            const object_t *object = object_at(x, z);
            if (object != nullptr) return false;

            const feature_t *feature = feature_at(x, z);
            if (feature != nullptr) {
                if (feature->type == FEAT_DOOR 
                        && feature->state == FSTATE_INACTIVE) {
                    return false;
                }
            }

            return vec3_eps_compare(new_position, _player.position, 0.1f) == false;
        }

        object_t *npc_at_position(vec3_t position) {
            const size_t x = round(position.x);
            const size_t z = round(position.z);
            return object_at(x, z);
        }

        object_t *object_at(size_t x, size_t y) {
            const size_t width  = _level->get_width();
            const size_t height = _level->get_height();
            if (x >= width || y >= height)
                return nullptr;

            return _objects[x + width * y];
        }

        feature_t *feature_at(size_t x, size_t y) {
            const size_t width  = _level->get_width();
            const size_t height = _level->get_height();
            if (x >= width || y >= height)
                return nullptr;

            return _features[x + width * y];
        }

        void handle_move(move_dir_t direction) {
            if (_player.hold) {
                _player.hold = false;
                next_turn();
                return;
            }

            _player.direction = get_move_direction(direction);
            const vec3_t position = move_character(_player, direction);
            if (can_move_to(position, _player.position)) {
                make_move(&_player, position);
            } else {
                object_t *npc = npc_at_position(position);
                if (npc != nullptr) {
                    handle_attack(npc, &_player);
                } else {
                    _player.entity->receive_message(CORE_DO_BOUNCE, position);
                }
            }
        }

        void handle_attack
                (object_t *target, object_t *attacker) {
            const int damage = calculate_damage(*target);
            const bool alive = hurt_character(target, *attacker, damage);
            const vec3_t original_position = target->position;
            if (alive) {
                const vec3_t d = vec3_sub(target->position, attacker->position);
                const vec3_t push_back = vec3_add(target->position, d);
                if (can_move_to(push_back, target->position)) {
                    make_move(target, push_back);
                    target->attacked = true;
                }
            }
            if (target->type == OBJ_BOULDER) {
                if (can_move_to(original_position, attacker->position))
                    make_move(attacker, original_position);
                else
                    attacker->entity->receive_message(CORE_DO_BOUNCE, original_position);
            } else {
                attacker->entity->receive_message(CORE_DO_ATTACK, original_position);
            }
        }

        bool hurt_character
                (object_t *target, const object_t &attacker, int damage) {
            if (target == nullptr || damage < 0) return false;
            const int health_left = target->health - damage;
            if (health_left <= 0) {
                const size_t x = round(target->position.x);
                const size_t z = round(target->position.z);
                const size_t level_width = _level->get_width();

                _objects[x + level_width * z] = nullptr;
                target->entity->receive_message(CORE_DO_DIE, attacker.position);

                if (target != &_player) delete target;
            } else {
                target->health = health_left;
                if (target->type != OBJ_BOULDER) {
                    target->entity->receive_message(CORE_DO_HURT, attacker.position);
                }
            }

            return health_left > 0;
        }

        void next_turn() {
            const size_t width  = _level->get_width();
            const size_t height = _level->get_height();
            const size_t max_characters = width * height;
            
            size_t count = 0;
            object_t **buffer = new object_t * [max_characters];

            for (size_t x = 0; x < width; x++) {
                for (size_t y = 0; y < height; y++) {
                    object_t *npc = object_at(x, y);
                    if (npc != nullptr && is_idle(*npc)) {
                        buffer[count] = npc;
                        count++;
                    }
                }
            }

            for (size_t i = 0; i < count; i++) {
                update_npc(buffer[i]);
            }

            delete buffer;
        }

        void make_move(object_t *target, vec3_t position) {
            const vec3_t old_position = target->position;
            const size_t level_width = _level->get_width();
            const size_t old_x = round(old_position.x);
            const size_t old_z = round(old_position.z);
            const size_t new_x = round(position.x);
            const size_t new_z = round(position.z);

            if (target != &_player) {
                _objects[old_x + level_width * old_z] = nullptr;
                _objects[new_x + level_width * new_z] = target;
            }

            feature_t *old_feat = feature_at(old_x, old_z);
            if (old_feat != nullptr) {
                if (old_feat->type == FEAT_BUTTON) {
                    old_feat->state = FSTATE_INACTIVE;
                    feature_t *target = _features[old_feat->target_id];
                    if (target != nullptr)
                        target->state = FSTATE_INACTIVE;
                }
            }

            feature_t *new_feat = feature_at(new_x, new_z);
            if (new_feat != nullptr) {
                if (new_feat->type == FEAT_BUTTON) {
                    new_feat->state = FSTATE_ACTIVE;
                    feature_t *target = _features[new_feat->target_id];
                    if (target != nullptr)
                        target->state = FSTATE_ACTIVE;
                }
            }

            target->position = position;
            target->entity->receive_message(CORE_DO_MOVE, position);
        }

        dir_t pick_next_direction(const object_t &npc) {
            const vec3_t diff = vec3_sub(_player.position, npc.position);
            const dir_t dir = vec3_to_dir(diff);
            const vec3_t position = vec3_add(npc.position, dir_to_vec3(dir));
            if (can_move_to(position, npc.position)) {
                return dir;
            } else {
                dir_t directions[4] { 
                    DIR_X_PLUS, DIR_Z_PLUS, DIR_X_MINUS, DIR_Z_MINUS,
                };
                shuffle(directions, 4);

                const vec3_t position = npc.position;
                for (size_t i = 0; i < 4; i++) {
                    const dir_t dir = directions[i];
                    const vec3_t target = vec3_add(position, dir_to_vec3(dir));
                    if (can_move_to(target, position))
                        return dir;
                }
            }

            return npc.direction;
        }

        bool can_attack_player(const object_t npc) {
            const float dx = fabs(npc.position.x - _player.position.x);
            const float dz = fabs(npc.position.z - _player.position.z);
            if (dx > 1.1f || dz > 1.1f) return false;
            
            const bool close_x = epsilon_compare(dx, 1, 0.05f);
            const bool close_z = epsilon_compare(dz, 1, 0.05f);

            return close_x != close_z; /* xor */
        }

        void update_npc(object_t *npc) {
            if (npc->type != OBJ_CHARACTER) {
                return;
            }
            if (npc->hold) {
                npc->hold = false;
                return;
            }
            if (npc->attacked) {
                npc->attacked = false;
                npc->direction = opposite_dir(npc->direction);
            }

            if (can_attack_player(*npc)) {
                handle_attack(&_player, npc);
            } else {
                vec3_t position = vec3_add(npc->position, dir_to_vec3(npc->direction));
                const bool change_dir = _random.uniform_zero_to_one() > 0.6f;
                if (change_dir || can_move_to(position, npc->position) == false) {
                    npc->direction = pick_next_direction(*npc);
                    position = vec3_add(npc->position, dir_to_vec3(npc->direction));
                }
                make_move(npc, position);
            }
        }
};

extern maybe_t<entity_t *> create_core(world_t *world, level_t * level) {
    if (level == nullptr) {
        return nothing<entity_t *>("Level is null.");
    }

    controller_comp_t *controller = world->create_controller();
    controller->initialize(new core_controller_t(level));

    entity_t *entity = world->create_entity(vec3(0, 0, 0), nullptr, nullptr, controller);

    return entity;
}
