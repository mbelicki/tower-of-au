#define WARP_DROP_PREFIX
#include "region.h"

#include <cstring> /* memmove */
#include <map>

#include "warp/utils/io.h"
#include "warp/renderer.h"

#include "libs/parson/parson.h"

#include "level.h"

using namespace warp;

static void destroy_portal(void *raw_portal) {
    portal_t *portal = (portal_t *)raw_portal;
    warp_str_destroy(&portal->region_name);
}

static void destroy_tile_graphics(void *raw_graphics) {
    tile_graphics_t *graphics = (tile_graphics_t *)raw_graphics;
    warp_str_destroy(&graphics->mesh);
    warp_str_destroy(&graphics->texture);
}

region_t::region_t(level_t **levels, size_t width, size_t height)
        : _initialized(false)
        , _width(width)
        , _height(height)
        , _levels(nullptr)
        , _portals() 
        , _graphics() {
    const size_t tiles_count = _width * _height;
    _levels = new (std::nothrow) level_t * [tiles_count];
    memmove(_levels, levels, tiles_count * sizeof (level_t *));

    _graphics = warp_array_create_typed(tile_graphics_t, 16, destroy_tile_graphics);
    _portals  = warp_array_create_typed(portal_t, 16, destroy_portal);
    
    light_settings_t defaults;
    fill_default_light_settings(&defaults);
    _lighting.sun_color = defaults.sun_color;
    _lighting.sun_direction = defaults.sun_direction;
    _lighting.ambient_color = defaults.ambient_color;
}

region_t::~region_t() {
    const size_t tiles_count = _width * _height;
    for (size_t i = 0; i < tiles_count; i++)
        delete _levels[i];
    delete [] _levels;
    warp_array_destroy(&_graphics);
    warp_array_destroy(&_portals);
}

void region_t::initialize(world_t *world) {
    if (world == NULL) { 
        warp_log_e("Cannot initialize region, world is null.");
        return;
    }

    for (size_t i = 0; i < _width; i++) {
        for (size_t j = 0; j < _height; j++) {
            level_t *level = _levels[i + _width * j];
            if (level->is_initialized() == false) {
                level->initialize(world, this);
            }
            level->set_display_position(vec3(13 * i, 0, 11 * j));
        }
    }
}

bool region_t::add_portal
            ( const char *region_name, size_t level_x, size_t level_z
            , size_t tile_x, size_t tile_z
            ) {
    portal_t portal;

    portal.region_name = warp_str_create(region_name);
    portal.level_x = level_x;
    portal.level_z = level_z;
    portal.tile_x = tile_x;
    portal.tile_z = tile_z;

    return warp_array_append(&_portals, &portal, 1);
}

bool region_t::add_tile_graphics
        (const warp_tag_t &name, const char *mesh, const char *tex) {
    const tile_graphics_t g = { name, warp_str_create(mesh), warp_str_create(tex) };
    return warp_array_append(&_graphics, &g, 1);
}

void region_t::set_lighting(const region_lighting_t *lighting) {
    if (lighting != nullptr) {
        _lighting = *lighting;
    }
}

const portal_t *region_t::get_portal(size_t id) {
    return (const portal_t *) warp_array_get(&_portals, id);
}

const tile_graphics_t *region_t::get_tile_graphics(const warp_tag_t &tag) const {
    const size_t count = warp_array_get_size(&_graphics);
    for (size_t i = 0; i < count; i++) {
        const tile_graphics_t *tg = (const tile_graphics_t *)warp_array_get(&_graphics, i);
        if (warp_tag_equals(&tg->name, &tag)) return tg;
    }
    return NULL;
}

void region_t::animate_transition
        (size_t new_x, size_t new_z, size_t old_x, size_t old_z, float k) {
    const int ddx = (int)new_x - (int)old_x;
    const int ddz = (int)new_z - (int)old_z;

    for (size_t i = 0; i < _width; i++) {
        for (size_t j = 0; j < _height; j++) {
            level_t *level = _levels[i + _width * j];

            const int dx = (int)i - (int)old_x;
            const int dz = (int)j - (int)old_z;
            const bool visible = abs(dx) <= 2 && abs(dz) <= 2;

            const float x = 13 * (dx - k * ddx);
            const float z = 11 * (dz - k * ddz);

            level->set_display_position(vec3(x, 0, z));
            level->set_visiblity(visible);
        }
    }
}

