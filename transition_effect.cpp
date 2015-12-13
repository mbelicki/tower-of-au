#include "transition_effect.h"

#include "warp/mat4.h"
#include "warp/world.h"
#include "warp/entity.h"
#include "warp/mesh.h"
#include "warp/textures.h"

using namespace warp;

static const size_t CIRCLE_SIDES = 24;

static void generate_side
        (vertex_t *buffer, int i, float out_radius, float in_radius) {
    const float a1 = ((float)i       / CIRCLE_SIDES) * 2.0f * PI;
    const float a2 = ((float)(i + 1) / CIRCLE_SIDES) * 2.0f * PI;

    const float cos1 = cos(a1);
    const float cos2 = cos(a2);

    const float sin1 = sin(a1);
    const float sin2 = sin(a2);

    buffer[0].position = vec3(cos1 * out_radius, sin1 * out_radius, 8.0f);
    buffer[1].position = vec3(cos2 * out_radius, sin2 * out_radius, 8.0f);
    buffer[2].position = vec3(cos2 * in_radius,  sin2 * in_radius,  8.0f);

    buffer[3].position = vec3(cos2 * in_radius,  sin2 * in_radius,  8.0f);
    buffer[4].position = vec3(cos1 * in_radius,  sin1 * in_radius,  8.0f);
    buffer[5].position = vec3(cos1 * out_radius, sin1 * out_radius, 8.0f);
}

static void tessellate(vertex_t *buffer, float out_radius, float in_radius) {
    for (size_t i = 0; i < CIRCLE_SIDES; i++)  {
        generate_side(buffer + 6 * i, i, out_radius, in_radius);
    }
}

class fade_circle_controller_t final : public controller_impl_i {
    public:
        fade_circle_controller_t
            (float out_radius, float duration, bool fade_in)
              : _out_radius(out_radius)
              , _duration(duration)
              , _timer(duration)
              , _fade_in(fade_in)
              , _mesh_id(0)
        {}

        dynval_t get_property(const tag_t &) const override {
            return dynval_t::make_null();
        }

        void initialize(entity_t *owner, world_t *world) override {
            _owner = owner;
            _world = world;

            recreate_mesh(_fade_in ? 1 : 0);
        }

        void update(float dt, const input_t &) override { 
            if (_timer <= 0) {
                _world->destroy_later(_owner);
            }
            if (_timer > 0) {
                _timer -= dt;
            }
            
            const float t = ease_cubic(_timer / _duration);
            const float k = _fade_in ? t : 1 - t;
            recreate_mesh(k);
        }

        bool accepts(messagetype_t) const override { return false; }
        void handle_message(const message_t &) override { }

        void recreate_mesh(float k) {
            const size_t count = CIRCLE_SIDES * 6;
            std::unique_ptr<vertex_t[]> vertices(new vertex_t [count]);

            tessellate(vertices.get(), _out_radius, k * _out_radius);
            
            if (_mesh_id == 0) {
                add_new_mesh(std::move(vertices), count);
            } else {
                mutate_mesh(std::move(vertices), count);
            }
        }

    private:
        world_t  *_world;
        entity_t *_owner;

        float _out_radius;
        float _duration;
        float _timer;
        bool _fade_in;
        mesh_id_t _mesh_id;

        maybeunit_t add_new_mesh
                (std::unique_ptr<vertex_t[]> vertices, size_t count) {
            meshmanager_t *meshes = _world->get_resources().meshes;

            maybe_t<mesh_id_t> maybe_id
                = meshes->add_mesh_from_buffer(vertices.get(), count);
            MAYBE_RETURN(maybe_id, unit_t, "Adding mesh failed:")

            _mesh_id = VALUE(maybe_id);
            meshes->load_single(_mesh_id); /* force VBO creation */

            model_t *model = new model_t; /* TODO: oh hello, memory leaks */
            model->initialize(_mesh_id, 0);
            
            _owner->receive_message(MSG_GRAPHICS_ADD_MODEL, model);
            return unit;
        }

        maybeunit_t mutate_mesh
                (std::unique_ptr<vertex_t[]> vertices, size_t count) {
            meshmanager_t *meshes = _world->get_resources().meshes;

            maybeunit_t mutation_result
                = meshes->mutate_mesh(_mesh_id, vertices.get(), count);
            MAYBE_RETURN(mutation_result, unit_t, "Failed to mutate mesh:");

            return unit;
        }
};

maybe_t<entity_t *> create_fade_circle
        (world_t *world, float out_radius, float duration, bool closing) {

    controller_comp_t *controller = world->create_controller();
    controller->initialize
        (new fade_circle_controller_t(out_radius, duration, closing));

    graphics_comp_t *graphics = world->create_graphics();
    graphics->set_pass_tag("ui");

    entity_t *entity
        = world->create_entity(vec3(0, 0, 0), graphics, nullptr, controller);
    return entity;
}

