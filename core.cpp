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
#include "region.h"
#include "level.h"
#include "character.h"
#include "features.h"
#include "bullets.h"
#include "persitence.h"
#include "text-label.h"

using namespace warp;

static const float LEVEL_TRANSITION_TIME = 1.0f;
static const int PLAYER_MAX_HP = 3;
static const int PLAYER_MAX_AMMO = 16;

enum core_state_t {
    CSTATE_LEVEL = 0,
    CSTATE_TRANSITION,
};

enum object_flags_t {
    FOBJ_NONE = 0,
    FOBJ_NPCMOVE_STILL = 1,
    FOBJ_NPCMOVE_VLINE = 2,
    FOBJ_NPCMOVE_HLINE = 4,
    FOBJ_NPCMOVE_ROAM  = 8,
};

struct object_t {
    object_type_t type;
    entity_t *entity;

    vec3_t position;
    dir_t direction;

    object_flags_t flags;

    int health;
    int ammo;
    bool hold;
    bool attacked;
    bool can_shoot;
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

static physics_comp_t *create_object_physics(world_t *world, vec2_t size) {
    physics_comp_t *physics = world->create_physics();
    if (physics != nullptr) {
        physics->_flags = PHYSFLAGS_FIXED;
        physics->_velocity = vec2(0, 0);
        physics->_bounds = rectangle_t(vec2(0, 0), size);
    }

    return physics;
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
    maybe_t<controller_comp_t *> controller = create_door_controller(world);
    physics_comp_t *physics = create_object_physics(world, vec2(1.0f, 0.4f));

    return world->create_entity(position, VALUE(graphics), physics, VALUE(controller));
}

static maybe_t<entity_t *> create_character_entity
        (vec3_t position, world_t *world, bool confirm_moves) {
    maybe_t<graphics_comp_t *> graphics
        = create_single_model_graphics(world, "npc.obj", "missing.png");
    maybe_t<controller_comp_t *> controller
        = create_character_controller(world, confirm_moves);
    physics_comp_t *physics = create_object_physics(world, vec2(0.4f, 0.4f));

    return world->create_entity(position, VALUE(graphics), physics, VALUE(controller));
}

static maybe_t<entity_t *> create_boulder_entity
        (vec3_t position, world_t *world) {
    maybe_t<graphics_comp_t *> graphics
        = create_single_model_graphics(world, "boulder.obj", "missing.png");
    maybe_t<controller_comp_t *> controller
        = create_character_controller(world, false);
    physics_comp_t *physics = create_object_physics(world, vec2(0.6f, 0.6f));

    return world->create_entity(position, VALUE(graphics), physics, VALUE(controller));
}

static dir_t get_move_direction(move_dir_t move_dir) {
    switch (move_dir) {
        case MOVE_NONE:
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
    object->ammo = 8;
    object->can_shoot = false;
    object->flags = FOBJ_NONE;

    object->entity = VALUE(entity);
    object->entity->set_tag("object");

    return object;
}

static void initialize_player
        (object_t *player, vec3_t start_position, world_t *world) {
    const maybe_t<entity_t *> avatar 
        = create_character_entity(start_position, world, true);
    player->entity = VALUE(avatar);
    player->entity->set_tag("player");
    player->position = start_position;
    player->direction = DIR_Z_MINUS;
    player->health = PLAYER_MAX_HP;
    player->ammo = PLAYER_MAX_AMMO;
    player->can_shoot = true;
}


static object_flags_t random_movement_flag(random_t *rand) {
    const object_flags_t flags[4]
        = { FOBJ_NPCMOVE_STILL, FOBJ_NPCMOVE_VLINE
          , FOBJ_NPCMOVE_HLINE, FOBJ_NPCMOVE_ROAM
          };
    return flags[rand->uniform_from_range(0, 3)];
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

            if (obj_type != OBJ_NONE && tile->spawn_probablity > 0) {
                const float r = random->uniform_zero_to_one();
                if (r <= tile->spawn_probablity) {
                    const dir_t dir = directions[random->uniform_from_range(0, 3)];
                    objs[index] = create_object(world, obj_type, dir, i, j);
                    objs[index]->can_shoot = random->boolean(0.2f);
                    objs[index]->flags = random_movement_flag(random);
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
        core_controller_t(const portal_t *start)
                : _owner(nullptr)
                , _world(nullptr)
                , _portal(*start)
                , _level(nullptr)
                , _level_x(0), _level_z(0)
                , _bullets(nullptr)
                , _font(nullptr)
                , _player()
                , _objects(nullptr)
                , _features(nullptr)
                , _tiles_count(0)
                , _random()
                , _state(CSTATE_LEVEL)
                , _transition_timer(0) {
            const size_t name_size = strnlen(start->region_name, 256);
            char *name_buffer = new char[name_size];
            strncpy(name_buffer, start->region_name, name_size + 1);
            _portal.region_name = name_buffer;
        }

        ~core_controller_t() {
            for (size_t i = 0; i < _tiles_count; i++) {
                delete _objects[i];
                delete _features[i];
            }
            delete _objects;
            delete _features;

            delete _bullets;

            delete [] _portal.region_name;
        }

        dynval_t get_property(const tag_t &) const override {
            return dynval_t::make_null();
        }

        void initialize(entity_t *owner, world_t *world) override {
            _owner = owner;
            _world = world;

            _bullets = new bullet_factory_t(world);
            _bullets->initialize();

            _level_x = _portal.level_x;
            _level_z = _portal.level_z;

            _region = VALUE(load_region(_portal.region_name));
            _region->initialize(_world);
            _region->change_display_positions(_level_x, _level_z);

            _level = VALUE(_region->get_level_at(_level_x, _level_z));
            
            get_default_font(world->get_resources())
                    .with_value([this](font_t *font) {
                _font = font;
            });

            const size_t width  = _level->get_width();
            const size_t height = _level->get_height();
            _tiles_count = width * height;
            _objects  = new (std::nothrow) object_t  * [_tiles_count];
            _features = new (std::nothrow) feature_t * [_tiles_count];

            spawn_objects(*_level, _objects, _features, world, &_random);

            initialize_player(&_player, vec3(_portal.tile_x, 0, _portal.tile_z), _world);
            update_player_health_display();
            update_player_ammo_display();
        }

        void update(float dt, const input_t &) override { 
            if (_state == CSTATE_TRANSITION) {
                _transition_timer -= dt;
                if (_transition_timer <= 0) {
                    _transition_timer = 0;
                    _state = CSTATE_LEVEL;
                }

                const float k = 1 - _transition_timer / LEVEL_TRANSITION_TIME;
                const float t = ease_cubic(k);
                _region->animate_transition
                    (_level_x, _level_z, _previous_level_x, _previous_level_z, t);

                const int dx = (int)_level_x - (int)_previous_level_x;
                const int dz = (int)_level_z - (int)_previous_level_z;
                
                vec3_t player_pos = _player.position;
                player_pos.x += dx * (1 - t) * 13;
                player_pos.z += dz * (1 - t) * 11;
                _player.entity->receive_message(CORE_DO_MOVE_IMMEDIATE, player_pos);

                if (_state == CSTATE_LEVEL) {
                    _region->change_display_positions(_level_x, _level_z);
                    spawn_objects(*_level, _objects, _features, _world, &_random);
                    make_move(&_player, _player.position, true);
                }
            }
        }

        bool accepts(messagetype_t type) const override {
            return type == CORE_TRY_MOVE
                || type == CORE_TRY_SHOOT
                || type == CORE_MOVE_DONE
                || type == CORE_BULLET_HIT
                ;
        }

        void handle_message(const message_t &message) override {
            if (_state == CSTATE_TRANSITION) return;

            const messagetype_t type = message.type;
            if (type == CORE_BULLET_HIT) {
                const vec3_t target_pos = VALUE(message.data.get_vec3());
                object_t *object = npc_at_position(target_pos);
                if (vec3_eps_compare(target_pos, _player.position, 0.05f)) {
                    object = &_player;
                }
                handle_attack(object, nullptr);
            } else if (type == CORE_MOVE_DONE) {
                next_turn();
                const int x = round(_player.position.x);
                const int z = round(_player.position.z);
                if (x < 0) {
                    start_level_change(_level_x - 1, _level_z);
                } else if (x >= 13) {
                    start_level_change(_level_x + 1, _level_z);
                } else if (z < 0) {
                    start_level_change(_level_x, _level_z - 1);
                } else if (z >= 11) {
                    start_level_change(_level_x, _level_z + 1);
                } else {
                    _level->get_tile_at(x, z)
                            .with_value([this](const tile_t *tile) {
                        if (tile->is_stairs) { 
                            maybe_t<const portal_t *> portal
                                = _region->get_portal(tile->portal_id);
                            if (portal.has_value()) {
                                change_region(VALUE(portal));
                            }
                        }
                    });
                }
            } else if (is_idle(_player)) {
                if (type == CORE_TRY_MOVE) {
                    const int direction = VALUE(message.data.get_int());
                    handle_move((move_dir_t)direction);
                } else if (type == CORE_TRY_SHOOT) {
                    const move_dir_t direction
                        = (move_dir_t) VALUE(message.data.get_int());
                    handle_shooting(&_player, get_move_direction(direction));
                }
            }
        }

    private:
        entity_t *_owner;
        world_t *_world;

        portal_t _portal;
        region_t *_region;
        level_t *_level;
        size_t _level_x;
        size_t _level_z;
        size_t _previous_level_x;
        size_t _previous_level_z;

        bullet_factory_t *_bullets;
        font_t *_font;

        object_t _player;
        object_t  **_objects;
        feature_t **_features;
        size_t _tiles_count;

        random_t _random;

        core_state_t _state;
        float _transition_timer;

        void update_player_health_display() {
            entity_t *hp_label = _world->find_entity("health_label");
            char buffer[PLAYER_MAX_HP + 1];
            for (size_t i = 0; i < PLAYER_MAX_HP; i++) {
                buffer[i] = (int)i < _player.health ? '#' : '$';
            }
            buffer[PLAYER_MAX_HP] = '\0';
            hp_label->receive_message(CORE_SHOW_TAG_TEXT, tag_t(buffer));
        }

        void update_player_ammo_display() {
            entity_t *hp_label = _world->find_entity("ammo_label");
            char buffer[16];
            snprintf(buffer, 16, "*%2d", _player.ammo);
            hp_label->receive_message(CORE_SHOW_TAG_TEXT, tag_t(buffer));
        }

        void remove_objects_and_feateures() {
            std::vector<entity_t *> buffer;
            _world->find_all_entities("object", &buffer);
            for (entity_t *npc : buffer) {
                _world->destroy_later(npc);
            }
            _world->find_all_entities("feature", &buffer);
            for (entity_t *feature : buffer) {
                _world->destroy_later(feature);
            }
            _world->find_all_entities("bullet", &buffer);
            for (entity_t *bullet : buffer) {
                _world->destroy_later(bullet);
            }
            
            for (size_t i = 0; i < _tiles_count; i++) {
                delete _objects[i];
                delete _features[i];
            }
        }

        void change_region(const portal_t *portal) {
            if (portal == nullptr) return;
            
            create_region_token(_world, portal);
            _world->request_state_change("level", 1.0f);
        }
        
        void start_level_change(size_t x, size_t z) {
            if (_region->get_level_at(x, z).failed()) return;

            _state = CSTATE_TRANSITION;
            _transition_timer = LEVEL_TRANSITION_TIME;

            _previous_level_x = _level_x;
            _previous_level_z = _level_z;
            change_level(x, z);
        }

        void change_level(size_t x, size_t z) {
            const maybe_t<level_t *> maybe_level = _region->get_level_at(x, z);
            if (maybe_level.failed()) return;

            _level = VALUE(maybe_level);
            
            remove_objects_and_feateures();

            const int dx = x - _level_x;
            const int dz = z - _level_z;
            _level_x = x;
            _level_z = z;

            vec3_t player_pos = _player.position;
            player_pos.x -= dx * 13;
            player_pos.z -= dz * 11;
            make_move(&_player, player_pos, true);
        }

        bool can_move_to(vec3_t new_position, vec3_t old_position) {
            const size_t x = round(new_position.x);
            const size_t z = round(new_position.z);

            maybe_t<const tile_t *> maybe_tile = _level->get_tile_at(x, z);
            if (maybe_tile.failed()) return true;

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

        void handle_shooting(object_t *shooter, dir_t dir) {
            if (shooter->can_shoot == false || shooter->ammo <= 0) return;
            const float speed = 4.5f;

            const vec3_t d = dir_to_vec3(dir);
            const vec3_t pos = vec3_add(shooter->position, vec3_scale(d, 0.5f));
            const vec3_t v = vec3_add(vec3_scale(d, speed), vec3(0.001f, 0, 0.001f));
            _bullets->create_bullet(pos, v, BULLET_ARROW, _level);

            const vec3_t recoil = vec3_add(shooter->position, vec3_scale(d, -1));
            shooter->entity->receive_message(CORE_DO_BOUNCE, recoil);
            shooter->ammo -= 1;

            if (shooter == &_player) {
                update_player_ammo_display();
            }
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
                make_move(&_player, position, false);
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
            if (target == nullptr) return;

            const vec3_t original_position = target->position;
            const int damage = calculate_damage(*target);
            const bool alive = hurt_character(target, damage);
            if (alive) {
                const vec3_t d
                    = attacker == nullptr 
                    ? vec3(0, 0, 0)
                    : vec3_sub(target->position, attacker->position)
                    ;
                const vec3_t push_back = vec3_add(target->position, d);
                if (can_move_to(push_back, target->position)) {
                    make_move(target, push_back, false);
                    target->attacked = true;
                }
            }
            if (attacker != nullptr) {
                if (target->type == OBJ_BOULDER) {
                    if (can_move_to(original_position, attacker->position))
                        make_move(attacker, original_position, false);
                    else
                        attacker->entity->receive_message(CORE_DO_BOUNCE, original_position);
                } else {
                    attacker->entity->receive_message(CORE_DO_ATTACK, original_position);
                }
            }
        }

        bool hurt_character(object_t *target, int damage) {
            if (target == nullptr || damage < 0) return false;
            const int health_left = target->health - damage;
            target->health = health_left;
            if (health_left <= 0) {
                const size_t x = round(target->position.x);
                const size_t z = round(target->position.z);
                const size_t level_width = _level->get_width();

                _objects[x + level_width * z] = nullptr;
                target->entity->receive_message(CORE_DO_DIE, vec3(0, 0, 0));

                if (target != &_player) { 
                    delete target;
                } else {
                    change_region(&_portal);
                }

                create_speech_bubble
                        ( _world, *_font, vec3_add(target->position, vec3(0, 1, 0))
                        , "ouch!"
                        );
            } else {
                if (target->type != OBJ_BOULDER) {
                    target->entity->receive_message(CORE_DO_HURT, vec3(0, 0, 0));
                }
            }
            
            if (target == &_player) {
                update_player_health_display();
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

        void make_move(object_t *target, vec3_t position, bool immediate) {
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
                    if (target != nullptr) {
                        target->state = FSTATE_INACTIVE;
                        target->entity->receive_message
                            (CORE_FEAT_STATE_CHANGE, target->state);
                    }
                }
            }

            feature_t *new_feat = feature_at(new_x, new_z);
            if (new_feat != nullptr) {
                if (new_feat->type == FEAT_BUTTON) {
                    new_feat->state = FSTATE_ACTIVE;
                    feature_t *target = _features[new_feat->target_id];
                    if (target != nullptr) {
                        target->state = FSTATE_ACTIVE;
                        target->entity->receive_message
                            (CORE_FEAT_STATE_CHANGE, target->state);
                    }
                }
            }

            target->position = position;
            const int msg_type
                = immediate ? CORE_DO_MOVE_IMMEDIATE : CORE_DO_MOVE;
            target->entity->receive_message(msg_type, position);
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

        bool can_attack_player(const object_t &npc) {
            const float dx = fabs(npc.position.x - _player.position.x);
            const float dz = fabs(npc.position.z - _player.position.z);
            if (dx > 1.1f || dz > 1.1f) return false;
            
            const bool close_x = epsilon_compare(dx, 1, 0.05f);
            const bool close_z = epsilon_compare(dz, 1, 0.05f);

            return close_x != close_z; /* xor */
        }

        dir_t can_shoot_player(const object_t &npc) {
            if (npc.can_shoot == false || npc.ammo <= 0) return DIR_NONE;

            const float dx = npc.position.x - _player.position.x;
            const float dz = npc.position.z - _player.position.z;

            const bool zero_x = epsilon_compare(dx, 0, 0.05f);
            const bool zero_z = epsilon_compare(dz, 0, 0.05f);
            if ((zero_x || zero_z) == false) return DIR_NONE;

            const size_t x = round(npc.position.x);
            const size_t z = round(npc.position.z);
            std::function<bool(const tile_t *)> pred = [](const tile_t *t) {
                return t->is_walkable;
            };

            if (zero_x) {
                dir_t result = dz > 0 ? DIR_Z_MINUS : DIR_Z_PLUS;
                const size_t dist = round(fabs(dz) - 1);
                if (_level->scan_if_all(pred, x, z, result, dist))
                    return result;
            } else if (zero_z) {
                dir_t result = dx > 0 ? DIR_X_MINUS : DIR_X_PLUS;
                const size_t dist = round(fabs(dx) - 1);
                if (_level->scan_if_all(pred, x, z, result, dist))
                    return result;
            }
            return DIR_NONE;
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

            dir_t shoot_dir = DIR_NONE;
            if (can_attack_player(*npc)) {
                handle_attack(&_player, npc);
            } else if ((shoot_dir = can_shoot_player(*npc)) != DIR_NONE) {
                handle_shooting(npc, shoot_dir);               
            } else {
                vec3_t position = vec3_add(npc->position, dir_to_vec3(npc->direction));
                const bool change_dir = _random.uniform_zero_to_one() > 0.6f;
                if (change_dir || (can_move_to(position, npc->position) == false)) {
                    npc->direction = pick_next_direction(*npc);
                    position = vec3_add(npc->position, dir_to_vec3(npc->direction));
                }
                make_move(npc, position, false);
            }
        }
};

extern maybe_t<entity_t *> create_core(world_t *world, const portal_t *start) {
    if (start == nullptr) {
        return nothing<entity_t *>("Region is null.");
    }

    controller_comp_t *controller = world->create_controller();
    controller->initialize(new core_controller_t(start));

    entity_t *entity = world->create_entity(vec3(0, 0, 0), nullptr, nullptr, controller);

    return entity;
}
