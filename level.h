#pragma once

#include "warp/maybe.h"
#include "warp/vec3.h"
#include "warp/direction.h"

#include <functional>

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
    size_t portal_id;
};

class level_t {
    public:
        level_t(const tile_t *tiles, size_t width, size_t height);
        ~level_t();

        warp::maybeunit_t initialize(warp::world_t *world);
        void set_display_position(const warp::vec3_t pos);
        void set_visiblity(bool visible);

        inline bool is_initialized() const { return _initialized; }
        inline size_t get_width() const { return _width; }
        inline size_t get_height() const { return _height; }

        warp::maybe_t<const tile_t *> get_tile_at(size_t x, size_t y) const;

        bool is_point_walkable(const warp::vec3_t point) const;

        bool scan_if_all
            ( std::function<bool(const tile_t *)> predicate
            , size_t x, size_t y, warp::dir_t direction, size_t distance
            ) const;

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
