#define WARP_DROP_PREFIX
#include "chat.h"

#include "warp/utils/io.h"
#include "warp/resources/resources.h"
#include "libs/parson/parson.h"
#include "game-resources.h"

static const char *DATA_DIR = "assets/data";

static void destroy_response(response_t *resp) {
    warp_str_destroy(&resp->text);
}

static void destroy_entry(void *raw_entry) {
    chat_entry_t *entry = raw_entry;
    warp_str_destroy(&entry->text);
    for (size_t i = 0; i < MAX_CHAT_RESPONSES_COUNT; i++) {
        destroy_response(&entry->responses[i]);
    }
}

extern const chat_entry_t *get_start_entry
        (const chat_t *chat, warp_random_t *rand) {
    if (chat == NULL || rand == NULL) {
        warp_log_e("One of the arguments of get_start_entry was null.");
        return NULL;
    }

    warp_array_t indices = warp_array_create_typed(size_t, 16, NULL);
    const size_t count = warp_array_get_size(&chat->entries);
    for (size_t i = 0; i < count; i++) {
        const chat_entry_t *entry = warp_array_get(&chat->entries, i);
        if (entry->can_start) {
            warp_array_append_value(size_t, &indices, i);
        }
    }

    const size_t selected = warp_random_index(rand, &indices);
    const size_t index = warp_array_get_value(size_t, &indices, selected);

    const chat_entry_t *entry = warp_array_get(&chat->entries, index);
    return entry;
}

extern const chat_entry_t *get_first_start_entry(const chat_t *chat) {
    if (chat == NULL) {
        warp_log_e("get_start_entry called with null chat.");
        return NULL;
    }

    const size_t count = warp_array_get_size(&chat->entries);
    for (size_t i = 0; i < count; i++) {
        const chat_entry_t *entry = warp_array_get(&chat->entries, i);
        if (entry->can_start) {
            return entry;
        }
    }
    return NULL;
}

extern const chat_entry_t *get_entry(const chat_t *chat, warp_tag_t id) {
    const size_t count = warp_array_get_size(&chat->entries);
    for (size_t i = 0; i < count; i++) {
        const chat_entry_t *entry = warp_array_get(&chat->entries, i);
        if (warp_tag_equals(&entry->id, &id)) {
            return entry;
        }
    }
    return NULL;
}

static bool has_json_member(const JSON_Object *o, const char *member_name) {
    return json_object_get_value(o, member_name) != NULL;
}

static bool parse_bool(const JSON_Object *o, const char *name) {
    if (has_json_member(o, name) == false) {
        return false;
    }
    return json_object_get_boolean(o, name);
}

static warp_tag_t parse_tag(const JSON_Object *o, const char *name) {
    if (has_json_member(o, name) == false) {
        return WARP_TAG("");
    }
    return WARP_TAG(json_object_get_string(o, name));
}

static warp_result_t parse_responses
        (chat_entry_t *entry, const JSON_Array *raw_resps) {
    const size_t count = json_array_get_count(raw_resps);
    const size_t max = MAX_CHAT_RESPONSES_COUNT;
    if (count > MAX_CHAT_RESPONSES_COUNT) {
        warp_log_d( "Maximum number of responses is %zu, "
                    "found chat entry with %zu responses. "
                    "Additional responses will be ignored."
                  , max, count
                  );
    }

    entry->responses_count = count >= max ? max : count;
    for (size_t i = 0; i < entry->responses_count; i++) {
        const JSON_Object *r = json_array_get_object(raw_resps, i);
        entry->responses[i].text    = WARP_STR(json_object_get_string(r, "text"));
        entry->responses[i].next_id = parse_tag(r, "nextId");
    }

    return warp_success();
}

static warp_result_t parse(chat_t *chat, const JSON_Object *root) {
    chat->entries = warp_array_create_typed(chat_entry_t, 16, destroy_entry);

    const JSON_Array *entries = json_object_get_array(root, "entries");
    const size_t count = json_array_get_count(entries);
    for (size_t i = 0; i < count; i++) {
        const JSON_Object *e = json_array_get_object(entries, i);

        chat_entry_t entry;
        entry.id = parse_tag(e, "id");
        entry.can_start = parse_bool(e, "canStart");
        entry.text = WARP_STR(json_object_get_string(e, "text"));

        const JSON_Array *responses = json_object_get_array(e, "responses");
        parse_responses(&entry, responses);

        warp_array_append(&chat->entries, &entry, 1);
    }

    return warp_success();
}

extern warp_result_t chat_parse(chat_t *chat, const char *path) {
    warp_array_t bytes = { NULL };
    warp_result_t read_result = read_file(path, &bytes);
    if (WARP_FAILED(read_result)) {
        return warp_result_propagate("Failed to read chat file", read_result);
    }

    const char *content = (char *) warp_array_get(&bytes, 0);
    JSON_Value *root_value = json_parse_string(content);
    if (json_value_get_type(root_value) != JSONObject) {
       return warp_failure("Cannot parse %s: root element is not an object.", path);
    }

    const JSON_Object *root = json_value_get_object(root_value);
    warp_result_t result = parse(chat, root);
    json_value_free(root_value);
    return result;
}

struct chat_record {
    chat_t chat;
};

static warp_result_t load(void *ctx, const char *path, void *raw_record) {
    (void)ctx;
    struct chat_record *record = raw_record;
    return chat_parse(&record->chat, path);
}

static warp_result_t unload(void *ctx, void *raw_record) {
    (void)ctx;
    struct chat_record *record = raw_record;
    warp_array_destroy(&record->chat.entries);
    return warp_success();
}

static void init_chat_loader(warp_loader_t *loader) {
    memset(loader, 0, sizeof *loader);
    loader->resource_type = (int)RES_CHAT;
    loader->file_extension = "chat.json";
    loader->asset_directory = DATA_DIR;
    loader->record_size = sizeof (struct chat_record);
    loader->context = NULL;
    loader->load = load;
    loader->unload = unload;
    loader->deinit = NULL;
}

extern void add_chat_loader(resources_t *res) {
    warp_loader_t loader;
    init_chat_loader(&loader);
    resources_add_loader(res, &loader);
}

extern const chat_t *get_chat(warp_resources_t *res, const char *name) {
    if (res == NULL) {
        warp_log_e("Get chat called with null resources.");
        return NULL;
    }
    if (name == NULL || name[0] == '\0') {
        warp_log_e("Get chat called with null or empty name.");
        return NULL;
    }

    res_id_t id = resources_lookup(res, name);
    if (id == WARP_RES_ID_INVALID) {
        id = resources_load(res, name);
        if (id == WARP_RES_ID_INVALID) {
            warp_log_e("Failed to get chat resource: '%s'.", name);
            return NULL;
        }
    }

    const struct chat_record *record = resources_get_data(res, id);
    if (record == NULL) {
        warp_log_e( "Unexpected failure to get chat record for: '%s' (id: %d)" 
                  , name, id
                  );
    }
    return &record->chat;
}

