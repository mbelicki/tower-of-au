#include "level.h"

#include "warp/world.h"
#include "warp/entity.h"
#include "warp/entity-helpers.h"

using namespace warp;

level_t::level_t(const tile_t *tiles, size_t width, size_t height) 
        : _initialized(false)
        , _width(width)
        , _height(height)
        , _entities(nullptr)
        , _tiles(nullptr) {
    const size_t tiles_count = _width * _height;
    /* because memmove does not interact well with C++ constructors and
     * destructors */
    _tiles = new (std::nothrow) tile_t[tiles_count];
    for (size_t i = 0; i < tiles_count; i++)
        _tiles[i] = tiles[i];

    _entities = new (std::nothrow) entity_t * [tiles_count]; 
    for (size_t i = 0; i < tiles_count; i++)
        _entities[i] = nullptr;
}

level_t::~level_t() {
    delete [] _tiles;
    delete [] _entities;
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

static maybe_t<entity_t *> create_tile
        (const tile_t &tile, size_t x, size_t y, world_t *world) {
    const char *mesh_name = tile.is_stairs 
                          ? "stairs.obj" 
                          : (tile.is_walkable ? "floor.obj" : "wall.obj");
    const char *tex_name  = "missing.png";

    maybe_t<graphics_comp_t *> graphics
        = create_single_model_graphics(world, mesh_name, tex_name);
    MAYBE_RETURN(graphics, entity_t *, "Failed to create tile:");
    
    const vec3_t position = vec3(x, 0, y);
    entity_t *entity = world->create_entity(position, VALUE(graphics), nullptr, nullptr);
    
    return entity;
}

maybeunit_t level_t::initialize(world_t *world) {
    if (_initialized) return nothing<unit_t>("Already initialized.");

    for (size_t i = 0; i < _width; i++) {
        for (size_t j = 0; j < _height; j++) {
            const size_t index = i + _width * j;
            maybe_t<entity_t *> maybe_tile_entity
                = create_tile(_tiles[index], i, j, world);
            MAYBE_RETURN(maybe_tile_entity, unit_t, "Failed to initialize:");

            _entities[index] = VALUE(maybe_tile_entity);
        }
    }
    
    _initialized = true;
    return unit;
}

extern level_t *generate_test_level() {
    const size_t width = 13;
    const size_t height = 9;
    const size_t count = width * height;

    tile_t tiles[count];
    for (size_t i = 0; i < width; i++) {
        for (size_t j = 0; j < height; j++) {
            bool is_wall = i == 0 || j == 0 || i == width - 1;
            const size_t index = i + width * j;
			tiles[index].is_walkable = !is_wall;
			tiles[index].is_stairs = false;
            tiles[index].spawn_probablity = 0;
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

    return new (std::nothrow) level_t(tiles, width, height);
}