void region_t::change_display_positions(size_t current_x, size_t current_z) {
    for (size_t i = 0; i < _width; i++) {
        for (size_t j = 0; j < _height; j++) {
            level_t *level = _levels[i + _width * j];

            const int dx = (int)i - (int)current_x;
            const int dz = (int)j - (int)current_z;
            const bool visible = abs(dx) <= 1 && abs(dz) <= 1;

            level->set_display_position(vec3(13 * dx, 0, 11 * dz));
            level->set_visiblity(visible);
        }
    }
}

level_t *region_t::get_level_at(size_t x, size_t y) const {
    if (x >= _width || y >= _height) {
        warp_log_e("Cannot obtain level at: %zu, %zu.", x, y);
        return NULL;
    }
    return _levels[x + _width * y];
}

region_t *generate_random_region(warp_random_t *random) {
    bool local_random = false;
    if (random == nullptr) {
        random = warp_random_create(209);
        local_random = true;
    }

    const size_t width = 9;
    const size_t height = 9;
    level_t * levels[width * height];
    for (size_t i = 0; i < width; i++) {
        for (size_t j = 0; j < height; j++) {
            levels[i + width * j] = generate_random_level(random);
        }
    }
    
    region_t *region = new region_t(levels, width, height);
    if (local_random) {
        warp_random_destroy(random);
    }
    return region;
}

static feature_type_t feature_from_string(const char *type) {
    if (type == nullptr) return FEAT_NONE;
    if (strncmp(type, "FEAT_DOOR", 9) == 0) {
        return FEAT_DOOR;
    } else if (strncmp(type, "FEAT_BUTTON", 11) == 0) {
        return FEAT_BUTTON;
    } else if (strncmp(type, "FEAT_SPIKES", 11) == 0) {
        return FEAT_SPIKES;
    } else if (strncmp(type, "FEAT_BREAKABLE_FLOOR", 20) == 0) {
        return FEAT_BREAKABLE_FLOOR;
    } 
    return FEAT_NONE;
}

static dir_t parse_direction(const char *dir) {
    if (dir == nullptr) return DIR_NONE;
    if (strncmp(dir, "x_plus", 7) == 0) {
        return DIR_X_PLUS;
    } else if (strncmp(dir, "x_minus", 8) == 0) {
        return DIR_X_MINUS;
    } else if (strncmp(dir, "z_plus", 7) == 0) {
        return DIR_Z_PLUS;
    } else if (strncmp(dir, "z_minus", 8) == 0) {
        return DIR_Z_MINUS;
    }
    return DIR_NONE;
}

static void fill_default_tile(tile_t *tile) {
    if (tile == nullptr) return;

    tile->is_walkable = true;
    tile->is_stairs = false;
    tile->spawn_probablity = 0;
    tile->object_dir = DIR_NONE;
    tile->feature = FEAT_NONE;
    tile->feat_target_id = 0;
}

static tile_t parse_tile(JSON_Object *tile) {
    tile_t result;
    fill_default_tile(&result);
    
    if (json_object_get_value(tile, "walkable") != nullptr) {
        result.is_walkable = json_object_get_boolean(tile, "walkable");
    }
    if (json_object_get_value(tile, "stairs") != nullptr) {
        result.is_stairs = json_object_get_boolean(tile, "stairs");
    }
    if (json_object_get_value(tile, "object") != nullptr) {
        result.object_id = WARP_TAG(json_object_get_string(tile, "object"));
    }
    if (json_object_get_value(tile, "objectDirection") != nullptr) {
        const char *dir = json_object_get_string(tile, "objectDirection");
        result.object_dir = parse_direction(dir);
    }
    if (json_object_get_value(tile, "feature") != nullptr) {
        const char *type = json_object_get_string(tile, "feature");
        result.feature = feature_from_string(type);
    }
    if (json_object_get_value(tile, "spawnRate") != nullptr) {
        result.spawn_probablity = json_object_dotget_number(tile, "spawnRate");
    }
    if (json_object_get_value(tile, "target") != nullptr) {
        const size_t x = json_object_dotget_number(tile, "target.x");
        const size_t y = json_object_dotget_number(tile, "target.y");
        result.feat_target_id = x + 13 * y;
    }
    if (json_object_get_value(tile, "portalId") != nullptr) {
        result.portal_id = json_object_dotget_number(tile, "portalId");
    }
    if (json_object_get_value(tile, "graphics") != nullptr) {
        result.graphics_id = WARP_TAG(json_object_dotget_string(tile, "graphics"));
    }
    
    return result;
}

