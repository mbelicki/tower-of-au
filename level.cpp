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

maybe_t<const tile_t *> level_t::get_tile_at(size_t x, size_t y) const {
    if (x >= _width) {
        return nothing<const tile_t *>("Illegal x value.");
    }
    if (y >= _height) {
        return nothing<const tile_t *>("Illegal y value.");
    }
    
    return _tiles + (x + _width * y);
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

extern level_t *generate_test_level() {
    const size_t width = 13;
    const size_t height = 11;
    const size_t count = width * height;

    tile_t tiles[count];
    for (size_t i = 0; i < width; i++) {
        for (size_t j = 0; j < height; j++) {
            bool is_wall = i == 0 || j == 0 || i == width - 1 || j == height - 1;
            const size_t index = i + width * j;
			tiles[index].is_walkable = is_wall == false;
			tiles[index].is_stairs = false;
            tiles[index].spawn_probablity = 0;
            tiles[index].boulder_probability = 0;
        }
    }

    tiles[2 + width * 3].is_walkable = false;
    tiles[2 + width * 2].is_walkable = false;
    tiles[3 + width * 2].is_walkable = false;

    tiles[4 + width * 2].is_walkable = false;
    tiles[4 + width * 2].is_stairs = true;

    tiles[8 + width * 1].spawn_probablity = 1;
    tiles[9 + width * 2].spawn_probablity = 1;
    tiles[3 + width * 5].spawn_probablity = 1;

    tiles[4 + width * 7].boulder_probability = 1;

    return new (std::nothrow) level_t(tiles, width, height);
}

extern level_t *generate_random_level(random_t *random) {
    const size_t width = 13;
    const size_t height = 11;
    const size_t count = width * height;

    tile_t tiles[count];
    for (size_t i = 0; i < width; i++) {
        for (size_t j = 0; j < height; j++) {
            bool is_wall = i == 0 || j == 0 || i == width - 1 || j == height - 1;
            const size_t index = i + width * j;
			tiles[index].is_walkable = false;
			tiles[index].is_stairs = false;
            tiles[index].spawn_probablity = 0;
            tiles[index].boulder_probability = 0;

            if (is_wall == false) {
                const bool is_floor = random->uniform_from_range(0, 10) >= 1; 
                tiles[index].is_walkable = is_floor;
                if (is_floor && random->uniform_zero_to_one() < 0.05f) {
                    tiles[index].spawn_probablity
                        = random->uniform_zero_to_one();
                } else if (is_floor && random->uniform_zero_to_one() < 0.06f) {
                    tiles[index].boulder_probability
                        = random->uniform_zero_to_one();
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
