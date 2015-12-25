#include "core.h"

#include <stdlib.h>
#include <time.h>
#include <vector>

#include "warp/world.h"
#include "warp/entity.h"
#include "warp/components.h"
#include "warp/direction.h"
#include "warp/entity-helpers.h"

#include "warp/collections/array.h"
#include "warp/utils/random.h"
#include "warp/utils/str.h"
#include "warp/utils/log.h"

#include "region.h"
#include "level.h"
#include "level_state.h"
#include "character.h"
#include "features.h"
#include "bullets.h"
#include "persitence.h"
#include "text-label.h"
#include "transition_effect.h"

using namespace warp;

static const float LEVEL_TRANSITION_TIME = 1.0f;
static const int PLAYER_MAX_HP = 3;
static const int PLAYER_MAX_AMMO = 16;

enum core_state_t {
    CSTATE_LEVEL = 0,
    CSTATE_TRANSITION,
};

static void destroy_string(void *raw_str) {
    str_destroy(* (warp_str_t *)raw_str);
}

/* TODO: move this to file */
static warp_array_t create_pain_texts() {
    warp_array_t array = array_create_typed(warp_str_t, 16, destroy_string);
    array_append_value(warp_str_t, &array, str_create("ouch!"));
    array_append_value(warp_str_t, &array, str_create("argh!"));
    array_append_value(warp_str_t, &array, str_create("oww!"));
    array_append_value(warp_str_t, &array, str_create("au!"));
    array_append_value(warp_str_t, &array, str_create("uggh!"));
    array_append_value(warp_str_t, &array, str_create("agh!"));
    return array;
}

static dir_t vec3_to_dir(const vec3_t v) {
    const vec3_t absolutes = vec3(fabs(v.x), fabs(v.y), fabs(v.z));
    if (absolutes.x > absolutes.y) {
        if (absolutes.x > absolutes.z) {
            return signum(v.x) > 0 ? DIR_X_PLUS : DIR_X_MINUS;
        } else {
            return signum(v.z) > 0 ? DIR_Z_PLUS : DIR_Z_MINUS;
        }
    } else {
        if (absolutes.y > absolutes.z) {
            return signum(v.y) > 0 ? DIR_Y_PLUS : DIR_Y_MINUS;
        } else {
            return signum(v.z) > 0 ? DIR_Z_PLUS : DIR_Z_MINUS;
        }
    }
}

static void shuffle(dir_t *array, size_t n) {
    for (size_t i = 0; i < n - 1; i++) {
        size_t j = i + rand() / (RAND_MAX / (n - i) + 1);
        dir_t tmp = array[j];
        array[j] = array[i];
        array[i] = tmp;
    }
}

static bool is_idle(const object_t *object) {
    if (object == nullptr) {
        warp_log_e("Cannot check if object is idle, object is null.");
        return false;
    }
    const dynval_t is_idle = object->entity->get_property("avat.is_idle");
    return VALUE(is_idle.get_int()) == 1;
}

static physics_comp_t *create_object_physics(world_t *world, vec2_t size) {
    physics_comp_t *physics = world->create_physics();
    if (physics != nullptr) {
        physics->_flags = PHYSFLAGS_FIXED;
        physics->_velocity = vec2(0, 0);
        physics->_bounds = rectangle_t(vec2(0, 0), size);
    }

    return physics;
}

static maybe_t<entity_t *> create_character_entity
        (vec3_t position, world_t *world, bool confirm_moves) {
    maybe_t<graphics_comp_t *> graphics
        = create_single_model_graphics(world, "npc.obj", "missing.png");
    maybe_t<controller_comp_t *> controller
        = create_character_controller(world, confirm_moves);
    physics_comp_t *physics = create_object_physics(world, vec2(0.4f, 0.4f));

    return world->create_entity(position, VALUE(graphics), physics, VALUE(controller));
}

static void initialize_player
        (object_t *player, vec3_t start_position, world_t *world) {
    const maybe_t<entity_t *> avatar 
        = create_character_entity(start_position, world, true);
    player->entity = VALUE(avatar);
    player->entity->set_tag("player");
    player->position = start_position;
    player->direction = DIR_Z_MINUS;
    player->health = PLAYER_MAX_HP;
    player->ammo = PLAYER_MAX_AMMO;
    player->can_shoot = true;
    player->flags = FOBJ_PLAYER_AVATAR;
}