static char get_tile_symbol(JSON_Object *tile) {
    const char *symbol = json_object_dotget_string(tile, "symbol");
    if (symbol == NULL) return '\0';
    return symbol[0];
}

static void fill_tiles_map(std::map<char, tile_t> *map, JSON_Array *tiles) {
    if (tiles == nullptr) return;
    
    const size_t count = json_array_get_count(tiles);
    for (size_t i = 0; i < count; i++) {
        JSON_Object *tile = json_array_get_object(tiles, i);
        const char symbol = get_tile_symbol(tile);
        if (symbol != '\0'){
            map->insert(std::make_pair(symbol, parse_tile(tile)));
        }
    }
}

static void prase_level_data
        ( tile_t *tiles, size_t width, size_t height
        , JSON_Array *data, std::function<tile_t(char)> mapper
        ) {
    /* TODO: validate actual data size */
    for (size_t j = 0; j < height; j++) {
        const char *row = json_array_get_string(data, j);
        for (size_t i = 0; i < width; i++) {
            tiles[i + width * j] = mapper(row[i]);
        }
    }
}

static void parse_level_decorations(JSON_Array *decors, decoration_t *ds) {
    for (size_t i = 0; i < json_array_get_count(decors); i++) {
        JSON_Object *decor = json_array_get_object(decors, i);
        decoration_t *d = ds + i;
        transforms_init(&d->transforms);

        d->graphics_id = WARP_TAG(json_object_get_string(decor, "graphics"));

        vec3_t position;
        position.x = json_object_dotget_number(decor, "position.x");
        position.y = json_object_dotget_number(decor, "position.y");
        position.z = json_object_dotget_number(decor, "position.z");

        vec3_t rotation;
        rotation.x = json_object_dotget_number(decor, "rotation.x");
        rotation.y = json_object_dotget_number(decor, "rotation.y");
        rotation.z = json_object_dotget_number(decor, "rotation.z");

        const quat_t rot = quat_from_euler(rotation.x, rotation.y, rotation.z);
        transforms_change_position(&d->transforms, position);
        transforms_change_rotation(&d->transforms, rot);
    }
}

static void parse_level
        ( level_t **parsed, JSON_Object *level
        , const std::map<char, tile_t> &global_map
        ) {
    const size_t width = 13;
    const size_t height = 11;

    std::map<char, tile_t> local_map;
    JSON_Array *local_tiles = json_object_dotget_array(level, "tiles");
    fill_tiles_map(&local_map, local_tiles);

    JSON_Array *data = json_object_dotget_array(level, "data");

    std::function<tile_t(char)> mapper
            = [&local_map, &global_map](char symbol) {
        tile_t result; fill_default_tile(&result);
        if (local_map.find(symbol) != local_map.end()) {
            result = local_map.at(symbol);
        } else if (global_map.find(symbol) != global_map.end()) {
            result = global_map.at(symbol);
        }
        return result;
    };

    tile_t tiles[width * height];
    prase_level_data(tiles, width, height, data, mapper);

    size_t decors_count = 0;
    decoration_t *decorations = NULL;
    JSON_Array *decors = json_object_get_array(level, "decorations");
    if (decors) { 
        decors_count = json_array_get_count(decors);
        decorations = new decoration_t[decors_count];
        parse_level_decorations(decors, decorations);
    }

    *parsed = new level_t(tiles, width, height, decorations, decors_count);
}

static void add_graphics(region_t *region, JSON_Array *graphics) {
    if (graphics == nullptr) return;

    const size_t count = json_array_get_count(graphics);
    for (size_t i = 0; i < count; i++) {
        JSON_Object *g = json_array_get_object(graphics, i);

        const char *name = json_object_get_string(g, "name");
        const char *mesh = json_object_get_string(g, "mesh");
        const char *texture = json_object_get_string(g, "texture");

        region->add_tile_graphics(WARP_TAG(name), mesh, texture);
    }
}

static void add_portals(region_t *region, JSON_Array *portals) {
    if (portals == nullptr) return;

    const size_t count = json_array_get_count(portals);
    for (size_t i = 0; i < count; i++) {
        JSON_Object *portal = json_array_get_object(portals, i);

        const char *name = json_object_get_string(portal, "region");
        const size_t level_x = json_object_dotget_number(portal, "level.x");
        const size_t level_z = json_object_dotget_number(portal, "level.y");
        const size_t tile_x = json_object_dotget_number(portal, "tile.x");
        const size_t tile_z = json_object_dotget_number(portal, "tile.y");

        region->add_portal(name, level_x, level_z, tile_x, tile_z);
    }
}

