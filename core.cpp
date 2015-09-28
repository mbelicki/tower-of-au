#include "core.h"

#include <stdlib.h>
#include <time.h>

#include "warp/world.h"
#include "warp/entity.h"
#include "warp/components.h"
#include "warp/direction.h"
#include "warp/entity-helpers.h"

#include "level.h"
#include "character.h"

using namespace warp;

struct character_t {
    entity_t *entity;

    vec3_t position;
    dir_t direction;
};

static maybe_t<entity_t *> create_npc(vec3_t position, world_t *world) {
    maybe_t<graphics_comp_t *> graphics
        = create_single_model_graphics(world, "npc.obj", "missing.png");
    maybe_t<controller_comp_t *> controller
        = create_character_controller(world);

    return world->create_entity(position, VALUE(graphics), nullptr, VALUE(controller));
}

static dir_t get_move_direction(move_dir_t move_dir) {
    switch (move_dir) {
        case MOVE_UP:
            return DIR_Z_MINUS;
        case MOVE_DOWN:
            return DIR_Z_PLUS;
        case MOVE_LEFT:
            return DIR_X_MINUS;
        case MOVE_RIGHT:
            return DIR_X_PLUS;
    }
}

static vec3_t move_character(const character_t &character, move_dir_t dir) {
    const dir_t direction = get_move_direction(dir);
    const vec3_t step = dir_to_vec3(direction);
    return vec3_add(character.position, step);
}

static int calculate_damage 
        (const character_t &, attack_element_t) {
    return 1;
}

static bool is_avatar_idle(const character_t &character) {
    const dynval_t is_idle = character.entity->get_property("avat.is_idle");
    return VALUE(is_idle.get_int()) == 1;
}

static void spawn_npcs(const level_t &level, character_t **npcs, world_t *world) {
    const size_t width = level.get_width();
    const size_t height = level.get_height();
    srand(time(NULL));
    for (size_t i = 0; i < width; i++) {
        for (size_t j = 0; j < height; j++) {
            maybe_t<const tile_t *> maybe_tile = level.get_tile_at(i, j);
            if (maybe_tile.failed()) continue;
            const tile_t *tile = VALUE(maybe_tile);

            if (tile->spawn_probablity > 0) {
                const float r = rand() / (float)RAND_MAX;
                if (r <= tile->spawn_probablity) {
                    const vec3_t position = vec3(i, 0, j);
                    maybe_t<entity_t *> entity = create_npc(position, world);

                    character_t *character = new (std::nothrow) character_t;
                    character->position = position;
                    character->direction = DIR_Z_PLUS;
                    character->entity = VALUE(entity);
                    npcs[i + width * j] = character;
                }
            }
        }
    }
}

class core_controller_t final : public controller_impl_i {
    public:
        core_controller_t(level_t *level)
            : _level(level)
            , _npcs(nullptr)
        {
        }

        ~core_controller_t() {
            for (size_t i = 0; i < _max_npc_count; i++) {
                delete _npcs[i];
            }
            delete _npcs;
        }

        dynval_t get_property(const tag_t &) const override {
            return dynval_t::make_null();
        }

        void initialize(entity_t *owner, world_t *world) override {
            _owner = owner;
            _world = world;

            maybe_t<entity_t *> avatar = create_npc(vec3(8, 0, 8), world);
            _player.entity = VALUE(avatar);
            _player.position = vec3(8, 0, 8);
            _player.direction = DIR_Z_MINUS;

            const size_t width  = _level->get_width();
            const size_t height = _level->get_height();
            _max_npc_count = width * height;
            _npcs = new (std::nothrow) character_t * [_max_npc_count];
            for (size_t i = 0; i < _max_npc_count; i++) {
                _npcs[i] = nullptr;
            }

            spawn_npcs(*_level, _npcs, world);
        }

        void update(float, const input_t &) override { }

        bool accepts(messagetype_t type) const override {
            return type == CORE_TRY_MOVE;
        }

