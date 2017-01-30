#pragma once

#include "warp/collections/array.h"
#include "warp/utils/random.h"
#include "warp/utils/tag.h"
#include "warp/utils/str.h"
#include "warp/math/vec3.h"

class level_t;

namespace warp {
    class world_t;
}

struct portal_t {
    warp_str_t region_name;
    size_t level_x, level_z;
    size_t tile_x, tile_z;
};

struct tile_graphics_t {
    warp_tag_t name;
    warp_str_t mesh;
    warp_str_t texture;
};

struct region_lighting_t {
    warp_vec3_t sun_color;
    warp_vec3_t sun_direction;
    warp_vec3_t ambient_color;
};

class region_t {
    public:
        region_t(level_t **levels, size_t width, size_t height);
        ~region_t();

        void initialize(warp::world_t *world);
        void change_display_positions(size_t current_x, size_t current_z);
        void animate_transition
            (size_t new_x, size_t new_z, size_t old_x, size_t old_z, float k);

        bool add_portal
            ( const char *region_name, size_t level_x, size_t level_z
            , size_t tile_x, size_t tile_z
            );
        bool add_tile_graphics
            (const warp_tag_t &name, const char *mesh, const char *texture);
        void set_lighting(const region_lighting_t *lighting);

        inline bool is_initialized() const { return _initialized; }
        inline size_t get_width() const { return _width; }
        inline size_t get_height() const { return _height; }

        level_t *get_level_at(size_t x, size_t y) const;
        const portal_t *get_portal(size_t id);
        const tile_graphics_t *get_tile_graphics(const warp_tag_t &id) const;
        const region_lighting_t *get_region_lighting() const { return &_lighting; }

    private:
        bool _initialized;
        
        size_t _width;
        size_t _height;

        level_t **_levels;
        
        warp_array_t _portals;
        warp_array_t _graphics;

        region_lighting_t _lighting;
};

region_t *generate_random_region(warp_random_t *random);
region_t *load_region(const char *path);
