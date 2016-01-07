#include "core.h"

#include <stdlib.h>
#include <time.h>
#include <vector>

#include "warp/keycodes.h"

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
#include "objects_ai.h"
#include "version.h"

using namespace warp;

static const float LEVEL_TRANSITION_TIME = 1.0f;

enum core_state_t {
    CSTATE_IDLE = 0,
    CSTATE_LEVEL_TRANSITION,
    CSTATE_REGION_TRANSITION,
};

static void destroy_string(void *raw_str) {
    warp_str_destroy((warp_str_t *)raw_str);
}

/* TODO: move this to file */
static warp_array_t create_pain_texts() {
    warp_array_t array = warp_array_create_typed(warp_str_t, 16, destroy_string);
    warp_array_append_value(warp_str_t, &array, warp_str_create("ouch!"));
    warp_array_append_value(warp_str_t, &array, warp_str_create("argh!"));
    warp_array_append_value(warp_str_t, &array, warp_str_create("oww!"));
    warp_array_append_value(warp_str_t, &array, warp_str_create("au!"));
    warp_array_append_value(warp_str_t, &array, warp_str_create("uggh!"));
    warp_array_append_value(warp_str_t, &array, warp_str_create("agh!"));
    return array;
}

static bool is_idle(const object_t *object) {
    if (object == nullptr) {
        warp_log_e("Cannot check if object is idle, object is null.");
        return false;
    }
    const dynval_t is_idle = object->entity->get_property("avat.is_idle");
    return VALUE(is_idle.get_int()) == 1;
}

class core_controller_t final : public controller_impl_i {
    public:
        core_controller_t(const portal_t *start)
                : _owner(nullptr)
                , _world(nullptr)
                , _portal(*start)
                , _waiting_for_animation(false)
                , _region(nullptr)
                , _level(nullptr)
                , _level_x(0), _level_z(0)
                , _previous_level_x(0), _previous_level_z(0)
                , _level_state(nullptr)
                , _font(nullptr)
                , _state(CSTATE_IDLE)
                , _transition_timer(0) 
                , _pain_texts()
                , _random(nullptr) 
                , _diagnostics(false)
                , _diag_label(nullptr)
                , _diag_buffer(nullptr) {
            _portal.region_name = warp_str_copy(&start->region_name);
            _pain_texts = create_pain_texts();
        }

        ~core_controller_t() {
            delete _level_state;

            warp_str_destroy(&_portal.region_name);
            warp_array_destroy(&_pain_texts);
            warp_random_destroy(_random);
        }

        dynval_t get_property(const tag_t &name) const override {
            if (name == "lighting") {
                return (void *)_region->get_region_lighting();
            }
            return dynval_t::make_null();
        }

        void initialize(entity_t *owner, world_t *world) override {
            _owner = owner;
            _world = world;

            _level_x = _portal.level_x;
            _level_z = _portal.level_z;

            const uint32_t seed = get_saved_seed(_world);
            _random = warp_random_create(seed);

            const char *region_name = warp_str_value(&_portal.region_name); 
            _region = load_region(region_name);
            if (_region == nullptr) {
                warp_log_e("Failed to load region: '%s'", region_name);
                abort();
            }

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
            _level_state = new level_state_t(_world, width, height);
            _level_state->spawn(_level, _random);

            initialize_player();
        }

        void initialize_player() {
            const object_t *player = get_saved_player_state(_world);
            const vec3_t pos = vec3(_portal.tile_x, 0, _portal.tile_z);
            
            if (player == nullptr || player->type == OBJ_NONE) {
                bool added = _level_state->spawn_object("player", pos, _random);
                if (added == false) {
                    warp_log_e("Failed to spawn player avatar.");
                    abort();
                }
                player = _level_state->find_player();
                save_player_state(_world, player);
                _last_player_state = *player;
            } else {
                _last_player_state = *player;
                _last_player_state.position = pos;
                _level_state->add_object(_last_player_state, "player");
            }

            update_player_health_display(&_last_player_state);
            update_player_ammo_display(&_last_player_state);

            if (player->flags & FOBJ_CAN_SHOOT) {
                enable_shooting_controls();
            }
        }

        void enable_shooting_controls() {
            entity_t *input = _world->find_entity("input");
            if (input != NULL) {
                input->receive_message(CORE_INPUT_ENABLE_SHOOTING, 1);
            }
        }

