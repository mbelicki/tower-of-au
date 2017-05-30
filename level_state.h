#pragma once

#include <vector>

#include "warp/math/vec3.h"
#include "warp/utils/directions.h"
#include "warp/utils/random.h"
#include "warp/memory/pool.h"

#include "level.h"
#include "core.h"

namespace warp {
    class entity_t;
}

enum object_type_t {
    OBJ_NONE = 0,
    OBJ_CHARACTER,
    OBJ_BOULDER,
    OBJ_TERMINAL,
    OBJ_PICK_UP,
};

enum object_flags_t {
    FOBJ_NONE            =   0,
    FOBJ_PLAYER_AVATAR   =   1,
    FOBJ_NPCMOVE_STILL   =   2,
    FOBJ_NPCMOVE_LINE    =   4,
    FOBJ_NPCMOVE_ROAM    =   8,
    FOBJ_NPCMOVE_SENTERY =  16,
    FOBJ_CAN_SHOOT       =  32,
    FOBJ_CAN_ROTATE      =  64,
    FOBJ_CAN_PUSH        = 128,
    FOBJ_FRIENDLY        = 256,
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

    const char *chat_scipt;
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
    size_t x, z;
};

typedef warp_pool_id_t obj_id_t;
typedef warp_pool_id_t feat_id_t;

#define OBJ_ID_INVALID WARP_POOL_ID_INVALID
#define FEAT_ID_INVALID WARP_POOL_ID_INVALID

enum command_type_t {
    CMD_TRY_MOVE,
    CMD_TRY_SHOOT
};

struct command_t {
    command_type_t type;
    move_dir_t direction;
    obj_id_t object_id;
};

enum event_type_t : int {
    EVENT_PLAYER_LEAVE = 1,
    EVENT_PLAYER_ENTER_PORTAL,
    EVENT_OBJECT_HURT,
    EVENT_OBJECT_KILLED,
    EVENT_PLAYER_ACTIVATED_TERMINAL,
    EVENT_PLAYER_STARTED_CONVERSATION,
};

struct event_t {
    obj_id_t object_id;
    object_t object_state; /* state of the object when the event happened */
    event_type_t type;
};

enum rt_event_type_t : int {
    RT_EVENT_BULETT_HIT = 1,
};

struct rt_event_t {
    rt_event_type_t type;
    warp::dynval_t value;
};

class bullet_factory_t;
class object_factory_t;

class level_state_t {
    public:
        level_state_t(warp::world_t *world, size_t level_width, size_t level_height);
        ~level_state_t();

        /* single objects/featues management: */
        obj_id_t add_object(const object_t *obj, const warp_tag_t &def_name);
        obj_id_t spawn_object(warp_tag_t name, warp_vec3_t pos , warp_random_t *rand);
        void set_object_flag(obj_id_t obj, object_flags_t flag);

        feat_id_t spawn_feature(feature_type_t type, size_t target, size_t x, size_t z);

        obj_id_t object_at_position(warp_vec3_t pos) const;
        obj_id_t object_at(size_t x, size_t y) const;
        feat_id_t feature_at(size_t x, size_t y) const;

        obj_id_t find_player() const;
        void find_all_characters(std::vector<obj_id_t> *characters);

        const object_t *get_object(obj_id_t id) const;
        const feature_t *get_feature(feat_id_t id) const;


        bool can_move_to(warp_vec3_t new_pos) const;
        bool is_object_idle(obj_id_t obj) const;
        bool has_object_flag(obj_id_t obj, object_flags_t flag) const;
        bool has_object_type(obj_id_t obj, object_type_t type) const;
        bool has_feature_type(feat_id_t obj, feature_type_t type) const;

        bool is_object_valid(obj_id_t obj) const;
    
        /* global state changes: */
        void spawn(const level_t *level, warp_random_t *rand);
        bool apply_command(const command_t *cmd);
        void process_real_time_event(const rt_event_t &event);
        void clear();

        const std::vector<event_t> &get_last_turn_events() const {
            return _events;
        }

        void clear_events() {
            _events.clear();
        }

        const level_t *get_current_level() const { return _level; }

    private:
        void update_object(const command_t *cmd);
        void handle_move(obj_id_t target, warp_vec3_t pos);
        void handle_picking_up(obj_id_t pick_up, obj_id_t character);
        void handle_conversation(obj_id_t npc, obj_id_t player);
        void handle_interaction(obj_id_t terminal, obj_id_t character);
        void handle_attack(obj_id_t target, obj_id_t attacker);
        void handle_shooting(obj_id_t shooter, warp_dir_t dir);

        void change_button_state(feat_id_t feat, feat_state_t state);

        void move_object(obj_id_t target, warp_vec3_t pos, bool immediate);
        bool hurt_object(obj_id_t target, int damage);

        void destroy_object(obj_id_t obj);
        void destroy_feature(feat_id_t feat);

        object_t *get_mutable_object(obj_id_t id) const;
        feature_t *get_mutable_feature(feat_id_t id) const;

        void place_object_at(obj_id_t id, size_t x, size_t z);

    private:
        bool _initialized;

        warp_pool_t *_obj_pool;
        warp_pool_t *_feat_pool;
        obj_id_t  *_obj_placement;
        feat_id_t *_feat_placement;

        warp::world_t *_world;
        size_t _width, _height;
        const level_t *_level;

        bullet_factory_t *_bullet_factory;
        object_factory_t *_object_factory;
        
        std::vector<event_t> _events;

};
