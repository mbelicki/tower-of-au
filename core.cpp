#define WARP_DROP_PREFIX
#include "core.h"

#include <stdlib.h>
#include <time.h>
#include <vector>

#include "warp/keycodes.h"

#include "warp/world.h"
#include "warp/entity.h"
#include "warp/components.h"
#include "warp/utils/directions.h"
#include "warp/entity-helpers.h"

#include "warp/collections/array.h"
#include "warp/utils/random.h"
#include "warp/utils/str.h"
#include "warp/utils/log.h"
#include "warp/math/utils.h"

#include "warp/graphics/font.h"

#include "chat.h"
#include "region.h"
#include "level.h"
#include "level_state.h"
#include "character.h"
#include "features.h"
#include "bullets.h"
#include "button.h"
#include "persitence.h"
#include "text-label.h"
#include "transition_effect.h"
#include "version.h"

using namespace warp;

static const float LEVEL_TRANSITION_TIME = 1.0f;

enum core_state_t {
    CSTATE_IDLE = 0,
    CSTATE_LEVEL_TRANSITION,
    CSTATE_REGION_TRANSITION,
    CSTATE_CONVERSATION,
};

struct converation_state_t {
    entity_t *fader;
    entity_t *text;
    entity_t *portrait;
    entity_t *buttons[3];
    size_t buttons_count;
    const chat_t *chat;
};

static void destroy_string(void *raw_str) {
    warp_str_destroy((warp_str_t *)raw_str);
}

static warp_array_t create_pain_texts() {
    const char *texts[] = {"ouch!", "argh!", "oww!", "au!", "uggh!", "agh!"};
    const size_t count = sizeof texts / sizeof texts[0];
    warp_array_t array = warp_array_create_typed(warp_str_t, 16, destroy_string);
    for (size_t i = 0; i < count; i++) {
        warp_array_append_value(warp_str_t, &array, WARP_STR(texts[i]));
    }
    return array;
}

class core_controller_t final : public controller_impl_i {
    public:
        core_controller_t(const portal_t *start)
                : _owner(NULL)
                , _world(NULL)
                , _portal(*start)
                , _waiting_for_animation(false)
                , _region(NULL)
                , _level(NULL)
                , _level_x(0), _level_z(0)
                , _previous_level_x(0), _previous_level_z(0)
                , _level_state(NULL)
                , _font(WARP_RES_ID_INVALID)
                , _state(CSTATE_IDLE)
                , _transition_timer(0) 
                , _pain_texts()
                , _random(NULL) 
                , _diagnostics(false)
                , _diag_label(NULL)
                , _diag_buffer(NULL) {
            _portal.region_name = warp_str_copy(&start->region_name);
            _pain_texts = create_pain_texts();
            _facts = warp_map_create_typed(int, NULL);
            memset(&_conversation, 0, sizeof _conversation);
        }

        ~core_controller_t() {
            delete _level_state;
            delete _region;

            warp_str_destroy(&_portal.region_name);
            warp_array_destroy(&_pain_texts);
            warp_map_destroy(&_facts);
            warp_random_destroy(_random);
        }

