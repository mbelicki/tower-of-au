#pragma once

#include "warp/maybe.h"
#include "warp/vec3.h"

class level_t;
class random_t;

namespace warp {
    class world_t;
}

class region_t {
    public:
        region_t(level_t **levels, size_t width, size_t height);
        ~region_t();

        warp::maybeunit_t initialize(warp::world_t *world);
        void change_display_positions(size_t current_x, size_t current_z);
        void animate_transition
            (size_t new_x, size_t new_z, size_t old_x, size_t old_z, float k);

        inline bool is_initialized() const { return _initialized; }
        inline size_t get_width() const { return _width; }
        inline size_t get_height() const { return _height; }

        warp::maybe_t<level_t *> get_level_at(size_t x, size_t y) const;

    private:
        bool _initialized;
        
        size_t _width;
        size_t _height;

        level_t **_levels;
};

region_t *generate_random_region(random_t *random);
warp::maybe_t<region_t *> load_region(const char* path);
