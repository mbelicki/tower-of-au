#define WARP_DROP_PREFIX
#include "level_state.h"

#include "warp/utils/log.h"
#include "warp/world.h"
#include "warp/entity.h"
#include "warp/entity-helpers.h"

#include "character.h"
#include "features.h"
#include "bullets.h"
#include "object_factory.h"

using namespace warp;

static physics_comp_t *create_object_physics(world_t *world, vec2_t size) {
    physics_comp_t *physics = world->create_physics();
    if (physics != nullptr) {
        physics->_flags = PHYSFLAGS_FIXED;
        physics->_velocity = vec3(0, 0, 0);
        physics->_bounds = aa_box_t(vec3(0, 0, 0), vec3(size.x, 1, size.y));
    }
    return physics;
}

static entity_t *create_button_entity
        (vec3_t position, world_t *world) {
    graphics_comp_t *graphics
        = create_single_model_graphics(world, "button.obj", "missing.png");
    return world->create_entity(position, graphics, NULL, NULL);
}

static entity_t *create_spikes_entity
        (vec3_t position, world_t *world) {
    graphics_comp_t *graphics
        = create_single_model_graphics(world, "spikes.obj", "atlas.png");
    return world->create_entity(position, graphics, NULL, NULL);
}

static entity_t *create_breakable_entity
        (vec3_t position, world_t *world) {
    graphics_comp_t *graphics
        = create_single_model_graphics(world, "cracked_floor.obj", "atlas.png");
    controller_comp_t *controller = create_door_controller(world);
    return world->create_entity(position, graphics, NULL, controller);
}

static entity_t *create_door_entity
        (vec3_t position, world_t *world) {
    graphics_comp_t *graphics
        = create_single_model_graphics(world, "door.obj", "missing.png");
    controller_comp_t *controller = create_door_controller(world);
    physics_comp_t *physics = create_object_physics(world, vec2(1.0f, 0.4f));

    return world->create_entity(position, graphics, physics, controller);
}

static feature_t *create_feature
        (world_t *world, feature_type_t type, size_t target, size_t x, size_t z) {
    const vec3_t pos = vec3(x, 0, z);
    entity_t *entity = nullptr;
    if (type == FEAT_BUTTON) {
        entity = create_button_entity(pos, world);
    } else if (type == FEAT_DOOR) {
        entity = create_door_entity(pos, world);
    } else if (type == FEAT_SPIKES) {
        entity = create_spikes_entity(pos, world);
    } else if (type == FEAT_BREAKABLE_FLOOR) {
        entity = create_breakable_entity(pos, world);
    } else {
        warp_log_e("Unsupported type of feature: %d", (int)type);
        return nullptr;
    }

    feature_t *feat = new feature_t;
    feat->type = type;
    feat->target_id = target;
    feat->entity = entity;
    feat->entity->set_tag(WARP_TAG("feature"));
    feat->state = type == FEAT_SPIKES ? FSTATE_ACTIVE : FSTATE_INACTIVE;

    return feat;
}

level_state_t::level_state_t(world_t *world, size_t width, size_t height) 
        : _initialized(false)
        , _world(world)
        , _width(width)
        , _height(height)
        , _objects(nullptr)
        , _features(nullptr)
        , _bullet_factory(nullptr) 
        , _object_factory(nullptr) {
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
        delete _objects[i];
        delete _features[i];
    }
    delete _objects;
    delete _features;
    delete _bullet_factory;
    delete _object_factory;
}

bool level_state_t::add_object
        (const object_t &obj, const warp_tag_t &def_name) {
    const size_t x = round(obj.position.x);
    const size_t z = round(obj.position.z);
    if (object_at(x, z) != nullptr) return false;
    
    object_t *new_obj = new object_t;
    *new_obj = obj;
    if (new_obj->entity == nullptr) {
        new_obj->entity
            = _object_factory->create_object_entity(new_obj, def_name, _world);
    }

    _objects[x + _width * z] = new_obj;
    new_obj->entity->receive_message(CORE_DO_ROTATE, (int)new_obj->direction);
    return true;
}

bool level_state_t::spawn_object
        (warp_tag_t name, vec3_t pos, warp_random_t *rand) {
    const size_t x = round(pos.x);
    const size_t z = round(pos.z);
    if (object_at(x, z) != nullptr) return false;

    object_t *new_obj = _object_factory->spawn(name, pos, DIR_Z_PLUS, rand, _world);

    _objects[x + _width * z] = new_obj;
    new_obj->entity->receive_message(CORE_DO_ROTATE, (int)DIR_Z_PLUS);
    return true;
}

