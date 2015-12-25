#include "level_state.h"

#include "warp/utils/log.h"
#include "warp/world.h"
#include "warp/entity.h"
#include "warp/entity-helpers.h"

#include "character.h"
#include "features.h"
#include "bullets.h"

using namespace warp;

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


static object_flags_t random_movement_flag(warp_random_t *rand) {
    const object_flags_t flags[4]
        = { FOBJ_NPCMOVE_STILL, FOBJ_NPCMOVE_VLINE
          , FOBJ_NPCMOVE_HLINE, FOBJ_NPCMOVE_ROAM
          };
    return flags[warp_random_from_range(rand, 0, 3)];
}


level_state_t::level_state_t(size_t width, size_t height) 
        : _width(width)
        , _height(height)
        , _objects(nullptr)
        , _features(nullptr)
        , _bullets(nullptr) {

    const size_t count = _width * _height;
    _objects  = new object_t  * [count];
    _features = new feature_t * [count];

    for (size_t i = 0; i < count; i++) {
        _objects[i] = nullptr;
        _features[i] = nullptr;
    }
}

level_state_t::~level_state_t() {
    const size_t count = _width * _height;
    for (size_t i = 0; i < count; i++) {
        if (_objects) delete _objects[i];
        if (_features) delete _features[i];
    }
    delete _objects;
    delete _features;
    delete _bullets;
}

bool level_state_t::add_object(const object_t &obj) {
    const size_t x = round(obj.position.x);
    const size_t z = round(obj.position.z);
    if (object_at(x, z) != nullptr) return false;
    
    object_t *new_obj = new object_t;
    *new_obj = obj;

    _objects[x + _width * z] = new_obj;
    return true;
}

const object_t *level_state_t::object_at_position(warp::vec3_t pos) const {
    const size_t x = round(pos.x);
    const size_t z = round(pos.z);
    return object_at(x, z);
}

const object_t *level_state_t::object_at(size_t x, size_t y) const {
    if (x >= _width || y >= _height) return nullptr;
    return _objects[x + _width * y];
}

const object_t *level_state_t::find_player() {
    const size_t count = _width * _height;
    for (size_t i = 0; i < count; i++) {
        const object_t *obj = _objects[i];
        if (obj != nullptr && (obj->flags & FOBJ_PLAYER_AVATAR) != 0) {
            return obj;
        }
    }
    return nullptr;
}

void level_state_t::find_all_characters
        (std::vector<const object_t *> *characters) {
    if (characters == nullptr) {
        warp_log_e("Called with null 'characters' paramter, skipping call.");
        return;
    }

    const size_t count = _width * _height;
    for (size_t i = 0; i < count; i++) {
        object_t *obj = _objects[i];
        if (obj != nullptr && obj->type == OBJ_CHARACTER) {
            characters->push_back(obj);
        }
    }
}

const feature_t *level_state_t::feature_at(size_t x, size_t y) const {
    if (x >= _width || y >= _height) return nullptr;
    return _features[x + _width * y];
}

bool level_state_t::can_move_to(vec3_t new_pos, const level_t *level) const {
    if (level == nullptr) {
        warp_log_e("Cannot spawn objects, null level.");
        return false;
    }

    const size_t x = round(new_pos.x);
    const size_t z = round(new_pos.z);

    maybe_t<const tile_t *> maybe_tile = level->get_tile_at(x, z);
    if (maybe_tile.failed()) return true;

    const tile_t *tile = VALUE(maybe_tile);
    //if (tile->is_stairs) {
    //    return old_pos.x > new_pos.x;
    //}
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

    return true; //vec3_eps_compare(new_pos, _player.position, 0.1f) == false;
}

