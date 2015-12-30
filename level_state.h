#pragma once

#include <vector>

#include "warp/vec3.h"
#include "warp/direction.h"
#include "warp/utils/random.h"

#include "level.h"
#include "core.h"

namespace warp {
    class entity_t;
}

class bullet_factory_t;

enum object_flags_t {
    FOBJ_NONE          =  0,
    FOBJ_PLAYER_AVATAR =  1,
    FOBJ_NPCMOVE_STILL =  2,
    FOBJ_NPCMOVE_LINE  =  4,
    FOBJ_NPCMOVE_ROAM  =  8,
    FOBJ_CAN_SHOOT     = 16,
    FOBJ_CAN_ROTATE    = 32,
};

WARP_ENABLE_FLAGS(object_flags_t)

struct object_t {
    object_type_t type;
    warp::entity_t *entity;

    warp::vec3_t position;
    warp::dir_t direction;

    object_flags_t flags;

    int health;
    int ammo;
};

void initialize_player_object
        (object_t *obj, warp::vec3_t init_pos, warp::world_t *world);

enum feat_state_t : int {
    FSTATE_INACTIVE = 0,
    FSTATE_ACTIVE = 1,
};

struct feature_t {
    feature_type_t type;
    warp::entity_t *entity;

    warp::vec3_t position;
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
        level_state_t(size_t level_width, size_t level_height);
        ~level_state_t();

        /* single objects/featues management: */
        bool add_object(const object_t &obj);
        bool spawn_object
                ( warp::tag_t name, warp::vec3_t pos
                , warp_random_t *rand, warp::world_t *world
                );

        const object_t *object_at_position(warp::vec3_t pos) const;
        const object_t *object_at(size_t x, size_t y) const;
        const object_t *find_player() const;
        void find_all_characters(std::vector<const object_t *> *characters);

        const feature_t *feature_at(size_t x, size_t y) const;

        /* state predicates: */
        bool can_move_to(warp::vec3_t new_pos) const;
    
        /* global state changes: */
        void spawn(warp::world_t *world, const level_t *level, warp_random_t *rand);
        void next_turn(const std::vector<command_t> &commands);
        void process_real_time_event(const rt_event_t &event);
        void clear(warp::world_t *world);

        const std::vector<event_t> &get_last_turn_events() const {
            return _events;
        }

        const level_t *get_current_level() const { return _level; }

    private:
        void update_object(const object_t *obj, const warp::message_t &command);
        void handle_move(object_t *target, warp::vec3_t pos);
        void handle_attack(object_t *target, object_t *attacker);
        void handle_shooting(object_t *shooter, warp::dir_t dir);

        void move_object(object_t *target, warp::vec3_t pos, bool immediate);
        bool hurt_object(object_t *target, int damage);

    private:
        bool _initialized;
        size_t _width, _height;
        object_t  **_objects;
        feature_t **_features;
        const level_t *_level;

        bullet_factory_t *_bullet_factory;
        object_factory_t *_object_factory;
        
        std::vector<event_t> _events;
};