        dynval_t get_property(const warp_tag_t &name) const override {
            if (warp_tag_equals_buffer(&name, "lighting")) {
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

            const warp_map_t *facts = get_saved_facts(_world);
            warp_map_copy_entries(&_facts, facts);

            const char *region_name = warp_str_value(&_portal.region_name); 
            _region = load_region(region_name);
            if (_region == NULL) {
                warp_critical("Failed to load region: '%s'", region_name);
            }

            _region->initialize(_world);

            _region->change_display_positions(_level_x, _level_z);
            _level = _region->get_level_at(_level_x, _level_z);
            if (_level == NULL) {
                warp_critical("Failed to get level font.");
            }
            
            _font = get_default_font(_world->get_resources());
            const size_t width  = _level->get_width();
            const size_t height = _level->get_height();
            _level_state = new level_state_t(_world, width, height);
            _level_state->spawn(_level, _random);

            initialize_player();
        }

        void initialize_player() {
            const object_t *player = get_saved_player_state(_world);
            const vec3_t pos = vec3(_portal.tile_x, 0, _portal.tile_z);
            
            if (player == NULL || player->type == OBJ_NONE) {
                bool added = _level_state->spawn_object(WARP_TAG("player"), pos, DIR_NONE, _random);
                if (added == false) {
                    warp_critical("Failed to spawn player avatar.");
                }
                const obj_id_t id = _level_state->find_player();
                player = _level_state->get_object(id);
                if (player == NULL) {
                    warp_critical("Failed to find player object on the level.");
                }
                save_player_state(_world, player);
                _last_player_state = *player;
            } else {
                _last_player_state = *player;
                _last_player_state.position = pos;
                _level_state->add_object(&_last_player_state, WARP_TAG("player"));
            }

            update_player_health_display(&_last_player_state);
            update_player_ammo_display(&_last_player_state);

            if (player->flags & FOBJ_CAN_SHOOT) {
                enable_shooting_controls();
            }
        }

        void enable_shooting_controls() {
            entity_t *input = _world->find_entity(WARP_TAG("input"));
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
                    _level_state->add_object(&_last_player_state, WARP_TAG("player"));
                }
            }
        }

        void handle_message(const message_t &message) override {
            const messagetype_t type = message.type;
            if (type == MSG_INPUT_KEY_UP) {
                SDL_Keycode code = message.data.get_int();
                if (code == SDLK_g) {
                    enable_diagnostics(_diagnostics == false);
                }
            }
            if (_state != CSTATE_IDLE) { 
                return;
            }

            const obj_id_t player = _level_state->find_player();
            if (type == CORE_RESTART_LEVEL) {
                change_region(&_portal, true);
            } else if (type == CORE_SAVE_RESET_DEFAULTS) {
                change_region(&_portal, false);
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
            } else if (_level_state->is_object_idle(player) &&
                        (type == CORE_TRY_MOVE || type == CORE_TRY_SHOOT)) {
                warp_log_d("player turn");
                move_dir_t dir = (move_dir_t) message.data.get_int();
                command_type_t ty = type == CORE_TRY_MOVE ? CMD_MOVE : CMD_SHOOT;
                command_t cmd = {ty, dir, player};
                _level_state->apply_command(&cmd);

                check_events();
                _waiting_for_animation = true;
            } else if (type == CORE_AI_COMMAND) {
                const command_t *cmd = (command_t *) message.data.get_pointer();
                _level_state->apply_command(cmd);
                check_events();
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

        res_id_t _font;

        core_state_t _state;
        float _transition_timer;

        converation_state_t _conversation;

        warp_array_t _pain_texts;
        warp_map_t _facts;
        warp_random_t *_random;

        bool _diagnostics;
        entity_t *_diag_label;
        char *_diag_buffer;

        void enable_diagnostics(bool enable) {
            _diagnostics = enable;
            _diag_label = _world->find_entity(WARP_TAG("diag_label"));
            _diag_label->receive_message(MSG_GRAPHICS_VISIBLITY, (int)enable);
            if (_diag_buffer == NULL) {
                _diag_buffer = new char [1024];
            }
        }

        void update_diagnostics() {
            if (_diagnostics == false || _diag_label == NULL) return;

            size_t x = 0; 
            size_t z = 0; 
            const obj_id_t id = _level_state->find_player();
            const object_t *player = _level_state->get_object(id);
            if (player != NULL) {
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
                const object_t *obj = &event.object_state;

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
                    const tile_t *tile = _level->get_tile_at(x, z);
                    if (tile != NULL && tile->is_stairs) { 
                        const portal_t *portal = _region->get_portal(tile->portal_id);
                        if (portal != NULL) {
                            change_region(portal, true);
                        }
                    }
                } else if (type == EVENT_OBJECT_HURT) {
                    emit_speech(obj, get_pain_text());
                } else if (type == EVENT_OBJECT_KILLED) {
                    if (obj->flags & FOBJ_PLAYER_AVATAR) {
                        change_region(&_portal, true);
                    }
                } else if (type == EVENT_PLAYER_ACTIVATED_TERMINAL) {
                    const obj_id_t player_id = _level_state->find_player();
                    const object_t *player = _level_state->get_object(player_id);
                    /* push terminal: */
                    if (obj->flags & FOBJ_CAN_PUSH) {
                        if ((player->flags & FOBJ_CAN_PUSH) == 0) {
                            emit_speech(obj, "gained\n push");
                            _level_state->set_object_flag(player_id, FOBJ_CAN_PUSH);
                        }
                    } else if (obj->flags & FOBJ_CAN_SHOOT) {
                        if ((player->flags & FOBJ_CAN_SHOOT) == 0) {
                            emit_speech(obj, " gained\nshooting");
                            _level_state->set_object_flag(player_id, FOBJ_CAN_SHOOT);
                            enable_shooting_controls();
                        }
                    }
                } else if (type == EVENT_PLAYER_STARTED_CONVERSATION) {
                    emit_speech(obj, "hello");
                    start_conversation(obj);
                }
            }

            const obj_id_t player_id = _level_state->find_player();
            if (player_id != OBJ_ID_INVALID) {
                const object_t *player = _level_state->get_object(player_id);
                update_player_health_display(player);
                update_player_ammo_display(player);
            }

            _level_state->clear_events();
        }

        void emit_speech(const object_t *obj, const char* text) {
            if (obj == NULL) {
                warp_log_e("Cannot emit speech bubule, null object.");
                return;
            }
            if (text == NULL) {
                warp_log_e("Cannot emit speech bubule, null text.");
                return;
            }
            const vec3_t pos = vec3_add(obj->position, vec3(0, 1.3f, -0.1f));
            create_speech_bubble(_world, _font, pos, text);
        }

        void update_player_health_display(const object_t *player) {
            if (player == NULL) {
                warp_log_e("Cannot update health, player is null.");
                return;
            }
            const int max_hp = player->max_health;

            entity_t *hp_label = _world->find_entity(WARP_TAG("health_label"));
            if (hp_label != NULL) {
                char *buffer = (char *)calloc(max_hp + 1, sizeof (char));
                for (size_t i = 0; i < (size_t)max_hp; i++) {
                    buffer[i] = (int)i < player->health ? '#' : '$';
                }
                buffer[max_hp] = '\0';
                hp_label->receive_message(CORE_SHOW_TAG_TEXT, WARP_TAG(buffer));
                free(buffer);
            }
        }

        void update_player_ammo_display(const object_t *player) {
            if (player == NULL) {
                warp_log_e("Cannot update health, player is null.");
                return;
            }

            entity_t *ammo_label = _world->find_entity(WARP_TAG("ammo_label"));
            if (ammo_label != NULL) {
                if (player->flags & FOBJ_CAN_SHOOT) {
                    char buffer[16];
                    snprintf(buffer, 16, "*%2d", player->ammo);
                    ammo_label->receive_message(CORE_SHOW_TAG_TEXT, WARP_TAG(buffer));
                    ammo_label->receive_message(MSG_GRAPHICS_VISIBLITY, 1);
                } else {
                    ammo_label->receive_message(MSG_GRAPHICS_VISIBLITY, 0);
                }
            }
        }

        void end_conversation() {
            _state = CSTATE_IDLE;
            for (size_t i = 0; i < _conversation.buttons_count; i++) {
                _world->destroy_later(_conversation.buttons[i]);
            }
            _world->destroy_later(_conversation.fader);
            _world->destroy_later(_conversation.text);
            _world->destroy_later(_conversation.portrait);

            _conversation.fader = create_fade_circle(_world, 700, 1.0f, false);
            _conversation.fader->receive_message(MSG_GRAPHICS_RECOLOR, vec4(0, 0, 0, 0.7f));
        }

        void update_conversation(warp_tag_t next_id) {
            if (warp_tag_equals_buffer(&next_id, "")) {
                end_conversation();
                return;
            }
            const chat_entry_t *entry
                = get_entry(_conversation.chat, _random, &_facts, next_id);
            const char *message = warp_str_value(&entry->text);
            _conversation.text->receive_message(CORE_SHOW_POINTER_TEXT, (void *)message);
            
            chat_entry_evaluate_side_effects(entry, &_facts);

            for (size_t i = 0; i < _conversation.buttons_count; i++) {
                _world->destroy_later(_conversation.buttons[i]);
            }
            create_conversation_buttons(entry);
        }

        void start_conversation(const object_t *npc) {
            (void) npc;
            _state = CSTATE_CONVERSATION;
            if (_conversation.fader != NULL) {
                _world->destroy_later(_conversation.fader);
            }

            _conversation.fader = create_fade_circle(_world, 700, 1.0f, true);
            _conversation.fader->receive_message(MSG_GRAPHICS_RECOLOR, vec4(0, 0, 0, 0.7f));

            _conversation.chat = get_chat(_world->get_resources(), npc->chat_scipt);
            const chat_entry_t *start = get_start_entry(_conversation.chat, _random, &_facts);

            const res_id_t font = get_dialog_font(_world->get_resources());
            const char *message = warp_str_value(&start->text);
            _conversation.text
                = create_label(_world, font, LABEL_POS_LEFT | LABEL_POS_BOTTOM);
            _conversation.text->receive_message(MSG_PHYSICS_MOVE, vec3(-130 + 16, -64, 8));
            _conversation.text->receive_message(CORE_SHOW_POINTER_TEXT, (void *)message);
            _conversation.text->receive_message(MSG_PHYSICS_SCALE, vec3(1.6f, 1.6f, 1));

            const char *portrait_tex = warp_str_value(&_conversation.chat->default_portrait);
            _conversation.portrait =
                create_ui_image(_world, vec2(-300, -128), vec2(256, 512), portrait_tex);
            
            chat_entry_evaluate_side_effects(start, &_facts);
            create_conversation_buttons(start);
        }

        void create_conversation_buttons(const chat_entry_t *entry) {
            _conversation.buttons_count = entry->responses_count;
            for (size_t i = 0; i < entry->responses_count; i++) {
                const char *resp = warp_str_value(&entry->responses[i].text);
                const warp_tag_t next_id = entry->responses[i].next_id;
                const float y = -128.0f + (-96.0f * i);
                _conversation.buttons[i] = create_text_button
                    ( _world, vec2(170, y), vec2(600, 64)
                    , [this, next_id]() { this->update_conversation(next_id); }
                    , resp, vec4(0, 0, 0, 0.6f)
                    );
            }
        }

        void change_region(const portal_t *portal, bool save_data) {
            if (portal == NULL) return;

            const float change_time = 2.0f;
            create_fade_circle(_world, 700, change_time, true);
            _world->request_state_change(WARP_TAG("level"), change_time);
            _state = CSTATE_REGION_TRANSITION;
            
            if (save_data) {
                save_portal(_world, portal);
                save_player_state(_world, &_last_player_state);
            }
            _world->broadcast_message(CORE_SAVE_TO_FILE, 0);
        }
        
        void start_level_change(const object_t *player, size_t x, size_t z) {
            if (_region->get_level_at(x, z) == NULL) return;

            _state = CSTATE_LEVEL_TRANSITION;
            _transition_timer = LEVEL_TRANSITION_TIME;

            _previous_level_x = _level_x;
            _previous_level_z = _level_z;
            change_level(player, x, z);
        }

        void change_level(const object_t *player, size_t x, size_t z) {
            level_t *level = _region->get_level_at(x, z);
            if (level == NULL) {
                warp_log_e("Cannot change level, no level at: %zu, %zu.", x, z);
                return;
            }

            _level = level;
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
            save_facts(_world, &_facts);

            _world->broadcast_message(CORE_SAVE_TO_FILE, 0);
        }

        const char *get_pain_text() {
            const int max_index = warp_array_get_size(&_pain_texts) - 1; 
            const int index = warp_random_from_range(_random, 0, max_index);
            const warp_str_t text = warp_array_get_value(warp_str_t, &_pain_texts, index);
            return warp_str_value(&text);
        }

        void next_turn() {
            std::vector<obj_id_t> characters;
            _level_state->find_all_characters(&characters);
            message_t next_turn(CORE_NEXT_TURN, (void *)_level_state);
            for (obj_id_t id : characters) {
                const object_t *obj = _level_state->get_object(id);
                obj->entity->receive(next_turn);
            }
        }
};

extern entity_t *create_core(world_t *world, const portal_t *start) {
    if (start == NULL) {
        warp_log_e("Cannot create core with null portal.");
        return NULL;
    }

    controller_comp_t *controller = world->create_controller();
    controller->initialize(new core_controller_t(start));

    entity_t *entity = world->create_entity(vec3(0, 0, 0), NULL, NULL, controller);
    entity->set_tag(WARP_TAG("core"));

    return entity;
}
