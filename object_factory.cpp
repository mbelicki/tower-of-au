#define WARP_DROP_PREFIX
#include "object_factory.h"

#include "warp/utils/io.h"
#include "warp/world.h"
#include "warp/entity.h"
#include "warp/entity-helpers.h"
#include "warp/utils/str.h"

#include "libs/parson/parson.h"

#include "level_state.h"
#include "character.h"

using namespace warp;

enum move_type_t : int {
    OBJMOVE_NONE,
    OBJMOVE_STILL,
    OBJMOVE_LINE,
    OBJMOVE_ROAM,
};

struct object_def_t {
    object_type_t type;
    dir_t         direction;
    move_type_t   movement_type;
    int           health;
    int           max_health;
    int           ammo;
    bool          can_shoot;
    bool          can_rotate;
    bool          can_push;
    bool          is_player;
    bool          is_friendly;

    warp_str_t    mesh_name;
    warp_str_t    texture_name;
};

static bool has_json_member(JSON_Object *obj, const char *member_name) {
    return json_object_get_value(obj, member_name) != NULL;
}

static void destroy_element(void *raw) {
    object_def_t *def = (object_def_t *) raw;
    warp_str_destroy(&def->mesh_name);
    warp_str_destroy(&def->texture_name);
    free(def);
}

object_factory_t::object_factory_t() {
    _objects = warp_map_create_typed(object_def_t, destroy_element);
}

static object_type_t parse_type(JSON_Object *obj) {
    if (has_json_member(obj, "type") == false) {
        return OBJ_NONE;
    }
    const char *value = json_object_get_string(obj, "type");
    if (strncmp("character", value, 10) == 0) {
        return OBJ_CHARACTER;
    } else if (strncmp("boulder", value, 8) == 0) {
        return OBJ_BOULDER;
    } else if (strncmp("terminal", value, 9) == 0) {
        return OBJ_TERMINAL;
    } else if (strncmp("pick-up", value, 8) == 0) {
        return OBJ_PICK_UP;
    }
    return OBJ_NONE;
}

static move_type_t parse_movement(JSON_Object *obj) {
    if (has_json_member(obj, "movementType") == false) {
        return OBJMOVE_STILL;
    }
    const char *value = json_object_get_string(obj, "movementType");
    if (strncmp("still", value, 6) == 0) {
        return OBJMOVE_STILL;
    } else if (strncmp("line", value, 5) == 0) {
        return OBJMOVE_LINE;
    } else if (strncmp("roam", value, 5) == 0) {
        return OBJMOVE_ROAM;
    } 
    return OBJMOVE_STILL;
}

static int parse_int_property
        (JSON_Object *obj, const char *name, int default_value) {
    if (has_json_member(obj, name) == false) {
        return default_value;
    }
    return (int)json_object_get_number(obj, name);
}

static bool parse_bool_flag(JSON_Object *obj, const char *name) {
    if (has_json_member(obj, name) == false) {
        return false;
    }
    return json_object_get_boolean(obj, name);
}

static void parse_graphics(object_def_t *def, JSON_Object *object) {
    const char *mesh_name = json_object_dotget_string(object, "graphics.mesh");
    const char *tex_name  = json_object_dotget_string(object, "graphics.texture");
    
    def->mesh_name = warp_str_create(mesh_name == NULL ? "npc.obj" : mesh_name);
    def->texture_name = warp_str_create(tex_name == NULL ? "missing.png" : tex_name);
}

static void parse_definition(JSON_Object *object, warp_map_t *objects) {
    if (json_object_get_value(object, "name") == NULL) {
        warp_log_e("Failed to parse object definition, missing name.");
        return;
    }

    const warp_tag_t name = WARP_TAG(json_object_get_string(object, "name"));
    object_def_t def;
    def.type          = parse_type(object);
    def.movement_type = parse_movement(object);
    def.health        = parse_int_property(object, "health", 1);
    def.max_health    = parse_int_property(object, "max_health", def.health);
    def.ammo          = parse_int_property(object, "ammo", 0);
    def.can_shoot     = parse_bool_flag(object, "canShoot");
    def.can_rotate    = parse_bool_flag(object, "canRotate");
    def.can_push      = parse_bool_flag(object, "canPush");
    def.is_player     = parse_bool_flag(object, "playerAvatar");
    def.is_friendly   = parse_bool_flag(object, "friendly");
    
    parse_graphics(&def, object);

    warp_map_tag_insert(objects, name, &def);
}

bool object_factory_t::load_definitions(const char *filename) {
    bool result = true;
    const char *filepath = NULL;
    const char *content = NULL;

    warp_array_t bytes;

    warp_str_t path = warp_str_format("assets/data/%s", filename);
    warp_str_t out_path;
    
    warp_result_t find_result;
    warp_result_t read_result;

    JSON_Value *root_value = NULL;
    const JSON_Object *root = NULL;
    const JSON_Array *objects = NULL;

    find_result = find_path(&path, &out_path);
    if (WARP_FAILED(find_result)) {
        warp_result_log("Failed to find texture path", &find_result);
        warp_result_destory(&find_result);
        result = false;
        goto cleanup;
    }

    filepath = warp_str_value(&out_path);
    read_result = read_file(filepath, &bytes);
    if (WARP_FAILED(read_result)) {
        warp_result_log("Failed to find texture path", &read_result);
        warp_result_destory(&read_result);
        result = false;
        goto cleanup;
    }

    content = (char *) warp_array_get(&bytes, 0);
    root_value = json_parse_string(content);
    if (json_value_get_type(root_value) != JSONObject) {
       warp_log_e("Cannot parse %s: root element is not an object.", path);
       result = false;
       goto cleanup;
    }

    root = json_value_get_object(root_value);
    objects = json_object_dotget_array(root, "objects");
    for (size_t i = 0; i < json_array_get_count(objects); i++) {
        JSON_Object *object = json_array_get_object(objects, i);
        parse_definition(object, &_objects);
    }

cleanup:
    warp_str_destroy(&path);
    warp_str_destroy(&out_path);
    warp_array_destroy(&bytes);
    json_value_free(root_value);

    return result;
}


