#define WARP_DROP_PREFIX
#include "chat.h"

#include "warp/utils/io.h"
#include "warp/utils/tokenizer.h"
#include "warp/resources/resources.h"
#include "libs/parson/parson.h"
#include "game-resources.h"

static const char *DATA_DIR = "assets/data";

/* predicate: */

enum pred_op {
    EQ = 0, NEQ, GT, GTE, LT, LTE
};

struct arg {
    bool is_literal;
    union {
        int literal_value;
        warp_str_t fact_name;
    };
};

struct predicate {
    enum   pred_op op;
    struct arg     arg1;  
    struct arg     arg2;  
};

typedef bool (*pred_operation)(int a, int b);

static bool equals          (int a, int b) { return a == b; }
static bool not_equals      (int a, int b) { return a != b; }
static bool greater_than    (int a, int b) { return a >  b; }
static bool greater_or_equal(int a, int b) { return a >= b; }
static bool lower_than      (int a, int b) { return a <  b; }
static bool lower_or_equal  (int a, int b) { return a <= b; }

pred_operation operations[] = {
    [EQ]  = equals,
    [NEQ] = not_equals,
    [GT]  = greater_than,
    [GTE] = greater_or_equal,
    [LT]  = lower_than,
    [LTE] = lower_or_equal,
};

bool parse_operator(enum pred_op *op, tokenizer_t *tok) {
    if (token_equals(tok, "==")) {
        *op = EQ;
        return true;
    } else if (token_equals(tok, "!=")) {
        *op = NEQ;
        return true;
    } else if (token_equals(tok, ">")) {
        *op = GT;
        return true;
    } else if (token_equals(tok, ">=")) {
        *op = GTE;
        return true;
    } else if (token_equals(tok, "<")) {
        *op = LT;
        return true;
    } else if (token_equals(tok, "<=")) {
        *op = LTE;
        return true;
    }
    return false;
}

bool parse_argument(struct arg *arg, tokenizer_t *tok) {
    if (token_is_int(tok)) {
        arg->is_literal = true;
        token_parse_int(tok, &arg->literal_value);
        return true;
    } else if (token_equals(tok, "true")) {
        arg->is_literal = true;
        arg->literal_value = 1;
        return true;
    } else if (token_equals(tok, "false")) {
        arg->is_literal = true;
        arg->literal_value = 0;
        return true;
    } else {
        arg->is_literal = false;
        arg->fact_name = token_parse_string(tok, false);
        return true;
    }
    return false;
}

warp_result_t parse_predicate(struct predicate *p, const char *str) {
    tokenizer_t tok;
    tokenizer_init(&tok, str, " ");
    memset(p, 0, sizeof *p);
    
    token_next(&tok, '\0');
    if (parse_argument(&p->arg1, &tok) == false) {
        char buffer[64];
        token_copy(&tok, buffer, 64);
        return warp_failure("Failed to parse '%s' as first predicate argument", buffer);
    }

    token_next(&tok, '\0');
    if (parse_operator(&p->op, &tok) == false) {
        char buffer[64];
        token_copy(&tok, buffer, 64);
        return warp_failure("Failed to parse '%s' as predicate operator", buffer);
    }

    token_next(&tok, '\0');
    if (parse_argument(&p->arg2, &tok) == false) {
        char buffer[64];
        token_copy(&tok, buffer, 64);
        return warp_failure("Failed to parse '%s' as second predicate argument.", buffer);
    }

    if (token_next(&tok, '\0')) {
        warp_log_d("Predicate '%s' has some unexpected tokens that were ignored.", str);
    }

    return warp_success();
}

static void destroy_argument(struct arg *a) {
    if (a->is_literal == false) {
        warp_str_destroy(&a->fact_name);
    }
}

static void destroy_predicate(struct predicate *p) {
    if (p == NULL) return;
    destroy_argument(&p->arg1);
    destroy_argument(&p->arg2);
}

static int get_fact(const char *name, const warp_map_t *facts) {
    if (warp_map_has_key(facts, name)) {
        return warp_map_get_value(int, facts, name);
    }
    return 0;
}

