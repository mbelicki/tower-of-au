#pragma once

#include "warp/statemanager.h"
#include "warp/utils/str.h"

class shared_editor_state_t {
public:
    shared_editor_state_t();
    ~shared_editor_state_t();
    
    warp_str_t get_region_name() const {
        return _region_name;
    }

    void set_region_name(const warp_str_t *name) {
        warp_str_destroy(&_region_name);
        _region_name = warp_str_copy(name);
    }

private:
    warp_str_t _region_name;
};

class enter_editor_transition_t final : public warp::transition_i {
    public:
        enter_editor_transition_t(shared_editor_state_t *state)
            : _state(state)
        { }

        bool is_entity_kept(const warp::entity_t *entity) const override;
        void initialize_state
            (const warp::tag_t &new_state, warp::world_t *world) override;
        void configure_renderer(warp::renderer_t *render) override;
    private:
        shared_editor_state_t *_state;
};

class edit_region_transition_t final : public warp::transition_i {
    public:
        edit_region_transition_t(shared_editor_state_t *state)
            : _state(state)
        { }

        bool is_entity_kept(const warp::entity_t *entity) const override;
        void initialize_state
            (const warp::tag_t &new_state, warp::world_t *world) override;
        void configure_renderer(warp::renderer_t *render) override;
    private:
        shared_editor_state_t *_state;
};
