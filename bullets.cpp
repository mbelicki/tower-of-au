#include "bullets.h"

#include "warp/mat4.h"
#include "warp/world.h"
#include "warp/entity.h"
#include "warp/textures.h"
#include "warp/meshmanager.h"
#include "warp/geometry.h"

#include "core.h"

using namespace warp;

class bullet_controller_t final : public controller_impl_i {
    public:
        dynval_t get_property(const tag_t &) const override {
            return dynval_t::make_null();
        }

        void initialize(entity_t *owner, world_t *world) override {
            _owner = owner;
            _world = world;
        }

        void update(float, const input_t &) override { }

        bool accepts(messagetype_t type) const override {
            return type == MSG_PHYSICS_COLLISION_DETECTED;
        }

        void handle_message(const message_t &message) override {
            messagetype_t type = message.type;
            if (type == MSG_PHYSICS_COLLISION_DETECTED) {
                entity_t *other = VALUE(message.data.get_entity());
                other->receive_message(CORE_BULLET_HIT, 1);
                _world->destroy_later(_owner);
                puts(other->get_tag().get_text());
            }
        }

    private:
        world_t  *_world;
        entity_t *_owner;
};

maybeunit_t bullet_factory_t::initialize() {
    meshmanager_t *meshes   = _world->get_resources().meshes;

    maybe_t<mesh_id_t> mesh_id = meshes->add_mesh("arrow.obj");
    MAYBE_RETURN(mesh_id, unit_t, "Failed to create mesh:");

    maybe_t<mesh_id_t> arrow_mesh_id = meshes->add_mesh("arrow.obj");
    MAYBE_RETURN(arrow_mesh_id, unit_t, "Failed to create arror mesh:");

    _mesh_id = VALUE(mesh_id);
    _arrow_mesh_id = VALUE(arrow_mesh_id);

    _initialized = true;
    
    return unit;
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

maybe_t<entity_t *> bullet_factory_t::create_bullet
        (vec3_t position, vec3_t velocity, bullet_type_t type) {
    if (_initialized == false) {
        return nothing<entity_t *>("Not initialized.");
    }

    const char *tex_name = get_texture_name(type);
    maybe_t<tex_id_t> tex_id
        = _world->get_resources().textures->add_texture(tex_name);
    MAYBE_RETURN(tex_id, entity_t *, "Failed to load bullet texture:");

    model_t model;
    if (type == BULLET_ARROW) {
        model.initialize(_arrow_mesh_id, VALUE(tex_id));
        position.y += 0.3f;
    } else {
        model.initialize(_mesh_id, VALUE(tex_id));
        float transforms[16];
        mat4_fill_translation(transforms, vec3(0, 0.3f, 0));
        model.change_local_transforms(transforms);
    }

    graphics_comp_t *graphics = _world->create_graphics();
    graphics->add_model(model);

    physics_comp_t *physics = _world->create_physics();
    physics->_bounds = rectangle_t(vec2(0, 0), vec2(0.2f, 0.2f));
    physics->_velocity = vec2(velocity.x, velocity.z);

    controller_comp_t *controller = _world->create_controller();
    controller->initialize(new bullet_controller_t);

    entity_t *entity = _world->create_entity(position, graphics, physics, controller);
    if (entity == nullptr) {
        _world->destroy_graphics(graphics);
        _world->destroy_physics(physics);
        _world->destroy_controller(controller);
        return nothing<entity_t *>("Failed to create entity.");
    }

    entity->set_tag("bullet");

    if (type == BULLET_ARROW) {
        const vec3_t v = vec3_normalize(velocity);
        quaternion_t quat = quat_from_direction(v, vec3(0, 1, 0), vec3(v.z, 0, -v.x));
        entity->receive_message(MSG_PHYSICS_ROTATE, quat);
    }
    
    return entity;
}
