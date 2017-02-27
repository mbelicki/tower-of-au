#pragma once

#include <vector>

#include "warp/math/vec3.h"
#include "warp/utils/directions.h"
#include "warp/utils/random.h"

#include "level.h"
#include "core.h"

namespace warp {
    class entity_t;
}

class bullet_factory_t;

enum object_type_t {
    OBJ_NONE = 0,
    OBJ_CHARACTER,
    OBJ_BOULDER,
    OBJ_TERMINAL,
    OBJ_PICK_UP,
};

enum object_flags_t {
    FOBJ_NONE          =  0,
    FOBJ_PLAYER_AVATAR =  1,
    FOBJ_NPCMOVE_STILL =  2,
    FOBJ_NPCMOVE_LINE  =  4,
    FOBJ_NPCMOVE_ROAM  =  8,
    FOBJ_CAN_SHOOT     = 16,
    FOBJ_CAN_ROTATE    = 32,
    FOBJ_CAN_PUSH      = 64,
};

WARP_ENABLE_FLAGS(object_flags_t)

struct object_t {
    object_type_t type;
    warp::entity_t *entity;

    warp_vec3_t position;
    warp_dir_t direction;

    object_flags_t flags;

    int health;
    int max_health;
    int ammo;
};

void initialize_player_object
        (object_t *obj, warp_vec3_t init_pos, warp::world_t *world);

enum feat_state_t : int {
    FSTATE_INACTIVE = 0,
    FSTATE_ACTIVE = 1,
};

struct feature_t {
    feature_type_t type;
    warp::entity_t *entity;

    size_t target_id;
    feat_state_t state;
};

struct command_t {
    const object_t *object;
    warp::message_t command;
};

enum event_type_t : int {
    EVENT_PLAYER_LEAVE = 1,
    EVENT_PLAYER_ENTER_PORTAL,
    EVENT_OBJECT_HURT,
    EVENT_OBJECT_KILLED,
    EVENT_PLAYER_ACTIVATED_TERMINAL,
};

struct event_t {
    object_t object;
    event_type_t type;
};

enum rt_event_type_t : int {
    RT_EVENT_BULETT_HIT = 1,
};

struct rt_event_t {
    rt_event_type_t type;
    warp::dynval_t value;
};

class object_factory_t;

class level_state_t {
    public:
        level_state_t(warp::world_t *world, size_t level_width, size_t level_height);
        ~level_state_t();

        /* single objects/featues management: */
        bool add_object(const object_t &obj, const warp_tag_t &def_name);
        bool spawn_object(warp_tag_t name, warp_vec3_t pos , warp_random_t *rand);
        void set_object_flag(const object_t *obj, object_flags_t flag);

        const object_t *object_at_position(warp_vec3_t pos) const;
        const object_t *object_at(size_t x, size_t y) const;
        const object_t *find_player() const;
        void find_all_characters(std::vector<const object_t *> *characters);

        const feature_t *feature_at(size_t x, size_t y) const;

        /* state predicates: */
        bool can_move_to(warp_vec3_t new_pos) const;
    
        /* global state changes: */
        void spawn(const level_t *level, warp_random_t *rand);
        void next_turn(const std::vector<command_t> &commands);
        void process_real_time_event(const rt_event_t &event);
        void clear();

        const std::vector<event_t> &get_last_turn_events() const {
            return _events;
        }

        const level_t *get_current_level() const { return _level; }

    private:
        void update_object(const object_t *obj, const warp::message_t &command);
        void handle_move(object_t *target, warp_vec3_t pos);
        void handle_picking_up(object_t *pick_up, object_t *character);
        void handle_interaction(object_t *terminal, object_t *character);
        void handle_attack(object_t *target, object_t *attacker);
        void handle_shooting(object_t *shooter, warp_dir_t dir);

        void change_button_state(feature_t *feat, feat_state_t state);

        void move_object(object_t *target, warp_vec3_t pos, bool immediate);
        bool hurt_object(object_t *target, int damage);

        void destroy_object(object_t *obj);
        void destroy_feature(feature_t *feat);

    private:
        bool _initialized;
        warp::world_t *_world;
        size_t _width, _height;
        object_t  **_objects;
        feature_t **_features;
        const level_t *_level;

        bullet_factory_t *_bullet_factory;
        object_factory_t *_object_factory;
        
        std::vector<event_t> _events;
};