void level_state_t::respawn
        (world_t *world, const level_t *level, warp_random_t *rand) {
    if (level == nullptr) {
        warp_log_e("Cannot spawn objects, null level.");
        return;
    }
    if (rand == nullptr) {
        warp_log_e("Cannot spawn objects, null random generator.");
        return;
    }

    if (_bullets == nullptr) {
        _bullets = new bullet_factory_t(world);
        _bullets->initialize();
    }

    const dir_t directions[4] { 
        DIR_X_PLUS, DIR_Z_PLUS, DIR_X_MINUS, DIR_Z_MINUS,
    };

    for (size_t i = 0; i < _width; i++) {
        for (size_t j = 0; j < _height; j++) {
            const maybe_t<const tile_t *> maybe_tile
                = level->get_tile_at(i, j);
            if (maybe_tile.failed()) continue;

            const size_t id = i + _width * j;
            const tile_t *tile = VALUE(maybe_tile);
            const object_type_t obj_type = tile->spawned_object;
            const feature_type_t feat_type = tile->feature;

            if (obj_type != OBJ_NONE && tile->spawn_probablity > 0) {
                const float r = warp_random_float(rand);
                if (r <= tile->spawn_probablity) {
                    int dir_index = warp_random_from_range(rand, 0, 3);
                    const dir_t dir = directions[dir_index];
                    _objects[id] = create_object(world, obj_type, dir, i, j);
                    _objects[id]->can_shoot = warp_random_boolean(rand);
                    _objects[id]->flags = random_movement_flag(rand);
                }
            }

            if (feat_type != FEAT_NONE) {
                const size_t target = tile->feat_target_id;
                _features[id] = create_feature(world, feat_type, target, i, j);
            }
        }
    }
}

void level_state_t::update
        (const level_t *level, const std::vector<command_t> &commands) {
    _events.clear();
    for (command_t command : commands) {
        update_object(command.object, command.command, level);
    }
}

