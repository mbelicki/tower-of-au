#define WARP_DROP_PREFIX
#include "level.h"

#include "warp/math/utils.h"
#include "warp/world.h"
#include "warp/entity.h"
#include "warp/graphics/model.h"
#include "warp/graphics/mesh-builder.h"
#include "warp/graphics/mesh-loader.h"
#include "warp/components.h"

#include "region.h"

using namespace warp;

level_t::level_t( const tile_t *tiles, size_t width, size_t height
                , const decoration_t *decors, size_t decors_count
                ) 
        : _initialized(false)
        , _width(width)
        , _height(height)
        , _decors_count(decors_count)
        , _tiles(NULL) 
        , _decors(NULL) 
        , _entity(NULL) {
    const size_t tiles_count = _width * _height;
    const size_t tile_size = sizeof *_tiles;
    _tiles = (tile_t *) calloc(tiles_count, tile_size);
    memmove(_tiles, tiles, tiles_count * tile_size);

    if (decors) {
        _decors = (decoration_t *) calloc(_decors_count, sizeof *_decors);
        memmove(_decors, decors, _decors_count * sizeof *_decors);
    }
}

level_t::~level_t() {
    free(_tiles);
    free(_decors);
}

void level_t::set_display_position(const vec3_t pos) {
    if (_initialized == false) return;
    _entity->receive_message(MSG_PHYSICS_MOVE, pos);
}

void level_t::set_visiblity(bool visible) {
    if (_initialized == false) return;
    _entity->receive_message(MSG_GRAPHICS_VISIBLITY, (int)visible);
}

const tile_t *level_t::get_tile_at(int x, int y) const {
    if (x < 0 || x >= (int)_width) {
        warp_log_e("Cannot get tile, illegal x value.");
        return NULL;
    }
    if (y < 0 || y >= (int)_height) {
        warp_log_e("Cannot get tile, illegal y value.");
        return NULL;
    }
    
    return _tiles + (x + _width * y);
}

bool level_t::is_point_walkable(const vec3_t point) const {
    const tile_t *tile = get_tile_at(round(point.x), round(point.z));
    return tile == NULL ? false : tile->is_walkable;
}

bool level_t::scan_if_all
        ( std::function<bool(const tile_t *)> predicate
        , size_t initial_x, size_t initial_z
        , warp_dir_t direction, size_t distance
        ) const {
    const vec3_t dir = dir_to_vec3(direction);
    const int dx = round(dir.x);
    const int dz = round(dir.z);
    int x = initial_x;
    int z = initial_z;

    bool accumulator = true;
    for (size_t i = 0; i < distance; i++) {
        const tile_t *t = get_tile_at(x, z);
        if (t != NULL) {
            accumulator = predicate(t) && accumulator;
        }
        x += dx;
        z += dz;
    }
    return accumulator;
}

static void append_graphics
        ( mesh_builder_t *builder
        , resources_t *res
        , const warp_tag_t *name
        , const transforms_t *transforms
        , const region_t *owner
        ) {
    const tile_graphics_t *graphics = owner->get_tile_graphics(*name);
    if (graphics == NULL) {
        warp_log_e("Failed to get tile graphics with id: %s.", name->text);
        return;
    }
    const char *mesh_name = warp_str_value(&graphics->mesh);
    const res_id_t id = warp_resources_load(res, mesh_name);
    mesh_builder_append(builder, id, transforms);
}

static void append_tile
        ( mesh_builder_t *builder
        , resources_t *res
        , const tile_t *tile
        , size_t x, size_t y
        , const region_t *owner
        ) {
    transforms_t transforms;
    transforms_init(&transforms);
    transforms_change_position(&transforms, vec3(x, 0, y));
    if (tile->is_stairs || tile->is_walkable) {
        transforms_change_rotation(&transforms, quat_from_euler(0, PI, 0));
    }
    append_graphics(builder, res, &tile->graphics_id, &transforms, owner);
}

static int level_mesh_numer = 0;

