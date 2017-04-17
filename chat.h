#pragma once

#include "warp/utils/tag.h"
#include "warp/utils/str.h"
#include "warp/utils/result.h"
#include "warp/utils/random.h"
#include "warp/collections/array.h"

#define MAX_CHAT_RESPONSES_COUNT 3

#ifdef __cplusplus
extern "C" {
#endif

typedef struct warp_resources warp_resources_t;

typedef struct response {
    warp_str_t text;
    warp_tag_t next_id;
} response_t;

typedef struct chat_entry {
    warp_tag_t id;
    bool can_start;
    warp_str_t text;
    size_t responses_count;
    response_t responses[MAX_CHAT_RESPONSES_COUNT];
} chat_entry_t;

typedef struct chat {
    warp_array_t entries;
} chat_t;

const chat_entry_t *get_start_entry(const chat_t *chat, warp_random_t *rand);
const chat_entry_t *get_first_start_entry(const chat_t *chat);
const chat_entry_t *get_entry(const chat_t *chat, warp_tag_t id);

warp_result_t chat_parse(chat_t *chat, const char *file_path);
void add_chat_loader(warp_resources_t *res);
/* Lookup or load chat if not already loaded: */
const chat_t *get_chat(warp_resources_t *res, const char *name);

#ifdef __cplusplus
}
#endif
