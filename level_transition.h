#pragma once

#include "warp/statemanager.h"

struct region_lighting_t;

class level_transition_t final : public warp::transition_i {
    public:
        bool is_entity_kept(const warp::entity_t *entity) const override;
        void initialize_state
            (const warp::tag_t &new_state, warp::world_t *world) override;
        void configure_renderer(warp::renderer_t *render) override;

    private:
        const region_lighting_t *_lighting;
};
