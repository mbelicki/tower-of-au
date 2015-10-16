#include "level.h"

#include "warp/world.h"
#include "warp/entity.h"
#include "warp/meshbuilder.h"
#include "warp/meshmanager.h"
#include "warp/components.h"

#include "random.h"

using namespace warp;

level_t::level_t(const tile_t *tiles, size_t width, size_t height) 
        : _initialized(false)
        , _width(width)
        , _height(height)
        , _tiles(nullptr) {
    const size_t tiles_count = _width * _height;
    /* because memmove does not interact well with C++ constructors and
     * destructors */
    _tiles = new (std::nothrow) tile_t[tiles_count];
    for (size_t i = 0; i < tiles_count; i++)
        _tiles[i] = tiles[i];
}

level_t::~level_t() {
    delete [] _tiles;
}

void level_t::set_display_position(const warp::vec3_t pos) {
    if (_initialized == false) return;
    _entity->receive_message(MSG_PHYSICS_MOVE, pos);
}

maybe_t<const tile_t *> level_t::get_tile_at(size_t x, size_t y) const {
    if (x >= _width) {
        return nothing<const tile_t *>("Illegal x value.");
    }
    if (y >= _height) {
        return nothing<const tile_t *>("Illegal y value.");
    }
    
    return _tiles + (x + _width * y);
}

bool level_t::is_point_walkable(const vec3_t point) const {
    const size_t x = round(point.x);
    const size_t z = round(point.z);
    maybe_t<const tile_t *> maybe_tile = get_tile_at(x, z);
    if (maybe_tile.failed()) return false;

    const tile_t *tile = VALUE(maybe_tile);
    return tile->is_walkable;
}

bool level_t::scan_if_all
        ( std::function<bool(const tile_t *)> predicate
        , size_t initial_x, size_t initial_z
        , warp::dir_t direction, size_t distance
        ) const {
    const vec3_t dir = dir_to_vec3(direction);
    const int dx = round(dir.x);
    const int dz = round(dir.z);
    int x = initial_x;
    int z = initial_z;

    bool accumulator = true;
    for (size_t i = 0; i < distance; i++) {
        get_tile_at(x, z).with_value([&](const tile_t *t) {
            accumulator = predicate(t) && accumulator;
        });
        x += dx;
        z += dz;
    }
    return accumulator;
}

static maybeunit_t append_tile
        ( meshbuilder_t *builder
        , meshmanager_t *meshes
        , const tile_t &tile
        , size_t x, size_t y
        ) {
    transforms_t transforms;
    transforms.change_position(vec3(x, 0, y));
    if (tile.is_stairs) {
        transforms.change_rotation(quat_from_euler(0, PI, 0));
    } else if (tile.is_walkable) {
        transforms.change_rotation(quat_from_euler(0, PI, 0));
    }

    const char *mesh_name = tile.is_stairs 
                          ? "stairs.obj" 
                          : (tile.is_walkable ? "floor.obj" : "wall.obj");
    const maybe_t<mesh_id_t> maybe_id = meshes->add_mesh(mesh_name);
    MAYBE_RETURN(maybe_id, unit_t, "Failed to get tile mesh:");

    const mesh_id_t id = VALUE(maybe_id);
    builder->append_mesh(*meshes, id, transforms);

    return unit;
}

maybeunit_t level_t::initialize(world_t *world) {
    if (_initialized) return nothing<unit_t>("Already initialized.");
    
    meshmanager_t *meshes = world->get_resources().meshes;
    meshbuilder_t builder;

    for (size_t i = 0; i < _width; i++) {
        for (size_t j = 0; j < _height; j++) {
            const size_t index = i + _width * j;
            maybeunit_t result
                = append_tile(&builder, meshes, _tiles[index], i, j);
            MAYBE_RETURN(result, unit_t, "Failed to append a tile mesh:");
        }
    }

    maybe_t<mesh_id_t> mesh_id = meshes->add_mesh_from_builder(builder);
    MAYBE_RETURN(mesh_id, unit_t, "Failed to create level mesh:");

    maybe_t<graphics_comp_t *> graphics = world->create_graphics();
    MAYBE_RETURN(graphics, unit_t, "Failed to create level graphics:");

    model_t model;
    model.initialize(VALUE(mesh_id), 0);
    
    (VALUE(graphics))->add_model(model);

    _entity = world->create_entity(vec3(0, 0, 0), VALUE(graphics), nullptr, nullptr);
    _entity->set_tag("level");
    
    _initialized = true;
    return unit;
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
            tiles[index].spawned_object = OBJ_NONE;
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
    tiles[8 + width * 1].spawned_object = OBJ_CHARACTER;
    tiles[9 + width * 2].spawn_probablity = 1;
    tiles[9 + width * 2].spawned_object = OBJ_CHARACTER;
    tiles[3 + width * 5].spawn_probablity = 1;
    tiles[3 + width * 5].spawned_object = OBJ_CHARACTER;

    tiles[4 + width * 7].spawn_probablity = 1;
    tiles[4 + width * 7].spawned_object = OBJ_BOULDER;

    tiles[7 + width * 7].feature = FEAT_BUTTON;
    tiles[7 + width * 7].feat_target_id = 7 + width * 4;

    tiles[7 + width * 4].feature = FEAT_DOOR;

    return new (std::nothrow) level_t(tiles, width, height);
}

extern level_t *generate_random_level(random_t *random) {
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
                const bool is_floor = random->uniform_from_range(0, 10) >= 1; 
                tiles[index].is_walkable = is_floor;
                if (is_floor && random->uniform_zero_to_one() < 0.05f) {
                    tiles[index].spawn_probablity
                        = random->uniform_zero_to_one();
                    tiles[index].spawned_object
                        = random->uniform_zero_to_one() < 0.7f
                        ? OBJ_CHARACTER
                        : OBJ_BOULDER
                        ;
                }  
            }
        }
    }

    const int stair_x = random->uniform_from_range(1, width - 3);
    const int stair_y = random->uniform_from_range(1, height - 2);
    
    tiles[stair_x + width * stair_y].is_stairs = true;
    tiles[stair_x + width * stair_y].spawn_probablity = 0;

    tiles[stair_x + 1 + width * stair_y].is_walkable = true;
    tiles[stair_x + width * (stair_y + 1)].is_walkable = true;

    return new (std::nothrow) level_t(tiles, width, height);
}