class core_controller_t final : public controller_impl_i {
    public:
        core_controller_t(const portal_t *start)
                : _owner(nullptr)
                , _world(nullptr)
                , _portal(*start)
                , _level(nullptr)
                , _level_x(0), _level_z(0)
                , _font(nullptr)
                , _state(CSTATE_LEVEL)
                , _transition_timer(0) 
                , _pain_texts()
                , _random(nullptr) {
            _random = warp_random_create(314);
            _portal.region_name = str_copy(start->region_name);
            _pain_texts = create_pain_texts();
        }

        ~core_controller_t() {
            delete _level_state;

            str_destroy(_portal.region_name);
            array_destroy(_pain_texts);
            warp_random_destroy(_random);
        }

        dynval_t get_property(const tag_t &) const override {
            return dynval_t::make_null();
        }

        void initialize(entity_t *owner, world_t *world) override {
            _owner = owner;
            _world = world;

            _level_x = _portal.level_x;
            _level_z = _portal.level_z;

            maybe_t<region_t *> region = load_region(str_value(_portal.region_name));
            if (region.failed()) {
                warp_log_e("Failed to load region: %s", region.get_message().c_str());
                abort();
            }

            _region = VALUE(region);
            maybeunit_t init_result = _region->initialize(_world);
            if (init_result.failed()) {
                warp_log_e("Failed to initialize region: %s", init_result.get_message().c_str());
                abort();
            }

            _region->change_display_positions(_level_x, _level_z);

            _level = VALUE(_region->get_level_at(_level_x, _level_z));
            
            get_default_font(world->get_resources())
                    .with_value([this](font_t *font) {
                _font = font;
            });

            const size_t width  = _level->get_width();
            const size_t height = _level->get_height();
            _level_state = new level_state_t(width, height);
            _level_state->respawn(_world, _level, _random);

            const vec3_t initial_pos = vec3(_portal.tile_x, 0, _portal.tile_z);
            initialize_player(&_last_player_state, initial_pos, _world);
            bool added = _level_state->add_object(_last_player_state);
            if (added == false) {
                warp_log_e("Failed to spawn player avatar.");
                abort();
            }

            update_player_health_display(&_last_player_state);
            update_player_ammo_display(&_last_player_state);
        }

        void update(float dt, const input_t &) override { 
            if (_state == CSTATE_TRANSITION) {
                _transition_timer -= dt;
                if (_transition_timer <= 0) {
                    _transition_timer = 0;
                    _state = CSTATE_LEVEL;
                }

                const float k = 1 - _transition_timer / LEVEL_TRANSITION_TIME;
                const float t = ease_cubic(k);
                _region->animate_transition
                    (_level_x, _level_z, _previous_level_x, _previous_level_z, t);

                const int dx = (int)_level_x - (int)_previous_level_x;
                const int dz = (int)_level_z - (int)_previous_level_z;

                const object_t *player = &_last_player_state;

                vec3_t player_pos = player->position;
                player_pos.x += dx * (1 - t) * 13;
                player_pos.z += dz * (1 - t) * 11;
                player->entity->receive_message(CORE_DO_MOVE_IMMEDIATE, player_pos);

                if (_state == CSTATE_LEVEL) {
                    _region->change_display_positions(_level_x, _level_z);
                    _level_state->respawn(_world, _level, _random);
                    _level_state->add_object(_last_player_state);
                }
            }
        }

        bool accepts(messagetype_t type) const override {
            return type == CORE_TRY_MOVE
                || type == CORE_TRY_SHOOT
                || type == CORE_MOVE_DONE
                || type == CORE_BULLET_HIT
                ;
        }

        void handle_message(const message_t &message) override {
            if (_state == CSTATE_TRANSITION) return;
            const object_t *player = _level_state->find_player();

            const messagetype_t type = message.type;
            if (type == CORE_BULLET_HIT) {
                //const vec3_t target_pos = VALUE(message.data.get_vec3());
                //object_t *object = npc_at_position(target_pos);
                //if (vec3_eps_compare(target_pos, _player.position, 0.05f)) {
                //    object = &_player;
                //}
                //handle_attack(object, nullptr);
            } else if (type == CORE_MOVE_DONE) {
                /* process events from player turn */
                check_events();
                /* let NPC make moves */
                next_turn();
                check_events();
            } else if (is_idle(player)) {
                command_t cmd = {player, message};
                std::vector<command_t> commands;
                commands.push_back(cmd);
                _level_state->update(_level, commands);
            }
        }

    private:
        entity_t *_owner;
        world_t *_world;

        portal_t _portal;
        object_t _last_player_state;

        region_t *_region;
        level_t *_level;
        size_t _level_x;
        size_t _level_z;
        size_t _previous_level_x;
        size_t _previous_level_z;

        level_state_t *_level_state;

        font_t *_font;

