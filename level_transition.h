#pragma once

#include "warp/statemanager.h"

struct region_lighting_t;

class level_transition_t final : public warp::transition_i {
    public:
        level_transition_t() : _renderer(NULL) {}
        bool is_entity_kept(const warp::entity_t *entity) const override;
        void initialize_state
            (const warp_tag_t &new_state, warp::world_t *world) override;
        void configure_renderer(warp::renderer_t *render) override;

    private:
        warp::renderer_t *_renderer;
};
