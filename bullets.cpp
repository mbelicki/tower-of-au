#define WARP_DROP_PREFIX
#include "bullets.h"

#include "warp/math/mat4.h"
#include "warp/world.h"
#include "warp/entity.h"
#include "warp/textures.h"
#include "warp/meshmanager.h"
#include "warp/geometry.h"

#include "core.h"
#include "level.h"

using namespace warp;

class bullet_controller_t final : public controller_impl_i {
    public:
        bullet_controller_t(const level_t *level)
            : _level(level)
        { }

        dynval_t get_property(const warp_tag_t &) const override {
            return dynval_t::make_null();
        }

        void initialize(entity_t *owner, world_t *world) override {
            _owner = owner;
            _world = world;
        }

        void update(float, const input_t &) override { 
            const vec3_t pos = _owner->get_position();
            if (_level->is_point_walkable(pos) == false) {
                _world->destroy_later(_owner);
            }
        }

        bool accepts(messagetype_t type) const override {
            return type == MSG_PHYSICS_COLLISION_DETECTED;
        }

        void handle_message(const message_t &message) override {
            messagetype_t type = message.type;
            if (type == MSG_PHYSICS_COLLISION_DETECTED) {
                const int coll_id = message.data.get_int();
                const collision_t *coll = _world->get_physics_collision(coll_id);
                /* TODO: passing the position does not seem to be the best idea */
                entity_t *other = coll->other;
                _world->broadcast_message(CORE_BULLET_HIT, other->get_position());
                _world->destroy_later(_owner);
            }
        }

    private:
        world_t  *_world;
        entity_t *_owner;

        const level_t *_level;
};

void bullet_factory_t::initialize() {
    mesh_manager_t *meshes  = _world->get_resources().meshes;

    _mesh_id = meshes->add_mesh("arrow.obj");
    _arrow_mesh_id = meshes->add_mesh("arrow.obj");

    _initialized = true;
}

static const char *get_texture_name(bullet_type_t type) {
    (void) type;
    return "missing.png";
    //switch (type) {
    //    case BULLET_MAGIC:
    //        return "magic.png";
    //    case BULLET_FIREBALL:
    //        return "fireball.png";
    //    case BULLET_ARROW:
    //        return "arrow.png";
    //    default:
    //        return "missing.png";
    //}
}

entity_t *bullet_factory_t::create_bullet
        ( vec3_t position, vec3_t velocity, bullet_type_t type
        , const level_t *level
        ) {
    if (_initialized == false) {
        warp_log_e("Bullet factory not initialized.");
        return NULL;
    }

    const char *tex_name = get_texture_name(type);
    const tex_id_t tex_id = _world->get_resources().textures->add_texture(tex_name);

    model_t model;
    if (type == BULLET_ARROW) {
        model.initialize(_arrow_mesh_id, tex_id);
        position.y += 0.3f;
    } else {
        model.initialize(_mesh_id, tex_id);
        float transforms[16];
        mat4_fill_translation(transforms, vec3(0, 0.3f, 0));
        model.change_local_transforms(transforms);
    }

    graphics_comp_t *graphics = _world->create_graphics();
    graphics->add_model(model);

    physics_comp_t *physics = _world->create_physics();
    physics->_bounds = aa_box_t(vec3(0, 0, 0), vec3(0.2f, 0.2f, 0.2f));
    physics->_velocity = vec3(velocity.x, 0, velocity.z);
    physics->_gravity_scale = 0;

    controller_comp_t *controller = _world->create_controller();
    controller->initialize(new bullet_controller_t(level));

    entity_t *entity = _world->create_entity(position, graphics, physics, controller);
    if (entity == nullptr) {
        _world->destroy_graphics(graphics);
        _world->destroy_physics(physics);
        _world->destroy_controller(controller);
        warp_log_e("Failed to create bullet.");
        return NULL;
    }

    entity->set_tag(WARP_TAG("bullet"));

    if (type == BULLET_ARROW) {
        const vec3_t v = vec3_normalize(velocity);
        quat_t quat = quat_from_direction(v, vec3(0, 1, 0), vec3(v.z, 0, -v.x));
        entity->receive_message(MSG_PHYSICS_ROTATE, quat);
    }
    
    return entity;
}