        core_state_t _state;
        float _transition_timer;

        warp_array_t _pain_texts;
        warp_random_t *_random;

        void check_events() {
            for (const event_t &event : _level_state->get_last_turn_events()) {
                const event_type_t type = event.type;
                const object_t *obj = &event.object;
                const int x = round(obj->position.x);
                const int z = round(obj->position.z);
                if (type == EVENT_PLAYER_LEAVE) {
                    _last_player_state = *obj;
                    const object_t *player = &_last_player_state;
                    if (x < 0) {
                        start_level_change(player, _level_x - 1, _level_z);
                    } else if (x >= 13) {
                        start_level_change(player, _level_x + 1, _level_z);
                    } else if (z < 0) {
                        start_level_change(player, _level_x, _level_z - 1);
                    } else if (z >= 11) {
                        start_level_change(player, _level_x, _level_z + 1);
                    }
                } else if (type == EVENT_PLAYER_ENTER_PORTAL) {
                    _level->get_tile_at(x, z)
                            .with_value([this](const tile_t *tile) {
                        if (tile->is_stairs) { 
                            maybe_t<const portal_t *> portal
                                = _region->get_portal(tile->portal_id);
                            if (portal.has_value()) {
                                change_region(VALUE(portal));
                            }
                        }
                    });
                } else if (type == EVENT_OBJECT_HURT) {
                    const vec3_t pos = vec3_add(obj->position, vec3(0, 1, 0));
                    create_speech_bubble(_world, *_font, pos, get_pain_text());
                } else if (type == EVENT_OBJECT_KILLED) {
                    if ((obj->flags & FOBJ_PLAYER_AVATAR) != 0) {
                        change_region(&_portal);
                    }
                }
            }
        }

        void update_player_health_display(const object_t *player) {
            if (player == nullptr) {
                warp_log_e("Cannot update health, player is null.");
                return;
            }

            entity_t *hp_label = _world->find_entity("health_label");
            char buffer[PLAYER_MAX_HP + 1];
            for (size_t i = 0; i < PLAYER_MAX_HP; i++) {
                buffer[i] = (int)i < player->health ? '#' : '$';
            }
            buffer[PLAYER_MAX_HP] = '\0';
            hp_label->receive_message(CORE_SHOW_TAG_TEXT, tag_t(buffer));
        }

        void update_player_ammo_display(const object_t *player) {
            if (player == nullptr) {
                warp_log_e("Cannot update health, player is null.");
                return;
            }

            entity_t *hp_label = _world->find_entity("ammo_label");
            char buffer[16];
            snprintf(buffer, 16, "*%2d", player->ammo);
            hp_label->receive_message(CORE_SHOW_TAG_TEXT, tag_t(buffer));
        }

        void change_region(const portal_t *portal) {
            if (portal == nullptr) return;
            
            const float change_time = 2.0f;
            create_fade_circle(_world, 700, change_time, true);
            create_region_token(_world, portal);
            _world->request_state_change("level", change_time);
        }
        
        void start_level_change(const object_t *player, size_t x, size_t z) {
            if (_region->get_level_at(x, z).failed()) return;

            _state = CSTATE_TRANSITION;
            _transition_timer = LEVEL_TRANSITION_TIME;

            _previous_level_x = _level_x;
            _previous_level_z = _level_z;
            change_level(player, x, z);
        }

        void change_level(const object_t *player, size_t x, size_t z) {
            const maybe_t<level_t *> maybe_level = _region->get_level_at(x, z);
            if (maybe_level.failed()) return;

            _level = VALUE(maybe_level);
            _level_state->clear(_world);

            const int dx = x - _level_x;
            const int dz = z - _level_z;
            _level_x = x;
            _level_z = z;

            vec3_t player_pos = player->position;
            player_pos.x -= dx * 13;
            player_pos.z -= dz * 11;
            
            _portal.level_x = x;
            _portal.level_z = z;
            _portal.tile_x = round(player_pos.x);
            _portal.tile_z = round(player_pos.z);

            _last_player_state = *player;
            _last_player_state.position = player_pos;
        }

        const char *get_pain_text() {
            const int max_index = array_get_size(&_pain_texts) - 1; 
            const int index = warp_random_from_range(_random, 0, max_index);
            const warp_str_t text = array_get_value(warp_str_t, &_pain_texts, index);
            return str_value(text);
        }