static int evaluate_argument(const struct arg *arg, const warp_map_t *facts) {
    if (arg->is_literal) {
        return arg->literal_value;
    }
    const char *name = warp_str_value(&arg->fact_name);
    return get_fact(name, facts);
}

static bool evaluate_predicate(const struct predicate *p, const warp_map_t *facts) {
    if (p == NULL) { 
        return true; 
    }
    const int a = evaluate_argument(&p->arg1, facts);
    const int b = evaluate_argument(&p->arg2, facts);
    return operations[p->op](a, b);
}

/* side effects: */

enum side_op {
    MUT = 0, INC, DEC
};

struct side_effect {
    enum side_op op;
    struct arg   arg1;
    struct arg   arg2;
};

typedef void (*side_operation)(const char *name, int value, warp_map_t *facts);

void mutate(const char *name, int value, warp_map_t *facts) {
    warp_map_insert(facts, name, &value);
}

void increment(const char *name, int value, warp_map_t *facts) {
    const int result = get_fact(name, facts) + value;
    warp_map_insert(facts, name, &result);
}

void decrement(const char *name, int value, warp_map_t *facts) {
    const int result = get_fact(name, facts) - value;
    warp_map_insert(facts, name, &result);
}

side_operation side_operations[] = {
    [MUT] = mutate,
    [INC] = increment,
    [DEC] = decrement,
};

bool parse_side_operator(enum side_op *op, tokenizer_t *tok) {
    if (token_equals(tok, "<-")) {
        *op = MUT;
        return true;
    } else if (token_equals(tok, "+=")) {
        *op = INC;
        return true;
    } else if (token_equals(tok, "-=")) {
        *op = DEC;
        return true;
    }
    return false;
}

static warp_result_t parse_side_effect(struct side_effect *se, const char *str) {
    tokenizer_t tok;
    tokenizer_init(&tok, str, " ");
    memset(se, 0, sizeof *se);
    
    token_next(&tok, '\0');
    if (parse_argument(&se->arg1, &tok) == false) {
        char buffer[64];
        token_copy(&tok, buffer, 64);
        return warp_failure("Failed to parse '%s' as side effect's fact.", buffer);
    }
    if (se->arg1.is_literal) {
        return warp_failure( "Cannot use '%d' value as side effect's fact."
                           , se->arg1.literal_value
                           );
    }

    token_next(&tok, '\0');
    if (parse_side_operator(&se->op, &tok) == false) {
        char buffer[64];
        token_copy(&tok, buffer, 64);
        return warp_failure("Failed to parse '%s' as side effect operator.", buffer);
    }

    token_next(&tok, '\0');
    if (parse_argument(&se->arg2, &tok) == false) {
        char buffer[64];
        token_copy(&tok, buffer, 64);
        return warp_failure("Failed to parse '%s' as side effect value.", buffer);
    }

    if (token_next(&tok, '\0')) {
        warp_log_d("Predicate '%s' has some unexpected tokens that were ignored.", str);
    }

    return warp_success();
}

static void destroy_side_effect(struct side_effect *se) {
    if (se == NULL) return;
    destroy_argument(&se->arg1);
    destroy_argument(&se->arg2);
}

extern void chat_entry_evaluate_side_effects
        (const chat_entry_t *entry, warp_map_t *facts) {
    if (entry->side_effect == NULL) {
        return;
    }
    struct side_effect *se = entry->side_effect;
    const char *fact = warp_str_value(&se->arg1.fact_name);
    const int value  = evaluate_argument(&se->arg2, facts);
    side_operations[se->op](fact, value, facts);
}

/* rest of the chat: */

static void destroy_response(response_t *resp) {
    warp_str_destroy(&resp->text);
}

static void destroy_entry(void *raw_entry) {
    chat_entry_t *entry = raw_entry;
    warp_str_destroy(&entry->text);
    for (size_t i = 0; i < MAX_CHAT_RESPONSES_COUNT; i++) {
        destroy_response(&entry->responses[i]);
    }

    destroy_predicate(entry->predicate);
    free(entry->predicate);

    destroy_side_effect(entry->side_effect);
    free(entry->side_effect);
}

