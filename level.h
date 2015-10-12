#pragma once

#include "warp/maybe.h"

class random_t;

namespace warp {
    class entity_t;
    class world_t;
}

enum feature_type_t : int {
    FEAT_NONE = 0,
    FEAT_BUTTON,
    FEAT_DOOR,
};

enum object_type_t {
    OBJ_NONE = 0,
    OBJ_CHARACTER,
    OBJ_BOULDER,
};

struct tile_t {
    bool is_walkable;
    bool is_stairs;

    float spawn_probablity;
    object_type_t spawned_object;

    feature_type_t feature;
    size_t feat_target_id;
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

        tile_t *_tiles;
        warp::entity_t *_entity;
};

/// Generates uninitialized level instance.
level_t *generate_test_level();
level_t *generate_random_level(random_t *random);