void level_state_t::clear(world_t *world) {
    std::vector<entity_t *> buffer;
    world->find_all_entities("object", &buffer);
    for (entity_t *npc : buffer) {
        world->destroy_later(npc);
    }
    world->find_all_entities("feature", &buffer);
    for (entity_t *feature : buffer) {
        world->destroy_later(feature);
    }
    world->find_all_entities("bullet", &buffer);
    for (entity_t *bullet : buffer) {
        world->destroy_later(bullet);
    }

    const size_t count = _width * _height;
    for (size_t i = 0; i < count; i++) {
        delete _objects[i];
        delete _features[i];
        _objects[i]  = NULL;
        _features[i] = NULL;
    }
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

static vec3_t calculate_new_pos(const object_t *character, move_dir_t dir) {
    const dir_t direction = get_move_direction(dir);
    const vec3_t step = dir_to_vec3(direction);
    return vec3_add(character->position, step);
}

void level_state_t::update_object
        (const object_t *obj, const message_t &command, const level_t *level) {
    if (obj == nullptr) {
        warp_log_e("Cannot update null object.");
        return;
    }

    object_t *object = (object_t *)object_at_position(obj->position);
    if (object == nullptr) {
        warp_log_e("Cannot update object, object is not there.");
        return;
    }
    
    const messagetype_t type = command.type;
    if (type == CORE_TRY_MOVE) {
        move_dir_t dir = (move_dir_t)VALUE(command.data.get_int());
        const vec3_t pos = calculate_new_pos(object, dir);
        handle_move(object, pos, level);
    } else if (type == CORE_TRY_SHOOT) {
        const move_dir_t direction = (move_dir_t) VALUE(command.data.get_int());
        handle_shooting(object, get_move_direction(direction), level);
    } else {
        warp_log_e("Unsupported message type: %d.", (int)type);
    }
}

void level_state_t::handle_move
        (object_t *target, warp::vec3_t pos, const level_t *level) {
    if (target == nullptr) { 
        warp_log_e("Cannot handle move, null target.");
        return;
    }

    if (can_move_to(pos, level)) {
        move_object(target, pos, false, level);
    } else {
        object_t *npc = (object_t *)object_at_position(pos);
        if (npc != nullptr) {
            handle_attack(npc, target, level);
        } else {
            target->entity->receive_message(CORE_DO_BOUNCE, pos);
        }
    }
}

static int calculate_damage(const object_t &target) {
    return target.type == OBJ_BOULDER ? 0 : 1;
}

void level_state_t::handle_attack
        (object_t *target, object_t *attacker, const level_t *level) {
    if (target == nullptr) { 
        warp_log_e("Cannot handle attack, null target.");
        return;
    }

    const vec3_t original_position = target->position;
    const vec3_t attacker_position
        = attacker == nullptr ? original_position : attacker->position;
    const vec3_t d = vec3_sub(original_position, attacker_position);
    const vec3_t push_back = vec3_add(target->position, d);
    if (can_move_to(push_back, level)) {
        move_object(target, push_back, false, level);
        target->attacked = true;
    }

    const int damage = calculate_damage(*target);
    hurt_object(target, damage);

    if (attacker != nullptr) {
        if (target->type == OBJ_BOULDER) {
            if (can_move_to(original_position, level)) {
                move_object(attacker, original_position, false, level);
            } else {
                attacker->entity->receive_message(CORE_DO_BOUNCE, original_position);
            }
        } else {
            attacker->entity->receive_message(CORE_DO_ATTACK, original_position);
        }
    }
}

void level_state_t::handle_shooting
        (object_t *shooter, warp::dir_t dir, const level_t *level) {
    if (shooter == nullptr) { 
        warp_log_e("Cannot handle shooting, null shooter.");
        return;
    }
    if (shooter->can_shoot == false || shooter->ammo <= 0) { 
        return;
    }

    const float speed = 4.5f;

    const vec3_t d = dir_to_vec3(dir);
    const vec3_t pos = vec3_add(shooter->position, vec3_scale(d, 0.5f));
    const vec3_t v = vec3_add(vec3_scale(d, speed), vec3(0.001f, 0, 0.001f));
    _bullets->create_bullet(pos, v, BULLET_ARROW, level);

    const vec3_t recoil = vec3_add(shooter->position, vec3_scale(d, -1));
    shooter->entity->receive_message(CORE_DO_BOUNCE, recoil);
    shooter->ammo -= 1;
}

void level_state_t::move_object
        (object_t *target, warp::vec3_t pos, bool immediate, const level_t *level) {
    if (target == nullptr) {
        warp_log_e("Cannot handle move, null target.");
        return;
    }

    const vec3_t old_position = target->position;
    const size_t old_x = round(old_position.x);
    const size_t old_z = round(old_position.z);
    const size_t new_x = round(pos.x);
    const size_t new_z = round(pos.z);

    _objects[old_x + _width * old_z] = nullptr;
    _objects[new_x + _width * new_z] = target;

    feature_t *old_feat = (feature_t *)feature_at(old_x, old_z);
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

    feature_t *new_feat = (feature_t *)feature_at(new_x, new_z);
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

    target->position = pos;
    const int msg_type = immediate ? CORE_DO_MOVE_IMMEDIATE : CORE_DO_MOVE;
    target->entity->receive_message(msg_type, pos);

    if ((target->flags & FOBJ_PLAYER_AVATAR) != 0) {
        const int x = round(pos.x);
        const int z = round(pos.z);
        if (x < 0 || x >= 13 || z < 0 || z >= 11) {
            event_t event = {*target, EVENT_PLAYER_LEAVE};
            _events.push_back(event);
        } else {
            level->get_tile_at(x, z).with_value([this, target](const tile_t *tile) {
                if (tile->is_stairs) { 
                    event_t event = {*target, EVENT_PLAYER_ENTER_PORTAL};
                    _events.push_back(event);
                }
            });
        }
    }
}

bool level_state_t::hurt_object(object_t *target, int damage) {
    if (target == nullptr || damage < 0) { 
        warp_log_e("Cannot hurt object, target is null.");
        return false;
    }
    if (damage <= 0) { 
        return true;
    }

    const int health_left = target->health - damage;
    target->health = health_left;
    if (health_left <= 0) {
        const size_t x = round(target->position.x);
        const size_t z = round(target->position.z);

        _objects[x + _width * z] = nullptr;
        target->entity->receive_message(CORE_DO_DIE, vec3(0, 0, 0));

        event_t event = {*target, EVENT_OBJECT_KILLED};
        _events.push_back(event);

        delete target;
    } else {
        if (target->type != OBJ_BOULDER) {
            target->entity->receive_message(CORE_DO_HURT, vec3(0, 0, 0));
        }
        event_t event = {*target, EVENT_OBJECT_HURT};
        _events.push_back(event);
    }

    return health_left > 0;
}