        void update(float dt, const input_t &) override {
            update_diagnostics();

            if (_state == CSTATE_LEVEL_TRANSITION) {
                _transition_timer -= dt;
                if (_transition_timer <= 0) {
                    _transition_timer = 0;
                    _state = CSTATE_IDLE;
                }

                const float k = 1 - _transition_timer / LEVEL_TRANSITION_TIME;
                const float t = ease_cubic(k);
                _region->animate_transition
                    (_level_x, _level_z, _previous_level_x, _previous_level_z, t);

                const int dx = (int)_level_x - (int)_previous_level_x;
                const int dz = (int)_level_z - (int)_previous_level_z;

                const object_t *player = &_last_player_state;

                vec3_t player_pos = player->position;
                player_pos.x += dx * (1 - t) * (13 - 1);
                player_pos.z += dz * (1 - t) * (11 - 1);
                player->entity->receive_message(CORE_DO_MOVE_IMMEDIATE, player_pos);

                if (_state == CSTATE_IDLE) {
                    _region->change_display_positions(_level_x, _level_z);
                    _level_state->spawn(_level, _random);
                    _level_state->add_object(_last_player_state, "player");
                }
            }
        }

        bool accepts(messagetype_t type) const override {
            return type == CORE_TRY_MOVE
                || type == CORE_TRY_SHOOT
                || type == CORE_MOVE_DONE
                || type == CORE_BULLET_HIT
                || type == CORE_RESTART_LEVEL
                || type == MSG_INPUT_KEYUP
                ;
        }

        void handle_message(const message_t &message) override {
            const messagetype_t type = message.type;
            if (type == MSG_INPUT_KEYUP) {
                SDL_Keycode code = VALUE(message.data.get_int());
                if (code == SDLK_g) {
                    enable_diagnostics(_diagnostics == false);
                }
            }
            if (_state != CSTATE_IDLE) { 
                return;
            }

            const object_t *player = _level_state->find_player();
            if (type == CORE_RESTART_LEVEL) {
                change_region(&_portal);
            } else if (type == CORE_BULLET_HIT) {
                warp_log_d("real time event");
                rt_event_t event = {RT_EVENT_BULETT_HIT, message.data};
                _level_state->process_real_time_event(event);

                check_events();
            } else if (type == CORE_MOVE_DONE && _waiting_for_animation) {
                warp_log_d("NPC turn");
                _waiting_for_animation = false;
                next_turn();

                check_events();
            } else if (is_idle(player)) {
                warp_log_d("player turn");
                command_t cmd = {player, message};
                std::vector<command_t> commands;
                commands.push_back(cmd);
                _level_state->next_turn(commands);

                check_events();
                _waiting_for_animation = true;
            }
        }

    private:
        entity_t *_owner;
        world_t *_world;

        portal_t _portal;
        object_t _last_player_state;
        bool _waiting_for_animation;

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

        bool _diagnostics;
        entity_t *_diag_label;
        char *_diag_buffer;

        void enable_diagnostics(bool enable) {
            _diagnostics = enable;
            _diag_label = _world->find_entity("diag_label");
            _diag_label->receive_message(MSG_GRAPHICS_VISIBLITY, (int)enable);
            if (_diag_buffer == nullptr) {
                _diag_buffer = new char [1024];
            }
        }

