#include <stdio.h>

#ifdef IOS
#include "SDL.h" /* TODO: this should not be required */
#endif

#include "warp/game.h"
#include "warp/statemanager.h"

#include "level_transition.h"

#ifndef VERSION
#define VERSION "missing version information."
#endif

#ifndef APP_NAME
#define APP_NAME "Unknown Application"
#endif

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

static int initialize_and_run() {
    std::shared_ptr<warp::transition_i> start_level(new level_transition_t);
    //std::shared_ptr<warp::transition_i> end_level(new gameover_transition_t);

    warp::statemanager_t states = {"level"};
    states.insert_transition(START_STATE, "level", start_level, false);
    states.insert_transition("level", "level", start_level, false);
    //states.insert_transition("level", END_STATE, end_level, false);

    warp::game_config_t config;
    fill_default_config(&config);

    config.max_entites_count = 512;
    config.window_name = "Tower of Au";

    warp::game_t game;
    warp::maybeunit_t maybe_initialized = game.initialize(config, states);
    if (maybe_initialized.failed()) {
        warp_log_e("Failed to initialize: %s", maybe_initialized.get_message().c_str());
        return 1;
    }

    return game.run();
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

