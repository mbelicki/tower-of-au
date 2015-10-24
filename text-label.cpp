#include "text-label.h"

#include "warp/mat4.h"
#include "warp/world.h"
#include "warp/entity.h"
#include "warp/mesh.h"
#include "warp/textures.h"

#include "core.h"

using namespace warp;

static const char *get_known_text(known_text_t id) {
    switch (id) {
        case TEXT_GAME_OVER:
            return "GAME OVER";
        default:
            return "<unknown text id>";
    }
}

extern maybe_t<font_t *> get_default_font(const resources_t &res) {
    texturemgr_t *textures = res.textures;
    const maybe_t<tex_id_t> maybe_id = textures->add_texture("font.png");
    MAYBE_RETURN(maybe_id, font_t *, "Failed to create font:");
    
    const vec2_t size = vec2(32.0f / 512.0f, 64.0f / 512.0f);
    font_t *font = new font_t(VALUE(maybe_id), size);

    return font;
}

class label_controller_t final : public controller_impl_i {
    public:
        label_controller_t
            ( const font_t &font, float height
            , font_origin_pos_t anchor
            ) : _font(font)
              , _anchor(anchor)
              , _height(height)
              , _mesh_id(0)
        {}

        dynval_t get_property(const tag_t &) const override {
            return dynval_t::make_null();
        }

        void initialize(entity_t *owner, world_t *world) override {
            _owner = owner;
            _world = world;
        }

        void update(float, const input_t &) override { }

        bool accepts(messagetype_t type) const override {
            return type == CORE_SHOW_KNOWN_TEXT
                || type == CORE_SHOW_TAG_TEXT
                ;
        }

        void handle_message(const message_t &message) override {
            messagetype_t type = message.type;
            maybeunit_t result = unit;
            if (type == CORE_SHOW_KNOWN_TEXT) {
                const known_text_t id
                    = (known_text_t) VALUE(message.data.get_int());
                result = recreate_text_mesh(get_known_text(id));
            } else if (type == CORE_SHOW_TAG_TEXT) {
                const tag_t tag = VALUE(message.data.get_tag());
                result = recreate_text_mesh(tag.get_text());
            }

            if (result.failed()) {
                fprintf(stderr, "Teselation failed: %s", result.get_message().c_str());
            }
        }

    private:
        world_t  *_world;
        entity_t *_owner;

        font_t _font;
        
        font_origin_pos_t _anchor;
        float _height;
        mesh_id_t _mesh_id;

        maybeunit_t recreate_text_mesh(const std::string &text) {
            const size_t count = _font.calculate_buffer_size(text);
            std::unique_ptr<vertex_t[]> vertices(new (std::nothrow) vertex_t [count]);

            maybe_t<size_t> tesselation_result
                = _font.tesselate( vertices.get(), count, text
                                 , _height, vec3(0, 0, 0), _anchor
                                 );
            MAYBE_RETURN(tesselation_result, unit_t, "Failed to tesselate:")
            
            if (_mesh_id == 0) {
                add_new_mesh(std::move(vertices), count);
            } else {
                mutate_mesh(std::move(vertices), count);
            }

            return unit;
        }

        maybeunit_t add_new_mesh
                (std::unique_ptr<vertex_t[]> vertices, size_t count) {
            meshmanager_t *meshes = _world->get_resources().meshes;

            maybe_t<mesh_id_t> maybe_id
                = meshes->add_mesh_from_buffer(vertices.get(), count);
            MAYBE_RETURN(maybe_id, unit_t, "Adding mesh failed:")

            _mesh_id = VALUE(maybe_id);
            meshes->load_single(_mesh_id); /* force VBO creation */

            model_t *model = new model_t; /* TODO: oh hello, memory leaks */
            model->initialize(_mesh_id, _font.get_texture_id());

            float rot_z[16]; mat4_fill_rotation_z(rot_z, PI * 0.5f);
            float rot_y[16]; mat4_fill_rotation_y(rot_y, PI * 0.5f);
            float transforms[16]; mat4_mul(transforms, rot_y, rot_z);
            mat4_fill_rotation_y(rot_y, PI * -0.5f);
            float final_trans[16]; mat4_mul(final_trans, transforms, rot_y);           

            model->change_local_transforms(final_trans);

            _owner->receive_message(MSG_GRAPHICS_REMOVE_MODELS, 0);
            _owner->receive_message(MSG_GRAPHICS_ADD_MODEL, model);

            const vec3_t pos = _owner->get_position();
            _owner->receive_message(MSG_PHYSICS_MOVE, pos);

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

static vec3_t get_position
        (label_flags_t flags, font_origin_pos_t *origin, float font_size) {
    vec3_t position = vec3(0, 0, 0);
    *origin = FONT_CENTER;

    if ((flags & LABEL_POS_TOP) != 0) {
        position.y += 350;
        *origin = FONT_TOP_LEFT;
    }
    if ((flags & LABEL_POS_BOTTOM) != 0) {
        position.y -= 350 - font_size;
        *origin = FONT_CENTER;
    }
    if ((flags & LABEL_POS_LEFT) != 0) {
        position.x -= 460;
        *origin = FONT_TOP_LEFT;
    }
    if ((flags & LABEL_POS_RIGHT) != 0) {
        position.x += 360;
        *origin = FONT_TOP_LEFT;
    }

    return position;
}

extern maybe_t<entity_t *> create_label
        (world_t *world, const font_t &font, label_flags_t flags) {
    font_origin_pos_t origin = FONT_CENTER;
    const float size = (flags & LABEL_LARGE) != 0 ? 64 : 32;
    const vec3_t position = get_position(flags, &origin, size);

    graphics_comp_t *graphics = world->create_graphics();
    graphics->set_pass_tag("ui");

    controller_comp_t *controller = world->create_controller();
    
    label_controller_t *label_ctrl
        = new label_controller_t(font, size, origin);
    controller->initialize(label_ctrl);

    return world->create_entity(position, graphics, nullptr, controller);
}