static const chat_entry_t *find_entry
        ( const chat_t *chat, warp_random_t *rand
        , const warp_map_t *facts
        , bool (*condition)(void *, const chat_entry_t *)
        , void *condition_context
        ) {
    warp_array_t indices = warp_array_create_typed(size_t, 16, NULL);
    const size_t count = warp_array_get_size(&chat->entries);
    for (size_t i = 0; i < count; i++) {
        const chat_entry_t *entry = warp_array_get(&chat->entries, i);
        const bool meets_prerequsites = evaluate_predicate(entry->predicate, facts); 
        const bool condition_is_true  = condition(condition_context, entry);
        if (meets_prerequsites && condition_is_true) {
            warp_array_append_value(size_t, &indices, i);
        }
    }

    const size_t selected = warp_random_index(rand, &indices);
    const size_t index = warp_array_get_value(size_t, &indices, selected);

    const chat_entry_t *entry = warp_array_get(&chat->entries, index);
    return entry;
}

static bool is_start(void *ctx, const chat_entry_t *e) {
    (void)ctx;
    return e->can_start;
}

static bool has_id(void *ctx, const chat_entry_t *e) {
    const warp_tag_t *id = ctx;
    return warp_tag_equals(&e->id, id);
}

#define RETURN_IF_NULL(arg, ret) \
    if ((arg) == NULL) { \
       warp_log_e("Unexpeced null argument '" #arg "' of function '%s'.", __func__); \
       return (ret); \
    } \

extern const chat_entry_t *get_start_entry
        (const chat_t *chat, warp_random_t *rand, const warp_map_t *facts) {
    RETURN_IF_NULL(chat, NULL);
    RETURN_IF_NULL(rand, NULL);
    RETURN_IF_NULL(facts, NULL);
    return find_entry(chat, rand, facts, is_start, NULL);
}

extern const chat_entry_t *get_entry
        ( const chat_t *chat, warp_random_t *rand, const warp_map_t *facts
        , warp_tag_t id
        ) {
    RETURN_IF_NULL(chat, NULL);
    RETURN_IF_NULL(rand, NULL);
    RETURN_IF_NULL(facts, NULL);
    return find_entry(chat, rand, facts, has_id, &id);
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

static warp_result_t parse_entry_predicate
        (chat_entry_t *entry, const JSON_Object *raw_entry) {
    entry->predicate = NULL;
    if (has_json_member(raw_entry, "predicate") == false) {
        return warp_success();
    }
    
    struct predicate *pred = malloc(sizeof *pred);
    const char *str = json_object_get_string(raw_entry, "predicate");
    warp_result_t result = parse_predicate(pred, str);
    if (WARP_FAILED(result)) {
        free(pred);
        return result;
    }
    
    entry->predicate = pred;
    return warp_success();
}

static warp_result_t parse_entry_side_effect
        (chat_entry_t *entry, const JSON_Object *raw_entry) {
    entry->side_effect = NULL;
    if (has_json_member(raw_entry, "sideEffect") == false) {
        return warp_success();
    }
    
    struct side_effect *se = malloc(sizeof *se);
    const char *str = json_object_get_string(raw_entry, "sideEffect");
    warp_result_t result = parse_side_effect(se, str);
    if (WARP_FAILED(result)) {
        free(se);
        return result;
    }
    
    entry->side_effect = se;
    return warp_success();
}

static warp_result_t parse(chat_t *chat, const JSON_Object *root) {
    chat->entries = warp_array_create_typed(chat_entry_t, 16, destroy_entry);

    const JSON_Array *entries = json_object_get_array(root, "entries");
    const size_t count = json_array_get_count(entries);
    for (size_t i = 0; i < count; i++) {
        warp_result_t result = warp_success();
        const JSON_Object *e = json_array_get_object(entries, i);

        chat_entry_t entry;
        entry.id = parse_tag(e, "id");
        entry.can_start = parse_bool(e, "canStart");
        entry.text = WARP_STR(json_object_get_string(e, "text"));

        const JSON_Array *responses = json_object_get_array(e, "responses");
        result = parse_responses(&entry, responses);
        if (WARP_FAILED(result)) return result;
        
        result = parse_entry_predicate(&entry, e);
        if (WARP_FAILED(result)) return result;

        result = parse_entry_side_effect(&entry, e);
        if (WARP_FAILED(result)) return result;

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

