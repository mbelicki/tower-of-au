#define WARP_DROP_PREFIX
#include "level_state.h"

#include <stdlib.h>

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
    if (physics != NULL) {
        physics->_flags = PHYSFLAGS_FIXED;
        physics->_velocity = vec3(0, 0, 0);
        aabb_init(&physics->_bounds, vec3(0, 0, 0), vec3(size.x, 1, size.y));
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
    entity_t *entity = NULL;
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
        return NULL;
    }

    feature_t *feat = new feature_t;
    feat->type = type;
    feat->target_id = target;
    feat->entity = entity;
    feat->entity->set_tag(WARP_TAG("feature"));
    feat->state = type == FEAT_SPIKES ? FSTATE_ACTIVE : FSTATE_INACTIVE;
    feat->x = x; feat->z = z;

    return feat;
}

level_state_t::level_state_t(world_t *world, size_t width, size_t height) 
        : _initialized(false)
        , _obj_pool(NULL)
        , _feat_pool(NULL)
        , _obj_placement(NULL)
        , _feat_placement(NULL)
        , _world(world)
        , _width(width)
        , _height(height)
        , _bullet_factory(NULL) 
        , _object_factory(NULL) {
    const size_t count = _width * _height;
    _obj_placement  = (obj_id_t *)  calloc(count, sizeof *_obj_placement);
    _feat_placement = (feat_id_t *) calloc(count, sizeof *_feat_placement);

    _obj_pool  = pool_create_typed(object_t,  count, NULL);
    _feat_pool = pool_create_typed(feature_t, count, NULL);
}

level_state_t::~level_state_t() {
    delete _bullet_factory;
    delete _object_factory;

    warp_pool_destroy(_obj_pool);
    warp_pool_destroy(_feat_pool);

    free(_obj_placement);
    free(_feat_placement);
}

void level_state_t::place_object_at(obj_id_t id, size_t x, size_t z) {
    if (x >= _width || z >= _height) {
        warp_log_e("Trying to place object out of level bounds. "
                   "Placement: (%zu, %zu), level size: (%zu, %zu). "
                  , x, z, _width, _height
                  );
        return;
    }
    _obj_placement[x + _width * z] = id;
}

obj_id_t level_state_t::add_object
        (const object_t *obj, const warp_tag_t &def_name) {
    const size_t x = round(obj->position.x);
    const size_t z = round(obj->position.z);
    if (object_at(x, z) != OBJ_ID_INVALID) {
        return OBJ_ID_INVALID;
    }

    obj_id_t id = pool_create_item(_obj_pool);
    object_t *new_obj = pool_get(object_t, _obj_pool, id);
    memmove(new_obj, obj, sizeof *new_obj);

    if (new_obj->entity == NULL) {
        new_obj->entity
            = _object_factory->create_object_entity(new_obj, def_name, _world);
    }

    place_object_at(id, x, z);
    new_obj->entity->receive_message(CORE_DO_ROTATE, (int)new_obj->direction);
    return id;
}

obj_id_t level_state_t::spawn_object
        (warp_tag_t name, vec3_t pos, warp_random_t *rand) {
    object_t *new_obj = _object_factory->spawn(name, pos, DIR_Z_PLUS, rand, _world);
    obj_id_t id = add_object(new_obj, name);
    delete new_obj;
    return id != OBJ_ID_INVALID; 
}

feat_id_t level_state_t::spawn_feature
        (feature_type_t type, size_t target, size_t x, size_t z) {
    if (feature_at(x, z) != FEAT_ID_INVALID) {
        return FEAT_ID_INVALID;
    }

    feature_t *feat = create_feature(_world, type, target, x, z);

    feat_id_t id = pool_create_item(_feat_pool);
    feature_t *new_feat = pool_get(feature_t, _feat_pool, id);
    memmove(new_feat, feat, sizeof *new_feat);
    delete feat;

    _feat_placement[x + _width * z] = id;
    return id;
}

void level_state_t::set_object_flag(obj_id_t obj, object_flags_t flag) {
    object_t *object = get_mutable_object(obj);
    if (object == NULL) {
        warp_log_e("Cannot set flag, object not found.");
        return;
    }
    object->flags |= flag;
}