void level_state_t::set_object_flag(const object_t *obj, object_flags_t flag) {
    if (obj == nullptr) {
        warp_log_e("Cannot set flag, null object.");
        return;
    }

    object_t *object = (object_t *)object_at_position(obj->position);
    if (object == nullptr) {
        warp_log_e("Cannot set flag, object is not there.");
        return;
    }

    object->flags |= flag;
}

const object_t *level_state_t::object_at_position(vec3_t pos) const {
    const size_t x = round(pos.x);
    const size_t z = round(pos.z);
    return object_at(x, z);
}

const object_t *level_state_t::object_at(size_t x, size_t y) const {
    if (x >= _width || y >= _height) return nullptr;
    return _objects[x + _width * y];
}

const object_t *level_state_t::find_player() const {
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

bool level_state_t::can_move_to(vec3_t new_pos) const {
    const int x = round(new_pos.x);
    const int z = round(new_pos.z);

    const tile_t *tile = _level->get_tile_at(x, z);
    if (tile == NULL) return true;

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
    return true;
}

static void change_direction(object_t *obj, dir_t dir) {
    if (obj->direction != dir && obj->type != OBJ_BOULDER) {
        obj->direction = dir;
        obj->entity->receive_message(CORE_DO_ROTATE, (int)dir);
    }
}

void level_state_t::spawn(const level_t *level, warp_random_t *rand) {
    if (level == nullptr) {
        warp_log_e("Cannot spawn objects, null level.");
        return;
    }
    if (level->get_width() != _width) {
        warp_log_e("Cannot spawn objects, level width does not match level state width.");
        return;
    }
    if (level->get_height() != _height) {
        warp_log_e("Cannot spawn objects, level height does not match level state height.");
        return;
    }
    if (rand == nullptr) {
        warp_log_e("Cannot spawn objects, null random generator.");
        return;
    }
    if (_initialized) {
        warp_log_e("Cannot spawn objects, already spawned.");
        return;
    }
    _initialized = true;
    _level = level;

    if (_bullet_factory == nullptr) {
        _bullet_factory = new bullet_factory_t(_world);
        _bullet_factory->initialize();
    }
    if (_object_factory == nullptr) {
        _object_factory = new object_factory_t();
        _object_factory->load_definitions("objects.json");
        _object_factory->load_resources(&_world->get_resources());
    }

    for (size_t i = 0; i < _width; i++) {
        for (size_t j = 0; j < _height; j++) {
            const tile_t * tile = level->get_tile_at(i, j);
            if (tile == NULL) continue;

            const size_t id = i + _width * j;
            const feature_type_t feat_type = tile->feature;

            if (warp_tag_equals_buffer(&tile->object_id, "") == false 
                    && tile->spawn_probablity > 0) {
                const float r = warp_random_float(rand);
                if (r <= tile->spawn_probablity) {
                    const vec3_t pos = vec3(i, 0, j);
                    object_t *obj = _object_factory->spawn
                        (tile->object_id, pos, DIR_Z_MINUS, rand, _world);
                    if (obj != nullptr) {
                        change_direction(obj, tile->object_dir);
                        _objects[id] = obj;
                    }
                }
            }

            if (feat_type != FEAT_NONE) {
                const size_t target = tile->feat_target_id;
                _features[id] = create_feature(_world, feat_type, target, i, j);
            }
        }
    }
}

void level_state_t::next_turn(const std::vector<command_t> &commands) {
    if (_initialized == false) {
        warp_log_e("Cannot simulate next turn, state not spawned.");
        return;
    }
    _events.clear();
    for (const command_t &command : commands) {
        update_object(command.object, command.command);
    }
}

void level_state_t::process_real_time_event(const rt_event_t &event) {
    if (_initialized == false) {
        warp_log_e("Cannot process real time event, state not spawned.");
        return;
    }

    _events.clear();
    if (event.type == RT_EVENT_BULETT_HIT) {
        const vec3_t target_pos = event.value.get_vec3();
        object_t *object = (object_t *)object_at_position(target_pos);
        handle_attack(object, nullptr);
    }
}

void level_state_t::clear() {
    if (_initialized == false) {
        warp_log_e("Cannot clear, already cleared.");
        return;
    }
    _initialized = false;

    std::vector<entity_t *> buffer;
    _world->find_all_entities(WARP_TAG("object"), &buffer);
    for (entity_t *npc : buffer) {
        _world->destroy_later(npc);
    }
    buffer.clear();
    _world->find_all_entities(WARP_TAG("feature"), &buffer);
    for (entity_t *feature : buffer) {
        _world->destroy_later(feature);
    }
    buffer.clear();
    _world->find_all_entities(WARP_TAG("bullet"), &buffer);
    for (entity_t *bullet : buffer) {
        _world->destroy_later(bullet);
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
        (const object_t *obj, const message_t &command) {
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
        move_dir_t dir = (move_dir_t) command.data.get_int();
        const vec3_t pos = calculate_new_pos(object, dir);
        handle_move(object, pos);
    } else if (type == CORE_TRY_SHOOT) {
        const move_dir_t direction = (move_dir_t) command.data.get_int();
        handle_shooting(object, get_move_direction(direction));
    } else {
        warp_log_e("Unsupported message type: %d.", (int)type);
    }
}

void level_state_t::handle_move(object_t *target, vec3_t pos) {
    if (target == nullptr) { 
        warp_log_e("Cannot handle move, null target.");
        return;
    }

    if (can_move_to(pos)) {
        move_object(target, pos, false);
    } else {
        object_t *npc = (object_t *)object_at_position(pos);
        if (npc != nullptr) {
            if (npc->type == OBJ_TERMINAL) {
                handle_interaction(npc, target);
            } else if (npc->type == OBJ_PICK_UP) {
                handle_picking_up(npc, target);
            } else {
                handle_attack(npc, target);
            }
        } else {
            target->entity->receive_message(CORE_DO_BOUNCE, pos);
        }
    }
}

static int calculate_damage(const object_t &target) {
    return target.type == OBJ_BOULDER ? 0 : 1;
}

void level_state_t::handle_picking_up
        (object_t *pick_up, object_t *character) {
    if (character == nullptr) { 
        warp_log_e("Cannot handle picking up, null character.");
        return;
    }
    if (pick_up == nullptr) { 
        warp_log_e("Cannot handle picking up, null pick up.");
        return;
    }

    character->health += pick_up->health;
    character->ammo += pick_up->ammo;
    character->max_health += pick_up->max_health;

    if (character->health > character->max_health) {
        character->health = character->max_health;
    }

    const vec3_t pick_up_pos = pick_up->position;
    pick_up->entity->receive_message(CORE_DO_DIE, vec3(0, 0, 0));
    destroy_object(pick_up);

    move_object(character, pick_up_pos, false);
}

void level_state_t::handle_interaction
        (object_t *terminal, object_t *character) {
    if (character == nullptr) { 
        warp_log_e("Cannot handle interaction, null character.");
        return;
    }
    if (terminal == nullptr) { 
        warp_log_e("Cannot handle interaction, null terminal.");
        return;
    }

    character->entity->receive_message(CORE_DO_BOUNCE, terminal->position);
    if (character->flags & FOBJ_PLAYER_AVATAR) {
        event_t event = {*terminal, EVENT_PLAYER_ACTIVATED_TERMINAL};
        _events.push_back(event);
    }
}

void level_state_t::handle_attack(object_t *target, object_t *attacker) {
    if (target == nullptr) { 
        warp_log_e("Cannot handle attack, null target.");
        return;
    }
    const bool attacker_can_push
        = attacker == nullptr ? false : (attacker->flags & FOBJ_CAN_PUSH) != 0;
    const bool target_is_boulder = target->type == OBJ_BOULDER;
    /* TODO: shouldn't this be a separate function? */
    const vec3_t original_position = target->position;
    const vec3_t attacker_position
        = attacker == nullptr ? original_position : attacker->position;
    const vec3_t d = vec3_sub(original_position, attacker_position);
    const vec3_t push_back = vec3_add(target->position, d);
    bool can_push_back = can_move_to(push_back);
    if (target_is_boulder) {
        can_push_back = can_push_back && attacker_can_push;
    }

    const int damage = calculate_damage(*target);
    const bool alive = hurt_object(target, damage);

    if (alive && can_push_back) {
        move_object(target, push_back, false);
    }


    if (attacker != nullptr) {
        /* rotate attacker */
        const vec3_t diff = vec3_sub(target->position, original_position);
        change_direction(attacker, vec3_to_dir(diff));
        
        if (target_is_boulder) {
            if (can_move_to(original_position) && attacker_can_push) {
                move_object(attacker, original_position, false);
            } else {
                attacker->entity->receive_message(CORE_DO_BOUNCE, original_position);
            }
        } else {
            attacker->entity->receive_message(CORE_DO_ATTACK, original_position);
        }
    }
}

void level_state_t::handle_shooting(object_t *shooter, warp::dir_t dir) {
    if (shooter == nullptr) { 
        warp_log_e("Cannot handle shooting, null shooter.");
        return;
    }
    if ((shooter->flags & FOBJ_CAN_SHOOT) == 0 || shooter->ammo <= 0) { 
        return;
    }

    change_direction(shooter, dir);

    const float speed = 4.5f;

    const vec3_t d = dir_to_vec3(dir);
    const vec3_t pos = vec3_add(shooter->position, vec3_scale(d, 0.5f));
    const vec3_t v = vec3_add(vec3_scale(d, speed), vec3(0.001f, 0, 0.001f));
    _bullet_factory->create_bullet(pos, v, BULLET_ARROW, _level);

    const vec3_t recoil = vec3_add(shooter->position, vec3_scale(d, -1));
    shooter->entity->receive_message(CORE_DO_BOUNCE, recoil);
    shooter->ammo -= 1;
}

void level_state_t::change_button_state(feature_t *feat, feat_state_t state) {
    if (feat == nullptr) {
        warp_log_e("Cannot change state of null feature.");
        return;
    }
    if (feat->type != FEAT_BUTTON) {
        warp_log_e("Expected feature to be of button type.");
        return;
    }

    feat->state = state;
    feature_t *target = _features[feat->target_id];
    if (target != nullptr) {
        target->state = state;
        target->entity->receive_message(CORE_FEAT_STATE_CHANGE, target->state);
    }
}

void level_state_t::move_object(object_t *target, vec3_t pos, bool immediate) {
    if (target == nullptr) {
        warp_log_e("Cannot handle move, null target.");
        return;
    }

    const vec3_t old_position = target->position;
    const size_t old_x = round(old_position.x);
    const size_t old_z = round(old_position.z);
    const size_t new_x = round(pos.x);
    const size_t new_z = round(pos.z);

    const vec3_t diff = vec3_sub(pos, old_position);
    const dir_t new_dir = vec3_to_dir(diff); 
    change_direction(target, new_dir);

    _objects[old_x + _width * old_z] = nullptr;
    _objects[new_x + _width * new_z] = target;

    feature_t *old_feat = (feature_t *)feature_at(old_x, old_z);
    if (old_feat != nullptr) {
        if (old_feat->type == FEAT_BUTTON) {
            change_button_state(old_feat, FSTATE_INACTIVE);
        } else if (old_feat->type == FEAT_BREAKABLE_FLOOR) {
            old_feat->entity->receive_message(CORE_FEAT_STATE_CHANGE, FSTATE_ACTIVE);
            _features[old_x + _width * old_z]
                = create_feature(_world, FEAT_SPIKES, 0, old_x, old_z);
            delete old_feat;
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
            const tile_t *tile = _level->get_tile_at(x, z);
            if (tile != NULL && tile->is_stairs) { 
                event_t event = {*target, EVENT_PLAYER_ENTER_PORTAL};
                _events.push_back(event);
            }
        }
    }

    feature_t *new_feat = (feature_t *)feature_at(new_x, new_z);
    if (new_feat != nullptr) {
        const feature_type_t type = new_feat->type;
        if (type == FEAT_BUTTON) {
            change_button_state(new_feat, FSTATE_ACTIVE);
        } else if (type == FEAT_SPIKES) {
            if (new_feat->state == FSTATE_ACTIVE) {
                hurt_object(target, target->health);
                target->entity->receive_message(CORE_DO_FALL, pos);
                if (target->type == OBJ_BOULDER) {
                    destroy_object(target);
                    new_feat->state = FSTATE_INACTIVE;
                }
            }
        }
    }
}

bool level_state_t::hurt_object(object_t *target, int damage) {
    if (target == nullptr) {
        warp_log_e("Cannot hurt object, target is null.");
        return false;
    }
    if (damage <= 0) {
        return true;
    }
    if (target->type == OBJ_BOULDER) {
        return true;
    }

    const int health_left = target->health - damage;
    target->health = health_left;
    if (health_left <= 0) {
        target->entity->receive_message(CORE_DO_DIE, vec3(0, 0, 0));
        event_t event = {*target, EVENT_OBJECT_KILLED};
        _events.push_back(event);
        
        destroy_object(target);
    } else {
        if (target->type != OBJ_BOULDER) {
            target->entity->receive_message(CORE_DO_HURT, vec3(0, 0, 0));
        }
        event_t event = {*target, EVENT_OBJECT_HURT};
        _events.push_back(event);
    }

    return health_left > 0;
}

void level_state_t::destroy_object(object_t *obj) {
    const size_t x = round(obj->position.x);
    const size_t z = round(obj->position.z);
    _objects[x + _width * z] = nullptr;
    delete obj;
}

