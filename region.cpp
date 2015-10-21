#include "region.h"

#include <cstring> /* memmove */

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