obj_id_t level_state_t::object_at_position(vec3_t pos) const {
    return object_at(round(pos.x), round(pos.z));
}

obj_id_t level_state_t::object_at(size_t x, size_t y) const {
    if (x >= _width || y >= _height) return OBJ_ID_INVALID;
    return _obj_placement[x + _width * y];
}

obj_id_t  level_state_t::find_player() const {
    pool_it_t *it = pool_iterate(_obj_pool);
    for (; it != NULL; it = pool_it_next(it)) {
        const object_t *obj = pool_it_get(object_t, it);
        if (obj->flags & FOBJ_PLAYER_AVATAR) {
            return pool_it_to_id(_obj_pool, it);
        }
    }
    return OBJ_ID_INVALID;
}

void level_state_t::find_all_characters(std::vector<obj_id_t> *characters) {
    if (characters == NULL) {
        warp_log_e("Called with null 'characters' paramter, skipping call.");
        return;
    }

    pool_it_t *it = pool_iterate(_obj_pool);
    for (; it != NULL; it = pool_it_next(it)) {
        const object_t *obj = pool_it_get(object_t, it);
        if (obj->type == OBJ_CHARACTER) {
            obj_id_t id = pool_it_to_id(_obj_pool, it);
            characters->push_back(id);
        }
    }
}

feat_id_t level_state_t::feature_at(size_t x, size_t y) const {
    if (x >= _width || y >= _height) return OBJ_ID_INVALID;
    return _feat_placement[x + _width * y];
}

bool level_state_t::can_move_to(vec3_t new_pos) const {
    const int x = round(new_pos.x);
    const int z = round(new_pos.z);

    const tile_t *tile = _level->get_tile_at(x, z);
    if (tile == NULL) return true;

    if (tile->is_walkable == false) return false;

    const obj_id_t object = object_at(x, z);
    if (object != OBJ_ID_INVALID) return false;

    const feat_id_t feature = feature_at(x, z);
    if (feature != FEAT_ID_INVALID) {
        const feature_t *feat = get_feature(feature);
        warp_log_d("feature type: %d", (int)feat->type);
        if (feat->type == FEAT_DOOR && feat->state == FSTATE_INACTIVE) {
            return false;
        }
    }
    return true;
}

bool level_state_t::is_object_idle(obj_id_t obj) const {
    const object_t *o = pool_get(object_t, _obj_pool, obj);
    if (o && o->entity) {
        const dynval_t is_idle = o->entity->get_property(WARP_TAG("avat.is_idle"));
        return is_idle.get_int() == 1;
    }
    return false;
}

bool level_state_t::is_object_valid(obj_id_t obj) const {
    void *maybe_data = pool_get_data(_obj_pool, obj);
    return maybe_data != NULL;
}

static void change_direction(object_t *obj, dir_t dir) {
    if (obj->direction != dir && obj->type != OBJ_BOULDER) {
        obj->direction = dir;
        obj->entity->receive_message(CORE_DO_ROTATE, (int)dir);
    }
}