static void parse_vec3(const JSON_Object *vector, vec3_t *output) {
    if (vector == nullptr) {
        warp_log_e("Cannot parse empty vector.");
        return;
    }
    if (output == nullptr) {
        warp_log_e("Cannot parse empty output.");
        return;
    }

    output->x = json_object_get_number(vector, "x");
    output->y = json_object_get_number(vector, "y");
    output->z = json_object_get_number(vector, "z");
}

static void parse_color(const JSON_Object *color, vec3_t *output) {
    if (color == nullptr) {
        warp_log_e("Cannot parse empty color.");
        return;
    }
    if (output == nullptr) {
        warp_log_e("Cannot parse empty output.");
        return;
    }

    output->x = json_object_get_number(color, "r");
    output->y = json_object_get_number(color, "g");
    output->z = json_object_get_number(color, "b");
}

static void parse_lighting(JSON_Object *object, region_lighting_t *lighting) {
    if (lighting == nullptr) {
        warp_log_e("Cannot parse region lighting, null lighting.");
    }
    if (object == nullptr) {
        warp_log_e("Cannot parse region lighting, null object.");
    }

    parse_color(json_object_get_object(object, "sunColor"), &lighting->sun_color);
    parse_vec3(json_object_get_object(object, "sunDirection"), &lighting->sun_direction);
    parse_color(json_object_get_object(object, "ambientColor"), &lighting->ambient_color);
}

static region_t *parse_json(const JSON_Object *root) {
    const size_t width = json_object_get_number(root, "width");
    const size_t height = json_object_get_number(root, "height");
    const size_t count = width * height;

    JSON_Array *levels = json_object_get_array(root, "levels");
    if (json_array_get_count(levels) != count) {
        warp_log_e( "Cannot parse region: width, height and size of"
                    " 'levels' are inconsitent."
                  );
        return nullptr;
    }

    std::map<char, tile_t> global_map;
    JSON_Array *global_tiles = json_object_get_array(root, "tiles");
    fill_tiles_map(&global_map, global_tiles);

    level_t **parsed_levels = new level_t * [count];
    for (size_t i = 0; i < count; i++) {
        JSON_Object *level = json_array_get_object(levels, i);
        parse_level(parsed_levels + i, level, global_map);
    }

    region_t *region = new region_t(parsed_levels, width, height);
    delete [] parsed_levels;
    
    JSON_Array *portals = json_object_get_array(root, "portals");
    add_portals(region, portals);

    JSON_Array *graphics = json_object_get_array(root, "graphics");
    add_graphics(region, graphics);

    JSON_Object *lighting = json_object_get_object(root, "lighting");
    region_lighting_t lights = *region->get_region_lighting();
    parse_lighting(lighting, &lights);
    region->set_lighting(&lights);
    
    return region;
}

extern region_t *load_region(const char *name) {
    region_t *result = NULL;
    const char *filepath = NULL;
    const char *content = NULL;

    warp_array_t bytes = { NULL };

    warp_str_t path = warp_str_format("assets/levels/%s", name);
    warp_str_t out_path = { NULL };
    
    warp_result_t find_result;
    warp_result_t read_result;

    JSON_Value *root_value = NULL;
    const JSON_Object *root = NULL;

    find_result = find_path(&path, &out_path);
    if (WARP_FAILED(find_result)) {
        warp_result_log("Failed to find region file path", &find_result);
        warp_result_destory(&find_result);
        goto cleanup;
    }

    filepath = warp_str_value(&out_path);
    read_result = read_file(filepath, &bytes);
    if (WARP_FAILED(read_result)) {
        warp_result_log("Failed to read region file", &read_result);
        warp_result_destory(&read_result);
        goto cleanup;
    }

    content = (char *) warp_array_get(&bytes, 0);
    root_value = json_parse_string(content);
    if (json_value_get_type(root_value) != JSONObject) {
       warp_log_e("Cannot parse %s: root element is not an object.", path);
       goto cleanup;
    }
    root = json_value_get_object(root_value);
    result = parse_json(root);
    
cleanup:
    warp_str_destroy(&path);
    warp_str_destroy(&out_path);
    warp_array_destroy(&bytes);
    json_value_free(root_value);

    return result;
}

