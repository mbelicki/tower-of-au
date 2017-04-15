#define WARP_DROP_PREFIX
#include "chat.h"

#include "warp/utils/io.h"
#include "warp/resources/resources.h"
#include "libs/parson/parson.h"

static const char *DATA_DIR = "assets/data";

static void destroy_response(response_t *resp) {
    warp_str_destroy(&resp->text);
}

static void destroy_entry(void *raw_entry) {
    chat_entry_t *entry = raw_entry;
    warp_str_destroy(&entry->text);
    for (size_t i = 0; i < MAX_RESPONSES_COUNT; i++) {
        destroy_response(&entry->responses[i]);
    }
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

static warp_result_t parse_responses
        (response_t *resps, const JSON_Array *raw_resps) {
    const size_t count = json_array_get_count(raw_resps);
    if (count > MAX_RESPONSES_COUNT) {
        warp_log_d( "Maximum number of responses is %d, "
                    "found chat entry with %zu responses. "
                    "Additional responses will be ignored."
                  , (int)MAX_RESPONSES_COUNT, count
                  );
    }

    for (size_t i = 0; i < MAX_RESPONSES_COUNT; i++) {
        const JSON_Object *r = json_array_get_object(raw_resps, i);
        resps[0].text    = WARP_STR(json_object_get_string(r, "text"));
        resps[0].next_id = WARP_TAG(json_object_get_string(r, "nextId"));
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
        entry.id = WARP_TAG(json_object_get_string(e, "id"));
        entry.can_start = parse_bool(e, "canStart");
        entry.text = WARP_STR(json_object_get_string(e, "text"));

        const JSON_Array *responses = json_object_get_array(e, "responses");
        parse_responses(entry.responses, responses);

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
    loader->resource_type = WARP_RES_MESH;
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