void level_state_t::spawn(const level_t *level, warp_random_t *rand) {
    if (level == NULL) {
        warp_log_e("Cannot spawn objects, null level.");
        return;
    }
    if (level->get_width() != _width || level->get_height() != _height) {
        warp_log_e("Cannot spawn objects, level size does not match "
                   "level state size (level: %zux%zu, state: %zux%zu)."
                  , level->get_width(), level->get_height(), _width, _height
                  );
        return;
    }
    if (rand == NULL) {
        warp_log_e("Cannot spawn objects, null random generator.");
        return;
    }
    if (_initialized) {
        warp_log_e("Cannot spawn objects, already spawned.");
        return;
    }
    _initialized = true;
    _level = level;

    if (_bullet_factory == NULL) {
        _bullet_factory = new bullet_factory_t(_world);
        _bullet_factory->initialize();
    }
    if (_object_factory == NULL) {
        _object_factory = new object_factory_t();
        _object_factory->load_definitions("objects.json");
        _object_factory->load_resources(_world->get_resources());
    }

    for (size_t i = 0; i < _width; i++) {
        for (size_t j = 0; j < _height; j++) {
            const tile_t *tile = level->get_tile_at(i, j);
            if (warp_tag_equals_buffer(&tile->object_id, "") == false 
                    && tile->spawn_probablity > 0) {
                const float r = warp_random_float(rand);
                if (r <= tile->spawn_probablity) {
                    const vec3_t pos = vec3(i, 0, j);
                    spawn_object(tile->object_id, pos, rand);
                    //if (obj != NULL) {
                    //    change_direction(obj, tile->object_dir);
                    //    _objects[id] = obj;
                    //}
                }
            }
            if (tile->feature != FEAT_NONE) {
                const size_t target = tile->feat_target_id;
                spawn_feature(tile->feature, target, i, j);
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
        update_object(command.object_id, command.command);
    }
}

void level_state_t::process_real_time_event(const rt_event_t &event) {
    if (_initialized == false) {
        warp_log_e("Cannot process real time event, state not spawned.");
        return;
    }

    //_events.clear();
    if (event.type == RT_EVENT_BULETT_HIT) {
        const vec3_t target_pos = event.value.get_vec3();
        obj_id_t object = object_at_position(target_pos);
        handle_attack(object, OBJ_ID_INVALID);
    }
}

void level_state_t::clear() {
    if (_initialized == false) {
        warp_log_e("Cannot clear, already cleared.");
        return;
    }
    _initialized = false;

    warp_array_t buffer = warp_array_create_typed(entity_t *, 16, NULL);
    _world->find_all_entities(WARP_TAG("object"), &buffer);
    _world->find_all_entities(WARP_TAG("feature"), &buffer);
    _world->find_all_entities(WARP_TAG("bullet"), &buffer);
    for (size_t i = 0; i < warp_array_get_size(&buffer); i++) {
        entity_t *e = warp_array_get_value(entity_t *, &buffer, i);
        _world->destroy_later(e);
    }
    warp_array_destroy(&buffer);

    _events.clear();

    pool_clear(_obj_pool);
    pool_clear(_feat_pool);

    const size_t count = _width * _height;
    for (size_t i = 0; i < count; i++) {
        _obj_placement[i]  = OBJ_ID_INVALID;
        _feat_placement[i] = FEAT_ID_INVALID;
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

object_t *level_state_t::get_mutable_object(obj_id_t id) const {
    return pool_get(object_t, _obj_pool, id);
}

feature_t *level_state_t::get_mutable_feature(feat_id_t id) const {
    return pool_get(feature_t, _feat_pool, id);
}

const object_t *level_state_t::get_object(obj_id_t id) const {
    return get_mutable_object(id);
}

const feature_t *level_state_t::get_feature(feat_id_t id) const {
    return get_mutable_feature(id);
}

static vec3_t calculate_new_pos(const object_t *obj, move_dir_t dir) {
    const dir_t direction = get_move_direction(dir);
    const vec3_t step = dir_to_vec3(direction);
    return vec3_add(obj->position, step);
}

void level_state_t::update_object(obj_id_t obj, const message_t &command) {
    if (obj == OBJ_ID_INVALID) {
        warp_log_e("Cannot update invalid object.");
        return;
    }

    const object_t *object = get_object(obj);
    const messagetype_t type = command.type;
    if (type == CORE_TRY_MOVE) {
        move_dir_t dir = (move_dir_t) command.data.get_int();
        const vec3_t pos = calculate_new_pos(object, dir);
        handle_move(obj, pos);
    } else if (type == CORE_TRY_SHOOT) {
        const move_dir_t direction = (move_dir_t) command.data.get_int();
        handle_shooting(obj, get_move_direction(direction));
    } else {
        warp_log_d("Object not updated: unsupported message type: %d.", (int)type);
    }
}

bool level_state_t::has_object_flag(obj_id_t id, object_flags_t flag) const {
    const object_t *obj = get_object(id);
    return obj != NULL && (obj->flags & flag);
}

bool level_state_t::has_object_type(obj_id_t id, object_type_t type) const {
    const object_t *obj = get_object(id);
    return obj != NULL && obj->type == type;
}

bool level_state_t::has_feature_type(feat_id_t id, feature_type_t type) const {
    const feature_t *feat = get_feature(id);
    return feat != NULL && feat->type == type;
}

void level_state_t::handle_move(obj_id_t target, vec3_t pos) {
    if (target == OBJ_ID_INVALID) { 
        warp_log_e("Cannot handle move, invalid target.");
        return;
    }

    if (can_move_to(pos)) {
        move_object(target, pos, false);
    } else {
        obj_id_t other = object_at_position(pos);
        if (other != OBJ_ID_INVALID) {
            const object_t *obj = get_object(other);
            const bool can_chat = has_object_flag(target, FOBJ_PLAYER_AVATAR)
                               && obj->chat_scipt != NULL;
            if (obj->type == OBJ_TERMINAL) {
                handle_interaction(other, target);
            } else if (obj->type == OBJ_PICK_UP) {
                handle_picking_up(other, target);
            } else if (can_chat){
                handle_conversation(other, target);
            } else {
                handle_attack(other, target);
            }
        } else {
            const object_t *obj = get_object(target);
            obj->entity->receive_message(CORE_DO_BOUNCE, pos);
        }
    }
}

static int calculate_damage(const object_t *target) {
    return target->type == OBJ_BOULDER ? 0 : 1;
}

void level_state_t::handle_picking_up(obj_id_t pick_up, obj_id_t character) {
    if (character == OBJ_ID_INVALID) { 
        warp_log_e("Cannot handle picking up, invalid character.");
        return;
    }
    if (pick_up == OBJ_ID_INVALID) { 
        warp_log_e("Cannot handle picking up, invalid pick up.");
        return;
    }

    object_t *pick_obj = get_mutable_object(pick_up);
    object_t *char_obj = get_mutable_object(character);

    char_obj->health     += pick_obj->health;
    char_obj->ammo       += pick_obj->ammo;
    char_obj->max_health += pick_obj->max_health;

    if (char_obj->health > char_obj->max_health) {
        char_obj->health = char_obj->max_health;
    }

    const vec3_t pick_up_pos = pick_obj->position;
    pick_obj->entity->receive_message(CORE_DO_DIE, vec3(0, 0, 0));
    destroy_object(pick_up);

    move_object(character, pick_up_pos, false);
}

void level_state_t::handle_conversation(obj_id_t npc, obj_id_t player) {
    const object_t *npc_obj    = get_object(npc);
    const object_t *player_obj = get_object(player);

    player_obj->entity->receive_message(CORE_DO_ATTACK, npc_obj->position);
    event_t event = {npc, *npc_obj, EVENT_PLAYER_STARTED_CONVERSATION};
    _events.push_back(event);
}

void level_state_t::handle_interaction(obj_id_t terminal, obj_id_t character) {
    if (character == OBJ_ID_INVALID) { 
        warp_log_e("Cannot handle interaction, invalid character.");
        return;
    }
    if (terminal == OBJ_ID_INVALID) { 
        warp_log_e("Cannot handle interaction, invalid terminal.");
        return;
    }

    const object_t *term_obj = get_object(terminal);
    const object_t *char_obj = get_object(character);

    char_obj->entity->receive_message(CORE_DO_BOUNCE, term_obj->position);
    if (char_obj->flags & FOBJ_PLAYER_AVATAR) {
        event_t event = {terminal, *term_obj, EVENT_PLAYER_ACTIVATED_TERMINAL};
        _events.push_back(event);
    }
}

void level_state_t::handle_attack(obj_id_t target, obj_id_t attacker) {
    if (target == OBJ_ID_INVALID) { 
        warp_log_e("Cannot handle attack, invalid target.");
        return;
    }
    //if (attacker == OBJ_ID_INVALID) { 
    //    warp_log_e("Cannot handle attack, invalid attacker.");
    //    return;
    //}
    
    if (has_object_flag(attacker, FOBJ_FRIENDLY) 
            && has_object_flag(target, FOBJ_PLAYER_AVATAR)) {
        return;
    }

    const bool attacker_can_push = has_object_flag(attacker, FOBJ_CAN_PUSH);
    const bool target_is_boulder = has_object_type(target, OBJ_BOULDER);

    const object_t *target_obj   = get_object(target);
    const object_t *attacker_obj = get_object(attacker);

    /* TODO: shouldn't this be a separate function? */
    const vec3_t original_position = target_obj->position;
    const vec3_t attacker_position
        = attacker_obj == NULL ? original_position : attacker_obj->position;
    const vec3_t d = vec3_sub(original_position, attacker_position);
    const vec3_t push_back = vec3_add(target_obj->position, d);
    bool can_push_back = can_move_to(push_back);
    if (target_is_boulder) {
        can_push_back = can_push_back && attacker_can_push;
    }

    const int damage = calculate_damage(target_obj);
    const bool alive = hurt_object(target, damage);

    if (alive && can_push_back) {
        move_object(target, push_back, false);
    }

    if (attacker_obj != NULL) {
        /* rotate attacker */
        const vec3_t diff = vec3_sub(target_obj->position, original_position);
        change_direction(get_mutable_object(attacker), vec3_to_dir(diff));
        
        if (target_is_boulder) {
            if (can_move_to(original_position) && attacker_can_push) {
                move_object(attacker, original_position, false);
            } else {
                attacker_obj->entity->receive_message(CORE_DO_BOUNCE, original_position);
            }
        } else {
            attacker_obj->entity->receive_message(CORE_DO_ATTACK, original_position);
        }
    }
}

void level_state_t::handle_shooting(obj_id_t shooter, warp_dir_t dir) {
    if (shooter == OBJ_ID_INVALID) { 
        warp_log_e("Cannot handle shooting, invalid shooter.");
        return;
    }
    object_t *obj = get_mutable_object(shooter);
    if (has_object_flag(shooter, FOBJ_CAN_SHOOT) == false || obj->ammo <= 0) { 
        return;
    }

    change_direction(obj, dir);

    const float speed = 4.5f;

    const vec3_t d = dir_to_vec3(dir);
    const vec3_t pos = vec3_add(obj->position, vec3_scale(d, 0.5f));
    const vec3_t v = vec3_add(vec3_scale(d, speed), vec3(0.001f, 0, 0.001f));
    _bullet_factory->create_bullet(pos, v, BULLET_ARROW, _level);

    const vec3_t recoil = vec3_add(obj->position, vec3_scale(d, -1));
    obj->entity->receive_message(CORE_DO_BOUNCE, recoil);
    obj->ammo -= 1;
}

void level_state_t::change_button_state(feat_id_t button, feat_state_t state) {
    if (button == FEAT_ID_INVALID) {
        warp_log_e("Cannot change state of invalid feature.");
        return;
    }
    if (has_feature_type(button, FEAT_BUTTON) == false) {
        warp_log_e("Expected feature to be of button type.");
        return;
    }

    feature_t *feat = get_mutable_feature(button);
    feat->state = state;
    feat_id_t target = _feat_placement[feat->target_id];
    if (target != FEAT_ID_INVALID) {
        feature_t *targ_feat = get_mutable_feature(target);
        targ_feat->state = state;
        targ_feat->entity->receive_message(CORE_FEAT_STATE_CHANGE, targ_feat->state);
    }
}

const char *name_object_type(object_type_t t) {
    switch (t) {
#define entry(n) case n: return #n;
        entry(OBJ_NONE)
        entry(OBJ_BOULDER)
        entry(OBJ_TERMINAL)
        entry(OBJ_CHARACTER)
        entry(OBJ_PICK_UP)
#undef entry
        default: return "<unknown>";
    }
}

void level_state_t::move_object(obj_id_t id, vec3_t pos, bool immediate) {
    if (id == OBJ_ID_INVALID) {
        warp_log_e("Cannot handle move, invalid target.");
        return;
    }

    object_t *target = get_mutable_object(id);

    const vec3_t old_position = target->position;
    const size_t old_x = round(old_position.x);
    const size_t old_z = round(old_position.z);
    const size_t new_x = round(pos.x);
    const size_t new_z = round(pos.z);

    if (object_at(old_x, old_z) != id) {
        warp_log_e("Cannot move, object is not there! "
                   "id: %d, old position: (%zu, %zu), type: %s" 
                  , (int)id, old_x, old_z, name_object_type(target->type)
                  );
        return;
    }

    const vec3_t diff = vec3_sub(pos, old_position);
    const dir_t new_dir = vec3_to_dir(diff); 
    change_direction(target, new_dir);

    place_object_at(OBJ_ID_INVALID, old_x, old_z);
    place_object_at(id,             new_x, new_z);

    target->position = pos;
    const int msg_type = immediate ? CORE_DO_MOVE_IMMEDIATE : CORE_DO_MOVE;
    target->entity->receive_message(msg_type, pos);

    feat_id_t old_feat = feature_at(old_x, old_z);
    if (old_feat != FEAT_ID_INVALID) {
        if (has_feature_type(old_feat, FEAT_BUTTON)) {
            change_button_state(old_feat, FSTATE_INACTIVE);
        } else if (has_feature_type(old_feat, FEAT_BREAKABLE_FLOOR)) {
            get_feature(old_feat)->entity->receive_message(CORE_FEAT_STATE_CHANGE, FSTATE_ACTIVE);
            destroy_feature(old_feat);
            spawn_feature(FEAT_SPIKES, 0, old_x, old_z);
        }
    }

    if (has_object_flag(id, FOBJ_PLAYER_AVATAR)) {
        const int x = round(pos.x);
        const int z = round(pos.z);
        if (x < 0 || x >= 13 || z < 0 || z >= 11) {
            event_t event = {id, *target, EVENT_PLAYER_LEAVE};
            _events.push_back(event);
        } else {
            const tile_t *tile = _level->get_tile_at(x, z);
            if (tile != NULL && tile->is_stairs) { 
                event_t event = {id, *target, EVENT_PLAYER_ENTER_PORTAL};
                _events.push_back(event);
            }
        }
    }

    feat_id_t new_feat = feature_at(new_x, new_z);
    if (new_feat != FEAT_ID_INVALID) {
        if (has_feature_type(new_feat, FEAT_BUTTON)) {
            change_button_state(new_feat, FSTATE_ACTIVE);
        } else if (has_feature_type(new_feat, FEAT_SPIKES)) {
            feature_t *feat = get_mutable_feature(new_feat);
            if (feat->state == FSTATE_ACTIVE) {
                hurt_object(id, target->health);
                target->entity->receive_message(CORE_DO_FALL, pos);
                if (target->type == OBJ_BOULDER) {
                    destroy_object(id);
                    feat->state = FSTATE_INACTIVE;
                }
            }
        }
    }
}

bool level_state_t::hurt_object(obj_id_t id, int damage) {
    if (id == OBJ_ID_INVALID) {
        warp_log_e("Cannot hurt object, target is invalid.");
        return false;
    }
    if (damage <= 0) {
        return true;
    }
    if (has_object_type(id, OBJ_BOULDER)) {
        return true;
    }

    object_t *target = get_mutable_object(id);
    const int health_left = target->health - damage;
    target->health = health_left;
    if (health_left <= 0) {
        target->entity->receive_message(CORE_DO_DIE, vec3(0, 0, 0));
        event_t event = {id, *target, EVENT_OBJECT_KILLED};
        _events.push_back(event);
        
        destroy_object(id);
    } else {
        if (target->type != OBJ_BOULDER) {
            target->entity->receive_message(CORE_DO_HURT, vec3(0, 0, 0));
        }
        event_t event = {id, *target, EVENT_OBJECT_HURT};
        _events.push_back(event);
    }

    return health_left > 0;
}

void level_state_t::destroy_object(obj_id_t id) {
    const object_t *obj = get_object(id);
    const size_t x = round(obj->position.x);
    const size_t z = round(obj->position.z);
    place_object_at(OBJ_ID_INVALID, x, z);
    pool_destroy_item(_obj_pool, id);
}

void level_state_t::destroy_feature(feat_id_t id) {
    const feature_t *feat = get_feature(id);
    _feat_placement[feat->x + _width * feat->z] = FEAT_ID_INVALID;
    pool_destroy_item(_feat_pool, id);
}
