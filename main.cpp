#define WARP_DROP_PREFIX
#include <stdio.h>

#ifdef IOS
#include "SDL.h" /* TODO: this should not be required */
#endif

#include "warp/game.h"
#include "warp/statemanager.h"

#include "level_transition.h"
#include "chat.h"
#include "version.h"

using namespace warp;

struct cliopts_t {
    bool show_version;
};

static void fill_default_options(cliopts_t *opts) {
    opts->show_version = false;
}

static void parse_options(int argc, char **argv, cliopts_t *opts) {
    fill_default_options(opts);
    for (int i = 0; i < argc; i++) {
        const char *opt = argv[i];
        if (strcmp(opt, "--version") == 0) {
            opts->show_version = true;
        }
    }
}

static void print_version() {
    printf("%s, version: %s\n", APP_NAME, VERSION);
}

static void register_loaders(resources_t *res) {
    add_chat_loader(res);
}

static int initialize_and_run() {
    transition_i *start_level = new level_transition_t;

    warp_tag_t state_tags[1] = { WARP_TAG("level") };
    statemanager_t *states = new statemanager_t(state_tags, 1);
    const warp_tag_t start = WARP_TAG(START_STATE);
    const warp_tag_t level = WARP_TAG("level");
    states->insert_transition(start, level, start_level, false);
    states->insert_transition(level, level, start_level, false);

    game_config_t config;
    fill_default_config(&config);

    config.max_entites_count = 512;
    config.window_name = APP_NAME;
    config.first_state = WARP_TAG("level");
    config.vsync_enabled = true;
    config.init_loaders = register_loaders;
    config.render_config.filter_textures = true;
    config.render_config.multisample_shadows = true;
    config.render_config.discard_invisible = true;
    config.render_config.enable_alpha_blend_in_main = true;
    config.render_config.shadow_map_size = 1024;

    game_t *game = new game_t;
    warp_result_t init_result = game->initialize(config, states);
    if (WARP_FAILED(init_result)) {
        warp_result_log("Faile to initialize game", &init_result);
        warp_result_destory(&init_result);
        return 1;
    }

    const int exit_code = game->run();
    /* on iOS the game continues to operate after main has finished */
    if (game->is_alive_after_main() == false) {
        delete game;
    }
    return exit_code;
}

int main(int argc, char **argv) {
    cliopts_t opts;
    parse_options(argc, argv, &opts);
    if (opts.show_version) {
        print_version();
        return 0;
    }

    return initialize_and_run();
}

