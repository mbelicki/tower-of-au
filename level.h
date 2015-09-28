#pragma once

#include "warp/maybe.h"

namespace warp {
    class entity_t;
    class world_t;
}

struct tile_t {
    bool is_walkable;
    float spawn_probablity;
};

class level_t {
    public:
        level_t(const tile_t *tiles, size_t width, size_t height);
        ~level_t();

        warp::maybeunit_t initialize(warp::world_t *world);

        inline bool is_initialized() const { return _initialized; }
        warp::maybe_t<const tile_t *> get_tile_at(size_t x, size_t y) const;

        size_t get_width() const { return _width; }
        size_t get_height() const { return _height; }

    private:
        bool _initialized;
        
        size_t _width;
        size_t _height;

        warp::entity_t **_entities;
        tile_t *_tiles;
};

/// Generates uninitialized level instance.
level_t *generate_test_level();
