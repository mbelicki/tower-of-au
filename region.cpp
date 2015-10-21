#include "region.h"

#include <cstring> /* memmove */
#include <map>

#include "libs/parson/parson.h"

#include "level.h"
#include "random.h"

using namespace warp;

region_t::region_t(level_t **levels, size_t width, size_t height)
        : _initialized(false)
        , _width(width)
        , _height(height)
        , _levels(nullptr) {
    const size_t tiles_count = _width * _height;
    _levels = new (std::nothrow) level_t * [tiles_count];
    memmove(_levels, levels, tiles_count * sizeof (level_t *));
}

region_t::~region_t() {
    delete [] _levels;
}

maybeunit_t region_t::initialize(world_t *world) {
    for (size_t i = 0; i < _width; i++) {
        for (size_t j = 0; j < _height; j++) {
            level_t *level = _levels[i + _width * j];
            if (level->is_initialized() == false)
                level->initialize(world);
            level->set_display_position(vec3(13 * i, 0, 11 * j));
        }
    }
    return unit;
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

maybe_t<level_t *> region_t::get_level_at(size_t x, size_t y) const {
    if (x >= _width)  return nothing<level_t *>("Illegal x value.");
    if (y >= _height) return nothing<level_t *>("Illegal y value.");
    return _levels[x + _width * y];
}

region_t *generate_random_region(random_t *random) {
    random_t local_random;
    if (random == nullptr) {
        random = &local_random;
    }

    const size_t width = 9;
    const size_t height = 9;
    level_t * levels[width * height];
    for (size_t i = 0; i < width; i++) {
        for (size_t j = 0; j < height; j++) {
            levels[i + width * j] = generate_random_level(random);
        }
    }
    
    return new region_t(levels, width, height);
}

static object_type_t object_from_string(const char *type) {
    if (type == nullptr) return OBJ_NONE;
    if (strncmp(type, "OBJ_NPC_GRUNT", 9) == 0) {
        return OBJ_CHARACTER;
    } else if (strncmp(type, "OBJ_BOULDER", 11) == 0) {
        return OBJ_BOULDER;
    }

    return OBJ_NONE;
}

static void fill_default_tile(tile_t *tile) {
    if (tile == nullptr) return;

    tile->is_walkable = true;
    tile->is_stairs = false;
    tile->spawn_probablity = 0;
    tile->spawned_object = OBJ_NONE;
    tile->feature = FEAT_NONE;
    tile->feat_target_id = 0;
}

static tile_t parse_tile(JSON_Object *tile) {
    tile_t result;
    fill_default_tile(&result);
    
    if (json_object_get_value(tile, "walkable") != nullptr) {
        result.is_walkable = json_object_get_boolean(tile, "walkable");
    }
    if (json_object_get_value(tile, "object") != nullptr) {
        const char *type = json_object_get_string(tile, "object");
        puts("checkin' object");
        result.spawned_object = object_from_string(type);
    }
    if (json_object_get_value(tile, "spawnRate") != nullptr) {
        result.spawn_probablity = json_object_dotget_number(tile, "spawnRate");
    }
    
    return result;
}

static maybe_t<char> get_tile_symbol(JSON_Object *tile) {
    const char *symbol = json_object_dotget_string(tile, "symbol");
    if (symbol == nullptr) return nothing<char>("No symbol.");
    return symbol[0];
}

static void fill_tiles_map(std::map<char, tile_t> *map, JSON_Array *tiles) {
    if (tiles == nullptr) return;
    
    const size_t count = json_array_get_count(tiles);
    for (size_t i = 0; i < count; i++) {
        JSON_Object *tile = json_array_get_object(tiles, i);
        get_tile_symbol(tile).with_value([map, tile](char symbol){
            map->insert(std::make_pair(symbol, parse_tile(tile)));
        });
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
        }
        if (global_map.find(symbol) != global_map.end()) {
            result = global_map.at(symbol);
        }
        return result;
    };

    tile_t tiles[width * height];
    prase_level_data(tiles, width, height, data, mapper);

    *parsed = new level_t(tiles, width, height);
}

maybe_t<region_t *> load_region(const char *path) {
    const JSON_Value *root_value = json_parse_file(path);
    if (json_value_get_type(root_value) != JSONObject) {
        return nothing<region_t *>
            ("Cannot parse %s: root element is not an object.", path);
    }
    const JSON_Object *root = json_value_get_object(root_value);

    const size_t width = json_object_dotget_number(root, "width");
    const size_t height = json_object_dotget_number(root, "height");
    const size_t count = width * height;

    JSON_Array *levels = json_object_dotget_array(root, "levels");
    if (json_array_get_count(levels) != count) {
        return nothing<region_t *>
            ("Cannot parse %s: width, height and size of 'levels' are inconsitent.", path);
    }

    std::map<char, tile_t> global_map;
    JSON_Array *global_tiles = json_object_dotget_array(root, "tiles");
    fill_tiles_map(&global_map, global_tiles);

    level_t **parsed_levels = new level_t * [count];
    for (size_t i = 0; i < count; i++) {
        JSON_Object *level = json_array_get_object(levels, i);
        parse_level(parsed_levels + i, level, global_map);
    }

    region_t *region = new region_t(parsed_levels, width, height);
    delete parsed_levels;

    return region;
}