void object_factory_t::load_resources(resources_t *res) {
    warp_map_it_t *it = warp_map_iterate(&_objects);
    for (; it != NULL; it = warp_map_it_next(it, &_objects)) {
        const object_def_t *def = (object_def_t *) warp_map_it_get(it);
        const char *mesh = warp_str_value(&def->mesh_name); 
        const char *tex  = warp_str_value(&def->texture_name); 
        resources_load(res, mesh);
        resources_load(res, tex);
    }
}

static dir_t random_direction(warp_random_t *rand) {
    const dir_t directions[4] {
        DIR_X_PLUS, DIR_Z_PLUS, DIR_X_MINUS, DIR_Z_MINUS,
    };
    return directions[warp_random_from_range(rand, 0, 3)];
}

static dir_t evaluate_direction(dir_t dir, warp_random_t *rand) {
    if (dir == DIR_NONE) {
        return random_direction(rand);
    }
    return dir;
}

static object_flags_t movement_to_flag(move_type_t move) {
    switch (move) {
        case OBJMOVE_STILL:
            return FOBJ_NPCMOVE_STILL;
        case OBJMOVE_LINE:
            return FOBJ_NPCMOVE_LINE;
        case OBJMOVE_ROAM:
            return FOBJ_NPCMOVE_ROAM;
        case OBJMOVE_NONE:
        default:
            return FOBJ_NONE;
    }
}

static object_flags_t evaluate_flags(const object_def_t *def) {
    object_flags_t flags = FOBJ_NONE;
    if (def->can_shoot) {
        flags |= FOBJ_CAN_SHOOT;
    }
    if (def->can_rotate) {
        flags |= FOBJ_CAN_ROTATE;
    }
    if (def->can_push) {
        flags |= FOBJ_CAN_PUSH;
    }
    if (def->is_player) {
        flags |= FOBJ_PLAYER_AVATAR;
    }
    if (def->is_friendly) {
        flags |= FOBJ_FRIENDLY;
    }
    flags |= movement_to_flag(def->movement_type);
    return flags;
}

static graphics_comp_t *create_graphics
        (world_t *world, const object_def_t *def) {
    const char *mesh_name = warp_str_value(&def->mesh_name);
    const char *tex_name = warp_str_value(&def->texture_name);
    return create_single_model_graphics(world, mesh_name, tex_name);
}

static controller_comp_t *create_controller
        (world_t *world, const object_t *obj) {
    const bool confirm_moves = (obj->flags & FOBJ_PLAYER_AVATAR) != 0;
    return create_character_controller(world, confirm_moves);
}

static physics_comp_t *create_physics(world_t *world, const object_t *obj) {
    /* TODO: this should be put in definition: */
    vec2_t size = vec2(0.4f, 0.4f);
    if (obj->type == OBJ_BOULDER) {
        size = vec2(0.6f, 0.6f);
    }
    
    physics_comp_t *physics = world->create_physics();
    if (physics != nullptr) {
        physics->_flags = PHYSFLAGS_FIXED;
        physics->_velocity = vec3(0, 0, 0);
        aabb_init(&physics->_bounds, vec3(0, 0, 0), vec3(size.x, 1, size.y));
    }

    return physics;
}

static entity_t *create_entity
        (world_t *world, const object_def_t *def, const object_t *obj) {
    graphics_comp_t *graphics = create_graphics(world, def);
    physics_comp_t *physics = create_physics(world, obj);
    controller_comp_t *controller = create_controller(world, obj);

    entity_t *entity
        = world->create_entity(obj->position, graphics, physics, controller);
    entity->set_tag(WARP_TAG(def->is_player ? "player" : "object"));
    return entity;
}

static object_t *evaluate_definition
        ( const object_def_t *def, vec3_t pos, dir_t dir
        , warp_random_t *rand, world_t *world
        ) {
    object_t *obj = new object_t;
    obj->type = def->type;
    obj->position = pos;
    obj->health = def->health;
    obj->max_health = def->max_health;
    obj->ammo = def->ammo;

    obj->direction = evaluate_direction(dir, rand);
    obj->flags = evaluate_flags(def);

    obj->entity = create_entity(world, def, obj);

    return obj;
}

entity_t *object_factory_t::create_object_entity
            (const object_t *obj, const warp_tag_t &def_name, world_t *world) {
    const object_def_t *def = get_definition(def_name);
    if (def == NULL) {
        warp_log_e("Couldn't find definition for object: %s.", def_name.text);
        return NULL;
    }
    return create_entity(world, def, obj);
}

object_t *object_factory_t::spawn
        (warp_tag_t name, vec3_t pos, dir_t dir, warp_random_t *rand, world_t *world) {
    const object_def_t *def = get_definition(name);
    if (def == NULL) {
        warp_log_e("Couldn't find definition for object: %s.", name.text);
        return NULL;
    }
    return evaluate_definition(def, pos, dir, rand, world);
}

const object_def_t *object_factory_t::get_definition(warp_tag_t name) {
    return (object_def_t *)warp_map_tag_get(&_objects, name);
}

