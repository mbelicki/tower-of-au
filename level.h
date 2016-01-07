#pragma once

#include "warp/tag.h"
#include "warp/vec3.h"
#include "warp/maybe.h"
#include "warp/direction.h"

#include "warp/utils/random.h"

#include <functional>

namespace warp {
    class entity_t;
    class world_t;
}

enum feature_type_t : int {
    FEAT_NONE = 0,
    FEAT_BUTTON,
    FEAT_DOOR,
    FEAT_SPIKES,
    FEAT_BREAKABLE_FLOOR,
};

struct tile_t {
    bool is_walkable;
    bool is_stairs;

    float spawn_probablity;
    warp::tag_t object_id;
    warp::dir_t object_dir;

    feature_type_t feature;
    size_t feat_target_id;
    size_t portal_id;
    warp::tag_t graphics_id;
};

class region_t;

class level_t {
    public:
        level_t(const tile_t *tiles, size_t width, size_t height);
        ~level_t();

        warp::maybeunit_t initialize
            (warp::world_t *world, const region_t *owner);
        void set_display_position(const warp::vec3_t pos);
        void set_visiblity(bool visible);

        inline bool is_initialized() const { return _initialized; }
        inline size_t get_width() const { return _width; }
        inline size_t get_height() const { return _height; }
        inline warp::entity_t *get_entity() const { return _entity; }

        warp::maybe_t<const tile_t *> get_tile_at(int x, int y) const;

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
level_t *generate_random_level(warp_random_t *random);