        void next_turn() {
            std::vector<command_t> commands;
            _level_state->update(_level, commands);

            //const size_t width  = _level->get_width();
            //const size_t height = _level->get_height();
            //const size_t max_characters = width * height;
            //
            //size_t count = 0;
            //object_t **buffer = new object_t * [max_characters];

            //for (size_t x = 0; x < width; x++) {
            //    for (size_t y = 0; y < height; y++) {
            //        object_t *npc = object_at(x, y);
            //        if (npc != nullptr && is_idle(*npc)) {
            //            buffer[count] = npc;
            //            count++;
            //        }
            //    }
            //}

            //for (size_t i = 0; i < count; i++) {
            //    update_npc(buffer[i]);
            //}

            //delete buffer;
        }

        dir_t pick_next_direction(const object_t &npc) {
            //const vec3_t diff = vec3_sub(_player.position, npc.position);
            //const dir_t dir = vec3_to_dir(diff);
            //const vec3_t position = vec3_add(npc.position, dir_to_vec3(dir));
            //if (can_move_to(position, npc.position)) {
            //    return dir;
            //} else {
            //    dir_t directions[4] { 
            //        DIR_X_PLUS, DIR_Z_PLUS, DIR_X_MINUS, DIR_Z_MINUS,
            //    };
            //    shuffle(directions, 4);

            //    const vec3_t position = npc.position;
            //    for (size_t i = 0; i < 4; i++) {
            //        const dir_t dir = directions[i];
            //        const vec3_t target = vec3_add(position, dir_to_vec3(dir));
            //        if (can_move_to(target, position))
            //            return dir;
            //    }
            //}

            return npc.direction;
        }

        //bool can_attack_player(const object_t &npc) {
        //    const float dx = fabs(npc.position.x - _player.position.x);
        //    const float dz = fabs(npc.position.z - _player.position.z);
        //    if (dx > 1.1f || dz > 1.1f) return false;
        //    
        //    const bool close_x = epsilon_compare(dx, 1, 0.05f);
        //    const bool close_z = epsilon_compare(dz, 1, 0.05f);

        //    return close_x != close_z; /* xor */
        //}

        //dir_t can_shoot_player(const object_t &npc) {
        //    if (npc.can_shoot == false || npc.ammo <= 0) return DIR_NONE;

        //    const float dx = npc.position.x - _player.position.x;
        //    const float dz = npc.position.z - _player.position.z;

        //    const bool zero_x = epsilon_compare(dx, 0, 0.05f);
        //    const bool zero_z = epsilon_compare(dz, 0, 0.05f);
        //    if ((zero_x || zero_z) == false) return DIR_NONE;

        //    const size_t x = round(npc.position.x);
        //    const size_t z = round(npc.position.z);
        //    std::function<bool(const tile_t *)> pred = [](const tile_t *t) {
        //        return t->is_walkable;
        //    };

        //    if (zero_x) {
        //        dir_t result = dz > 0 ? DIR_Z_MINUS : DIR_Z_PLUS;
        //        const size_t dist = round(fabs(dz) - 1);
        //        if (_level->scan_if_all(pred, x, z, result, dist))
        //            return result;
        //    } else if (zero_z) {
        //        dir_t result = dx > 0 ? DIR_X_MINUS : DIR_X_PLUS;
        //        const size_t dist = round(fabs(dx) - 1);
        //        if (_level->scan_if_all(pred, x, z, result, dist))
        //            return result;
        //    }
        //    return DIR_NONE;
        //}

        void update_npc(object_t *npc) {
            if (npc->type != OBJ_CHARACTER) {
                return;
            }
            if (npc->attacked) {
                npc->attacked = false;
                npc->direction = opposite_dir(npc->direction);
            }

            //dir_t shoot_dir = DIR_NONE;
            //if (can_attack_player(*npc)) {
            //    handle_attack(&_player, npc);
            //} else if ((shoot_dir = can_shoot_player(*npc)) != DIR_NONE) {
            //    handle_shooting(npc, shoot_dir);               
            //} else {
            //    vec3_t position = vec3_add(npc->position, dir_to_vec3(npc->direction));
            //    const bool change_dir = warp_random_float(_random) > 0.6f;
            //    if (change_dir || (can_move_to(position, npc->position) == false)) {
            //        npc->direction = pick_next_direction(*npc);
            //        position = vec3_add(npc->position, dir_to_vec3(npc->direction));
            //    }
            //    make_move(npc, position, false);
            //}
        }
};

extern maybe_t<entity_t *> create_core(world_t *world, const portal_t *start) {
    if (start == nullptr) {
        return nothing<entity_t *>("Region is null.");
    }

    controller_comp_t *controller = world->create_controller();
    controller->initialize(new core_controller_t(start));

    entity_t *entity = world->create_entity(vec3(0, 0, 0), nullptr, nullptr, controller);

    return entity;
}