void level_t::initialize(world_t *world, const region_t *owner) {
    if (_initialized) {
        warp_log_e("Level is already initialized.");
        return;
    }
    
    resources_t *res = world->get_resources();
    mesh_builder_t builder;
    warp_mesh_builder_init(&builder, res);

    for (int j = _height - 1; j >= 0; j--) {
        for (int i = _width - 1; i >= 0; i--) {
            const tile_t *tile = _tiles + (i + _width * j);
            append_tile(&builder, res, tile, i, j, owner);
        }
    }

    for (size_t i = 0; i < _decors_count; i++) {
        const decoration_t *decor = _decors + i;
        append_graphics( &builder, res
                       , &decor->graphics_id, &decor->transforms, owner
                       );
    }

    warp_str_t name = warp_str_format("level-%d", level_mesh_numer++);
    const res_id_t mesh_id = mesh_builder_create_resource(&builder, warp_str_value(&name));
    const res_id_t tex_id  = resources_load(res, "atlas.png");
    graphics_comp_t *graphics = world->create_graphics();

    warp_str_destroy(&name);

    model_t model;
    model_init(&model, mesh_id, tex_id);
    
    graphics->add_model(model);

    _entity = world->create_entity(vec3(0, 0, 0), graphics, NULL, NULL);
    _entity->set_tag(WARP_TAG("level"));
    
    _initialized = true;
    warp_mesh_builder_destroy(&builder);
}

static void fill_empty_room(tile_t *tiles, size_t width, size_t height) {
    if (tiles == nullptr) return;
    for (size_t i = 0; i < width; i++) {
        for (size_t j = 0; j < height; j++) {
            bool is_wall = i == 0 || j == 0 || i == width - 1 || j == height - 1;
            const size_t index = i + width * j;
			tiles[index].is_walkable = is_wall == false;
			tiles[index].is_stairs = false;
            tiles[index].spawn_probablity = 0;
            tiles[index].feature = FEAT_NONE;
        }
    }
}

extern level_t *generate_test_level() {
    const size_t width = 13;
    const size_t height = 11;
    const size_t count = width * height;

    tile_t tiles[count];
    fill_empty_room(tiles, width, height);

    tiles[2 + width * 3].is_walkable = false;
    tiles[2 + width * 2].is_walkable = false;
    tiles[3 + width * 2].is_walkable = false;
    tiles[6 + width * 4].is_walkable = false;
    tiles[8 + width * 4].is_walkable = false;

    tiles[4 + width * 2].is_walkable = false;
    tiles[4 + width * 2].is_stairs = true;

    tiles[8 + width * 1].spawn_probablity = 1;
    tiles[9 + width * 2].spawn_probablity = 1;
    tiles[3 + width * 5].spawn_probablity = 1;

    tiles[4 + width * 7].spawn_probablity = 1;

    tiles[7 + width * 7].feature = FEAT_BUTTON;
    tiles[7 + width * 7].feat_target_id = 7 + width * 4;

    tiles[7 + width * 4].feature = FEAT_DOOR;

    return new (std::nothrow) level_t(tiles, width, height, NULL, 0);
}

extern level_t *generate_random_level(warp_random_t *random) {
    const size_t width = 13;
    const size_t height = 11;
    const size_t count = width * height;

    tile_t tiles[count];
    fill_empty_room(tiles, width, height);
    for (size_t i = 0; i < width; i++) {
        for (size_t j = 0; j < height; j++) {
            const size_t index = i + width * j;
            bool is_wall = tiles[index].is_walkable == false;

            if (is_wall == false) {
                const bool is_floor = warp_random_boolean(random);
                tiles[index].is_walkable = is_floor;
                if (is_floor && warp_random_float(random) < 0.05f) {
                    tiles[index].spawn_probablity = warp_random_float(random);
                }  
            }
        }
    }

    tiles[6 + width * 0].is_walkable = true;
    tiles[6 + width * 1].is_walkable = true;

    tiles[6 + width * 10].is_walkable = true;
    tiles[6 + width *  9].is_walkable = true;

    tiles[0 + width * 5].is_walkable = true;
    tiles[1 + width * 5].is_walkable = true;

    tiles[12 + width * 5].is_walkable = true;
    tiles[11 + width * 5].is_walkable = true;

    return new (std::nothrow) level_t(tiles, width, height, NULL, 0);
}