        void update_diagnostics() {
            if (_diagnostics == false || _diag_label == nullptr) return;

            size_t x = 0; 
            size_t z = 0; 
            const object_t *player = _level_state->find_player();
            if (player != nullptr) {
                x = round(player->position.x);
                z = round(player->position.z);
            }
            
            const stats_t &stats = _world->get_statistics();

            snprintf
                ( _diag_buffer, 1024
                , "%s\n%s\nlevel x: %zu z: %zu, tile x: %zu z: %zu\n%f fps"
                , VERSION
                , warp_str_value(&_portal.region_name)
                , _level_x, _level_z, x, z
                , stats.avg_fps
                );

            _diag_label->receive_message(CORE_SHOW_POINTER_TEXT, (void *)_diag_buffer);
        }

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
                    emit_speech(obj, get_pain_text());
                } else if (type == EVENT_OBJECT_KILLED) {
                    if ((obj->flags & FOBJ_PLAYER_AVATAR) != 0) {
                        change_region(&_portal);
                    }
                } else if (type == EVENT_PLAYER_ACTIVATED_TERMINAL) {
                    const object_t *player = _level_state->find_player();
                    /* push terminal: */
                    if (obj->flags & FOBJ_CAN_PUSH) {
                        if ((player->flags & FOBJ_CAN_PUSH) == 0) {
                            emit_speech(obj, "gained\n push");
                            _level_state->set_object_flag(player, FOBJ_CAN_PUSH);
                        }
                    } else if (obj->flags & FOBJ_CAN_SHOOT) {
                        if ((player->flags & FOBJ_CAN_SHOOT) == 0) {
                            emit_speech(obj, " gained\nshooting");
                            _level_state->set_object_flag(player, FOBJ_CAN_SHOOT);
                            enable_shooting_controls();
                        }
                    }
                }
            }

            const object_t *player = _level_state->find_player();
            if (player != nullptr) {
                update_player_health_display(player);
                update_player_ammo_display(player);
            }
        }

        void emit_speech(const object_t *obj, const char* text) {
            if (obj == nullptr) {
                warp_log_e("Cannot emit speech bubule, null object.");
                return;
            }
            if (text == nullptr) {
                warp_log_e("Cannot emit speech bubule, null text.");
                return;
            }
            const vec3_t pos = vec3_add(obj->position, vec3(0, 1.3f, -0.1f));
            create_speech_bubble(_world, *_font, pos, text);
        }

        void update_player_health_display(const object_t *player) {
            static const int PLAYER_MAX_HP = 3; /* TODO: fix this later */
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

            entity_t *ammo_label = _world->find_entity("ammo_label");
            if (player->flags & FOBJ_CAN_SHOOT) {
                char buffer[16];
                snprintf(buffer, 16, "*%2d", player->ammo);
                ammo_label->receive_message(CORE_SHOW_TAG_TEXT, tag_t(buffer));
                ammo_label->receive_message(MSG_GRAPHICS_VISIBLITY, 1);
            } else {
                ammo_label->receive_message(MSG_GRAPHICS_VISIBLITY, 0);
            }
        }

        void change_region(const portal_t *portal) {
            if (portal == nullptr) return;

            const float change_time = 2.0f;
            create_fade_circle(_world, 700, change_time, true);
            save_portal(_world, portal);
            save_player_state(_world, &_last_player_state);
            _world->request_state_change("level", change_time);
            _state = CSTATE_REGION_TRANSITION;

            _world->broadcast_message(CORE_SAVE_TO_FILE, 0);
        }
        
        void start_level_change(const object_t *player, size_t x, size_t z) {
            if (_region->get_level_at(x, z).failed()) return;

            _state = CSTATE_LEVEL_TRANSITION;
            _transition_timer = LEVEL_TRANSITION_TIME;

            _previous_level_x = _level_x;
            _previous_level_z = _level_z;
            change_level(player, x, z);
        }

        void change_level(const object_t *player, size_t x, size_t z) {
            const maybe_t<level_t *> maybe_level = _region->get_level_at(x, z);
            if (maybe_level.failed()) return;

            _level = VALUE(maybe_level);
            _level_state->clear();

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

            const uint32_t new_seed = warp_random_next(_random);
            warp_random_seed(_random, new_seed);

            save_portal(_world, &_portal);
            save_player_state(_world, &_last_player_state);
            save_random_seed(_world, new_seed);

            _world->broadcast_message(CORE_SAVE_TO_FILE, 0);
        }

        const char *get_pain_text() {
            const int max_index = warp_array_get_size(&_pain_texts) - 1; 
            const int index = warp_random_from_range(_random, 0, max_index);
            const warp_str_t text = warp_array_get_value(warp_str_t, &_pain_texts, index);
            return warp_str_value(&text);
        }

        void next_turn() {
            std::vector<const object_t *> characters;
            _level_state->find_all_characters(&characters);

            std::vector<command_t> commands;
            for (const object_t *obj : characters) {
                command_t buffer;
                const bool picked
                    = pick_next_command(&buffer, obj, _level_state, _random);
                if (picked) commands.push_back(buffer);
            }
            _level_state->next_turn(commands);
        }
};

extern entity_t *create_core(world_t *world, const portal_t *start) {
    if (start == nullptr) {
        warp_log_e("Cannot create core with null portal.");
        return nullptr;
    }

    controller_comp_t *controller = world->create_controller();
    controller->initialize(new core_controller_t(start));

    entity_t *entity = world->create_entity(vec3(0, 0, 0), nullptr, nullptr, controller);
    entity->set_tag("core");

    return entity;
}