        void handle_message(const message_t &message) override {
            const messagetype_t type = message.type;
            const bool avatar_is_idle = is_avatar_idle(_player);
            if (avatar_is_idle) {
                if (type == CORE_TRY_MOVE) {
                    const int direction = VALUE(message.data.get_int());
                    handle_move((move_dir_t)direction);
                }
            }
        }

    private:
        entity_t *_owner;
        world_t *_world;

        level_t *_level;

        character_t _player;
        character_t **_npcs;
        size_t _max_npc_count;

        bool can_move_to(vec3_t position) {
            const size_t x = round(position.x);
            const size_t z = round(position.z);

            maybe_t<const tile_t *> maybe_tile = _level->get_tile_at(x, z);
            if (maybe_tile.failed())
                return false;

            const tile_t *tile = VALUE(maybe_tile);
            const character_t *npc = npc_at(x, z);
            return tile->is_walkable && npc == nullptr;
        }

        character_t *npc_at_position(vec3_t position) {
            const size_t x = round(position.x);
            const size_t z = round(position.z);
            return npc_at(x, z);
        }

        character_t *npc_at(size_t x, size_t y) {
            const size_t width  = _level->get_width();
            const size_t height = _level->get_height();
            if (x >= width || y >= height)
                return nullptr;

            return _npcs[x + width * y];
        }

        void handle_move(move_dir_t direction) {
            _player.direction = get_move_direction(direction);
            const vec3_t position = move_character(_player, direction);
            if (can_move_to(position)) {
                _player.position = position;
                _player.entity->receive_message(CORE_DO_MOVE, position);
            } else {
                character_t *npc = npc_at_position(position);
                if (npc != nullptr) {
                    handle_attack(npc, ATTACK_ELEM_STEEL);
                    puts("attaque!");
                }
            }

            next_turn();
        }

        void handle_attack(character_t *c, attack_element_t element) {
            int damage = calculate_damage(*c, element);
            hurt_character(c, damage);
        }

        void hurt_character(character_t *c, int damage) {
            if (c == nullptr || damage <= 0) return;
            const size_t x = round(c->position.x);
            const size_t z = round(c->position.z);
            const size_t level_width = _level->get_width();

            _npcs[x + level_width * z] = nullptr;
            _world->destroy_later(c->entity);
            delete c;
        }

        void next_turn() {
            const size_t width  = _level->get_width();
            const size_t height = _level->get_height();
            const size_t max_characters = width * height;
            
            size_t count = 0;
            character_t **buffer = new character_t * [max_characters];

            for (size_t x = 0; x < width; x++) {
                for (size_t y = 0; y < height; y++) {
                    character_t *npc = npc_at(x, y);
                    if (npc != nullptr) {
                        buffer[count] = npc;
                        count++;
                    }
                }
            }

            for (size_t i = 0; i < count; i++) {
                update_npc(buffer[i]);
            }

            delete buffer;
        }

        void move_npc(character_t *npc, vec3_t position) {
            const vec3_t old_position = npc->position;
            const size_t level_width = _level->get_width();

            size_t x = round(old_position.x);
            size_t z = round(old_position.z);
            _npcs[x + level_width * z] = nullptr;

            x = round(position.x);
            z = round(position.z);
            _npcs[x + level_width * z] = npc;

            npc->position = position;
            npc->entity->receive_message(CORE_DO_MOVE, position);
        }

        void update_npc(character_t *npc) {
            const vec3_t position
                = vec3_add(npc->position, dir_to_vec3(npc->direction));
            if (can_move_to(position)) {
                move_npc(npc, position);
            }
        }
};

extern maybe_t<entity_t *> create_core(world_t *world, level_t * level) {
    if (level == nullptr) {
        return nothing<entity_t *>("Level is null.");
    }

    controller_comp_t *controller = world->create_controller();
    controller->initialize(new core_controller_t(level));

    entity_t *entity = world->create_entity(vec3(0, 0, 0), nullptr, nullptr, controller);

    return entity;
}
