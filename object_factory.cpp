#include "object_factory.h"

#include "warp/io.h"
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
    int           ammo;
    bool          can_shoot;
    bool          is_player;

    warp_str_t    mesh_name;
    warp_str_t    texture_name;
};

static bool has_json_member(JSON_Object *obj, const char *member_name) {
    return json_object_get_value(obj, member_name) != nullptr;
}

object_factory_t::object_factory_t() {
}

object_factory_t::~object_factory_t() {
    typedef std::map<tag_t, object_def_t *>::iterator map_it_t;
    for (map_it_t it = _objects.begin(); it != _objects.end(); it++) {
        object_def_t *def = it->second;
        str_destroy(def->mesh_name);
        str_destroy(def->texture_name);
        delete def;
    }
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

static int parse_health(JSON_Object *obj) {
    if (has_json_member(obj, "health") == false) {
        return 1;
    }
    return (int)json_object_get_number(obj, "health");
}

static int parse_ammo(JSON_Object *obj) {
    if (has_json_member(obj, "ammo") == false) {
        return 0;
    }
    return (int)json_object_get_number(obj, "ammo");
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
    
    def->mesh_name = str_create(mesh_name == nullptr ? "npc.obj" : mesh_name);
    def->texture_name = str_create(tex_name == nullptr ? "missing.png" : tex_name);
}

static void parse_definition
        (JSON_Object *object, std::map<tag_t, object_def_t *> *objects) {
    if (json_object_get_value(object, "name") == nullptr) {
        warp_log_e("Failed to parse object definition, missing name.");
        return;
    }

    const tag_t name = json_object_get_string(object, "name");
    object_def_t *def = new object_def_t;
    def->type          = parse_type(object);
    def->movement_type = parse_movement(object);
    def->health        = parse_health(object);
    def->ammo          = parse_ammo(object);
    def->can_shoot     = parse_bool_flag(object, "canShoot");
    def->is_player     = parse_bool_flag(object, "playerAvatar");
    
    parse_graphics(def, object);

    objects->insert(std::make_pair(name, def));
}

bool object_factory_t::load_definitions(const char *filename) {
    char path[1024]; 
    maybeunit_t path_found = find_path(filename, "assets/data", path, 1024);
    if (path_found.failed()) {
        path_found.log_failure();
        return false;
    }

    const JSON_Value *root_value = json_parse_file(path);
    if (json_value_get_type(root_value) != JSONObject) {
       warp_log_e("Cannot parse %s: root element is not an object.", path);
    }
    const JSON_Object *root = json_value_get_object(root_value);
    JSON_Array *objects = json_object_dotget_array(root, "objects");
    for (size_t i = 0; i < json_array_get_count(objects); i++) {
        JSON_Object *object = json_array_get_object(objects, i);
        parse_definition(object, &_objects);
    }

    return true;
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
    if (def->is_player) {
        flags |= FOBJ_PLAYER_AVATAR;
    }
    flags |= movement_to_flag(def->movement_type);
    return flags;
}

static graphics_comp_t *create_graphics
        (world_t *world, const object_def_t *def) {
    const char *mesh_name = str_value(def->mesh_name);
    const char *tex_name = str_value(def->texture_name);
    maybe_t<graphics_comp_t *> graphics
        = create_single_model_graphics(world, mesh_name, tex_name);
    return VALUE(graphics);
}

static controller_comp_t *create_controller
        (world_t *world, const object_t *obj) {
    const bool confirm_moves = (obj->flags & FOBJ_PLAYER_AVATAR) != 0;
    maybe_t<controller_comp_t *> controller
        = create_character_controller(world, confirm_moves);
    return VALUE(controller);
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
        physics->_velocity = vec2(0, 0);
        physics->_bounds = rectangle_t(vec2(0, 0), size);
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
    entity->set_tag(def->is_player ? "player" : "object");
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
    obj->ammo = def->ammo;

    obj->direction = evaluate_direction(dir, rand);
    obj->flags = evaluate_flags(def);

    obj->entity = create_entity(world, def, obj);

    return obj;
}

object_t *object_factory_t::spawn
        (tag_t name, vec3_t pos, dir_t dir, warp_random_t *rand, world_t *world) {
    const object_def_t *def = get_definition(name);
    if (def == nullptr) {
        warp_log_e("Couldn't find definition for object: %s.", name.get_text());
        return nullptr;
    }
    return evaluate_definition(def, pos, dir, rand, world);
}

const object_def_t *object_factory_t::get_definition(tag_t name) {
    const std::map<warp::tag_t, object_def_t *>::iterator it = _objects.find(name);
    if (it == _objects.end()) { 
        return nullptr;
    }
    return it->second;
}

