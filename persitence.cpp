#include "persitence.h"

#include "warp/world.h"
#include "warp/entity.h"
#include "warp/components.h"

#include "region.h"

using namespace warp;


class region_token_controller_t final : public controller_impl_i {
    public:
        region_token_controller_t(const portal_t *portal)
                : _owner(nullptr)
                , _world(nullptr)
                , _portal(*portal) {
            const size_t name_size = strnlen(portal->region_name, 256);
            char *name_buffer = new char[name_size];
            strncpy(name_buffer, portal->region_name, name_size + 1);
            _portal.region_name = name_buffer;
        }

        ~region_token_controller_t() {
            delete [] _portal.region_name;
        }

        dynval_t get_property(const tag_t &name) const override {
            if (name == "portal") {
                return (void *)&_portal;
            }

            return dynval_t::make_null();
        }

        void initialize(entity_t *owner, world_t *world) override {
            _owner = owner;
            _world = world;
        }

        void update(float, const input_t &) override { }
        bool accepts(messagetype_t) const override { return false; }
        void handle_message(const message_t &) override { }

    private:
        entity_t *_owner;
        world_t *_world;

        portal_t _portal;
};

maybe_t<entity_t *>
        create_region_token(world_t *world, const portal_t *portal) {
    if (portal == nullptr) {
        return nothing<entity_t *>("Null portal.");
    }
    controller_comp_t *controller = world->create_controller();
    controller->initialize(new region_token_controller_t(portal));

    entity_t *entity
        = world->create_entity(vec3(0, 0, 0), nullptr, nullptr, controller);
    entity->set_tag("region_data");

    return entity;
}
