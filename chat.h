#pragma once

#include "warp/utils/tag.h"
#include "warp/utils/str.h"
#include "warp/utils/result.h"
#include "warp/collections/array.h"

#define MAX_RESPONSES_COUNT 3

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
    response_t responses[MAX_RESPONSES_COUNT];
} chat_entry_t;

typedef struct chat {
    warp_array_t entries;
} chat_t;

warp_result_t chat_parse(chat_t *chat, const char *file_path);
void add_chat_loader(warp_resources_t *res);



#ifdef __cplusplus
}
#endif
