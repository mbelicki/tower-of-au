#define WARP_DROP_PREFIX
#include "transition_effect.h"

#include "warp/math/mat4.h"
#include "warp/math/utils.h"
#include "warp/world.h"
#include "warp/entity.h"
#include "warp/graphics/mesh.h"
#include "warp/graphics/mesh-manager.h"
#include "warp/graphics/texture-manager.h"

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

        dynval_t get_property(const warp_tag_t &) const override {
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
            vertex_t *vertices = (vertex_t *) malloc(count * sizeof *vertices);

            tessellate(vertices, _out_radius, k * _out_radius);
            
            if (_mesh_id == 0) {
                add_new_mesh(vertices, count);
            } else {
                mutate_mesh(vertices, count);
            }
            free(vertices);
        }

    private:
        world_t  *_world;
        entity_t *_owner;

        float _out_radius;
        float _duration;
        float _timer;
        bool _fade_in;
        res_id_t _mesh_id;

        void add_new_mesh(vertex_t *vertices, size_t count) {
            resources_t *res = _world->get_resources();

            _mesh_id = warp_mesh_resource_from_buffer(res, "fader", vertices, count);

            model_t *model = new model_t; /* TODO: oh hello, memory leaks */
            model_init(model, _mesh_id, 0);
            model->color = vec4(0, 0, 0, 1);
            
            _owner->receive_message(MSG_GRAPHICS_ADD_MODEL, model);
        }

        void mutate_mesh(vertex_t *vertices, size_t count) {
            resources_t *res = _world->get_resources();
            warp_mesh_resource_mutate(res, _mesh_id, vertices, count);
        }
};

entity_t *create_fade_circle
        (world_t *world, float out_radius, float duration, bool closing) {
    controller_comp_t *controller = world->create_controller();
    controller->initialize
        (new fade_circle_controller_t(out_radius, duration, closing));

    graphics_comp_t *graphics = world->create_graphics();
    graphics->remove_pass_tags();
    graphics->add_pass_tag(WARP_TAG("ui"));

    entity_t *entity = world->create_entity(vec3(0, 0, 0), graphics, NULL, controller);
    entity->set_tag(WARP_TAG("fade_circle"));

    return entity;
}

