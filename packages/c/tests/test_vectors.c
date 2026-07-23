#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "def.h"

#ifndef TEST_VECTORS_PATH
#define TEST_VECTORS_PATH "../../vectors/test-vectors.json"
#endif

typedef struct {
    char *input;
    char *value;
} vector_case;

typedef struct {
    char *input;
    char *reason;
} error_case;

typedef struct {
    vector_case *items;
    size_t count;
} vector_list;

typedef struct {
    error_case *items;
    size_t count;
} error_list;

typedef struct {
    vector_list encode_body;
    vector_list decode_body;
    error_list decode_body_errors;
    vector_list encode_profile;
    vector_list encode_profile_hash;
    error_list encode_profile_errors;
    vector_list decode_profile;
    error_list decode_profile_errors;
} test_vectors;

static int streq(const char *a, const char *b) {
    return strcmp(a, b) == 0;
}

static def_status reason_to_status(const char *reason) {
    if (streq(reason, "label_too_long")) {
        return DEF_ERR_LABEL_TOO_LONG;
    }
    if (streq(reason, "invalid_escape")) {
        return DEF_ERR_INVALID_ESCAPE;
    }
    if (streq(reason, "invalid_utf8")) {
        return DEF_ERR_INVALID_UTF8;
    }
    if (streq(reason, "invalid_encoding")) {
        return DEF_ERR_INVALID_ENCODING;
    }
    if (streq(reason, "invalid_locator")) {
        return DEF_ERR_INVALID_LOCATOR;
    }
    if (streq(reason, "not_decodable")) {
        return DEF_ERR_NOT_DECODABLE;
    }
    return DEF_OK;
}

static void free_vector_list(vector_list *list) {
    for (size_t i = 0; i < list->count; i++) {
        free(list->items[i].input);
        free(list->items[i].value);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
}

static void free_error_list(error_list *list) {
    for (size_t i = 0; i < list->count; i++) {
        free(list->items[i].input);
        free(list->items[i].reason);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
}

static void free_vectors(test_vectors *vectors) {
    free_vector_list(&vectors->encode_body);
    free_vector_list(&vectors->decode_body);
    free_error_list(&vectors->decode_body_errors);
    free_vector_list(&vectors->encode_profile);
    free_vector_list(&vectors->encode_profile_hash);
    free_error_list(&vectors->encode_profile_errors);
    free_vector_list(&vectors->decode_profile);
    free_error_list(&vectors->decode_profile_errors);
}

static char *read_file(const char *path) {
    FILE *file = fopen(path, "rb");
    long size;
    char *buffer;

    if (file == NULL) {
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }

    size = ftell(file);
    if (size < 0) {
        fclose(file);
        return NULL;
    }

    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }

    buffer = (char *)malloc((size_t)size + 1);
    if (buffer == NULL) {
        fclose(file);
        return NULL;
    }

    if (fread(buffer, 1, (size_t)size, file) != (size_t)size) {
        free(buffer);
        fclose(file);
        return NULL;
    }

    buffer[size] = '\0';
    fclose(file);
    return buffer;
}

static const char *skip_ws(const char *json) {
    while (*json == ' ' || *json == '\t' || *json == '\n' || *json == '\r') {
        json++;
    }
    return json;
}

static char *parse_json_string(const char **json) {
    const char *cursor = skip_ws(*json);
    size_t capacity = 64;
    size_t len = 0;
    char *out;

    if (*cursor != '"') {
        return NULL;
    }
    cursor++;

    out = (char *)malloc(capacity);
    if (out == NULL) {
        return NULL;
    }

    while (*cursor != '\0' && *cursor != '"') {
        char ch = *cursor++;

        if (ch == '\\') {
            if (*cursor == '\0') {
                free(out);
                return NULL;
            }
            ch = *cursor++;
            switch (ch) {
                case '"':
                case '\\':
                case '/':
                    break;
                case 'b':
                    ch = '\b';
                    break;
                case 'f':
                    ch = '\f';
                    break;
                case 'n':
                    ch = '\n';
                    break;
                case 'r':
                    ch = '\r';
                    break;
                case 't':
                    ch = '\t';
                    break;
                case 'u': {
                    unsigned int code = 0;
                    for (int i = 0; i < 4; i++) {
                        char digit = cursor[i];
                        code <<= 4;
                        if (digit >= '0' && digit <= '9') {
                            code |= (unsigned int)(digit - '0');
                        } else if (digit >= 'a' && digit <= 'f') {
                            code |= (unsigned int)(digit - 'a' + 10);
                        } else if (digit >= 'A' && digit <= 'F') {
                            code |= (unsigned int)(digit - 'A' + 10);
                        } else {
                            free(out);
                            return NULL;
                        }
                    }
                    cursor += 4;
                    if (code <= 0x7f) {
                        ch = (char)code;
                    } else if (code <= 0x7ff) {
                        if (len + 2 >= capacity) {
                            capacity *= 2;
                            out = (char *)realloc(out, capacity);
                            if (out == NULL) {
                                return NULL;
                            }
                        }
                        out[len++] = (char)(0xc0 | (code >> 6));
                        ch = (char)(0x80 | (code & 0x3f));
                    } else {
                        if (len + 3 >= capacity) {
                            capacity *= 2;
                            out = (char *)realloc(out, capacity);
                            if (out == NULL) {
                                return NULL;
                            }
                        }
                        out[len++] = (char)(0xe0 | (code >> 12));
                        out[len++] = (char)(0x80 | ((code >> 6) & 0x3f));
                        ch = (char)(0x80 | (code & 0x3f));
                    }
                    break;
                }
                default:
                    free(out);
                    return NULL;
            }
        }

        if (len + 1 >= capacity) {
            capacity *= 2;
            out = (char *)realloc(out, capacity);
            if (out == NULL) {
                return NULL;
            }
        }
        out[len++] = ch;
    }

    if (*cursor != '"') {
        free(out);
        return NULL;
    }

    out[len] = '\0';
    *json = cursor + 1;
    return out;
}

static const char *find_key_array(const char *json, const char *key) {
    char search[128];
    snprintf(search, sizeof(search), "\"%s\"", key);
    return strstr(json, search);
}

static int parse_vector_cases(const char *json, vector_list *list) {
    const char *cursor = json;
    size_t capacity = 8;
    size_t count = 0;

    list->items = (vector_case *)calloc(capacity, sizeof(vector_case));
    if (list->items == NULL) {
        return 1;
    }

    cursor = strchr(cursor, '[');
    if (cursor == NULL) {
        return 1;
    }
    cursor++;

    while (1) {
        char *input = NULL;
        char *value = NULL;
        char *field;

        cursor = skip_ws(cursor);
        if (*cursor == ']') {
            break;
        }
        if (*cursor != '{') {
            return 1;
        }
        cursor++;

        input = NULL;
        value = NULL;

        while (1) {
            char *key;
            cursor = skip_ws(cursor);
            if (*cursor == '}') {
                cursor++;
                break;
            }

            key = parse_json_string(&cursor);
            if (key == NULL) {
                free(input);
                free(value);
                return 1;
            }

            cursor = skip_ws(cursor);
            if (*cursor != ':') {
                free(key);
                free(input);
                free(value);
                return 1;
            }
            cursor++;

            field = parse_json_string(&cursor);
            if (field == NULL) {
                free(key);
                free(input);
                free(value);
                return 1;
            }

            if (streq(key, "input")) {
                free(input);
                input = field;
            } else if (streq(key, "encoded") || streq(key, "decoded")) {
                free(value);
                value = field;
            } else {
                free(field);
            }
            free(key);

            cursor = skip_ws(cursor);
            if (*cursor == ',') {
                cursor++;
            }
        }

        if (input == NULL || value == NULL) {
            free(input);
            free(value);
            return 1;
        }

        if (count >= capacity) {
            capacity *= 2;
            list->items = (vector_case *)realloc(list->items, capacity * sizeof(vector_case));
            if (list->items == NULL) {
                free(input);
                free(value);
                return 1;
            }
        }

        list->items[count].input = input;
        list->items[count].value = value;
        count++;

        cursor = skip_ws(cursor);
        if (*cursor == ',') {
            cursor++;
        }
    }

    list->count = count;
    return 0;
}

static int parse_error_cases(const char *json, error_list *list) {
    const char *cursor = json;
    size_t capacity = 8;
    size_t count = 0;

    list->items = (error_case *)calloc(capacity, sizeof(error_case));
    if (list->items == NULL) {
        return 1;
    }

    cursor = strchr(cursor, '[');
    if (cursor == NULL) {
        return 1;
    }
    cursor++;

    while (1) {
        char *input = NULL;
        char *reason = NULL;

        cursor = skip_ws(cursor);
        if (*cursor == ']') {
            break;
        }
        if (*cursor != '{') {
            return 1;
        }
        cursor++;

        while (1) {
            char *key;
            char *field;
            cursor = skip_ws(cursor);
            if (*cursor == '}') {
                cursor++;
                break;
            }

            key = parse_json_string(&cursor);
            if (key == NULL) {
                free(input);
                free(reason);
                return 1;
            }

            cursor = skip_ws(cursor);
            if (*cursor != ':') {
                free(key);
                free(input);
                free(reason);
                return 1;
            }
            cursor++;

            field = parse_json_string(&cursor);
            if (field == NULL) {
                free(key);
                free(input);
                free(reason);
                return 1;
            }

            if (streq(key, "input")) {
                free(input);
                input = field;
            } else if (streq(key, "reason")) {
                free(reason);
                reason = field;
            } else {
                free(field);
            }
            free(key);

            cursor = skip_ws(cursor);
            if (*cursor == ',') {
                cursor++;
            }
        }

        if (input == NULL || reason == NULL) {
            free(input);
            free(reason);
            return 1;
        }

        if (count >= capacity) {
            capacity *= 2;
            list->items = (error_case *)realloc(list->items, capacity * sizeof(error_case));
            if (list->items == NULL) {
                free(input);
                free(reason);
                return 1;
            }
        }

        list->items[count].input = input;
        list->items[count].reason = reason;
        count++;

        cursor = skip_ws(cursor);
        if (*cursor == ',') {
            cursor++;
        }
    }

    list->count = count;
    return 0;
}

static int load_vectors(const char *path, test_vectors *vectors) {
    char *json = read_file(path);
    const char *section;

    if (json == NULL) {
        fprintf(stderr, "failed to read vectors file: %s\n", path);
        return 1;
    }

    section = find_key_array(json, "encode_body");
    if (section == NULL || parse_vector_cases(section, &vectors->encode_body) != 0) {
        free(json);
        return 1;
    }

    section = find_key_array(json, "decode_body");
    if (section == NULL || parse_vector_cases(section, &vectors->decode_body) != 0) {
        free(json);
        return 1;
    }

    section = find_key_array(json, "decode_body_errors");
    if (section == NULL || parse_error_cases(section, &vectors->decode_body_errors) != 0) {
        free(json);
        return 1;
    }

    section = find_key_array(json, "encode_profile");
    if (section == NULL || parse_vector_cases(section, &vectors->encode_profile) != 0) {
        free(json);
        return 1;
    }

    section = find_key_array(json, "encode_profile_hash");
    if (section == NULL || parse_vector_cases(section, &vectors->encode_profile_hash) != 0) {
        free(json);
        return 1;
    }

    section = find_key_array(json, "encode_profile_errors");
    if (section == NULL || parse_error_cases(section, &vectors->encode_profile_errors) != 0) {
        free(json);
        return 1;
    }

    section = find_key_array(json, "decode_profile");
    if (section == NULL || parse_vector_cases(section, &vectors->decode_profile) != 0) {
        free(json);
        return 1;
    }

    section = find_key_array(json, "decode_profile_errors");
    if (section == NULL || parse_error_cases(section, &vectors->decode_profile_errors) != 0) {
        free(json);
        return 1;
    }

    free(json);
    return 0;
}

static int test_encode_body_cases(const vector_list *cases) {
    for (size_t i = 0; i < cases->count; i++) {
        char out[256];
        size_t out_len = 0;
        def_status status = def_encode_body(cases->items[i].input, out, sizeof(out), &out_len);
        if (status != DEF_OK) {
            fprintf(stderr, "encode_body failed for %s: %d\n", cases->items[i].input, status);
            return 1;
        }
        if (!streq(out, cases->items[i].value)) {
            fprintf(stderr, "encode_body(%s) = %s, want %s\n", cases->items[i].input, out,
                    cases->items[i].value);
            return 1;
        }
    }
    return 0;
}

static int test_decode_body_cases(const vector_list *cases) {
    for (size_t i = 0; i < cases->count; i++) {
        char out[256];
        size_t out_len = 0;
        def_status status = def_decode_body(cases->items[i].input, out, sizeof(out), &out_len);
        if (status != DEF_OK) {
            fprintf(stderr, "decode_body failed for %s: %d\n", cases->items[i].input, status);
            return 1;
        }
        if (!streq(out, cases->items[i].value)) {
            fprintf(stderr, "decode_body(%s) = %s, want %s\n", cases->items[i].input, out,
                    cases->items[i].value);
            return 1;
        }
    }
    return 0;
}

static int test_decode_body_errors(const error_list *cases) {
    for (size_t i = 0; i < cases->count; i++) {
        char out[256];
        size_t out_len = 0;
        def_status status = def_decode_body(cases->items[i].input, out, sizeof(out), &out_len);
        def_status expected = reason_to_status(cases->items[i].reason);
        if (status != expected) {
            fprintf(stderr, "decode_body error mismatch for %s: got %d want %d\n", cases->items[i].input, status,
                    expected);
            return 1;
        }
    }
    return 0;
}

static int test_encode_profile_cases(const vector_list *cases) {
    for (size_t i = 0; i < cases->count; i++) {
        char out[256];
        size_t out_len = 0;
        def_status status =
            def_encode_profile(cases->items[i].input, DEF_IDRTO_HASH_MARKER, out, sizeof(out), &out_len);
        if (status != DEF_OK) {
            fprintf(stderr, "encode_profile failed for %s: %d\n", cases->items[i].input, status);
            return 1;
        }
        if (!streq(out, cases->items[i].value)) {
            fprintf(stderr, "encode_profile(%s) = %s, want %s\n", cases->items[i].input, out,
                    cases->items[i].value);
            return 1;
        }
    }
    return 0;
}

static int test_encode_profile_hash_cases(const vector_list *cases) {
    for (size_t i = 0; i < cases->count; i++) {
        char out[256];
        size_t out_len = 0;
        def_status status =
            def_encode_profile(cases->items[i].input, DEF_IDRTO_HASH_MARKER, out, sizeof(out), &out_len);
        if (status != DEF_OK) {
            fprintf(stderr, "encode_profile hash failed for %s: %d\n", cases->items[i].input, status);
            return 1;
        }
        if (!streq(out, cases->items[i].value)) {
            fprintf(stderr, "encode_profile hash(%s) = %s, want %s\n", cases->items[i].input, out,
                    cases->items[i].value);
            return 1;
        }
    }
    return 0;
}

static int test_encode_profile_errors(const error_list *cases) {
    for (size_t i = 0; i < cases->count; i++) {
        char out[256];
        size_t out_len = 0;
        def_status status =
            def_encode_profile(cases->items[i].input, DEF_IDRTO_HASH_MARKER, out, sizeof(out), &out_len);
        def_status expected = reason_to_status(cases->items[i].reason);
        if (status != expected) {
            fprintf(stderr, "encode_profile error mismatch for %s: got %d want %d\n", cases->items[i].input, status,
                    expected);
            return 1;
        }
    }
    return 0;
}

static int test_decode_profile_cases(const vector_list *cases) {
    for (size_t i = 0; i < cases->count; i++) {
        char out[256];
        size_t out_len = 0;
        def_status status =
            def_decode_profile(cases->items[i].input, DEF_IDRTO_HASH_MARKER, out, sizeof(out), &out_len);
        if (status != DEF_OK) {
            fprintf(stderr, "decode_profile failed for %s: %d\n", cases->items[i].input, status);
            return 1;
        }
        if (!streq(out, cases->items[i].value)) {
            fprintf(stderr, "decode_profile(%s) = %s, want %s\n", cases->items[i].input, out,
                    cases->items[i].value);
            return 1;
        }
    }
    return 0;
}

static int test_decode_profile_errors(const error_list *cases) {
    for (size_t i = 0; i < cases->count; i++) {
        char out[256];
        size_t out_len = 0;
        def_status status =
            def_decode_profile(cases->items[i].input, DEF_IDRTO_HASH_MARKER, out, sizeof(out), &out_len);
        def_status expected = reason_to_status(cases->items[i].reason);
        if (status != expected) {
            fprintf(stderr, "decode_profile error mismatch for %s: got %d want %d\n", cases->items[i].input, status,
                    expected);
            return 1;
        }
    }
    return 0;
}

int main(void) {
    test_vectors vectors;
    const char *paths[] = {
        TEST_VECTORS_PATH,
        "../../vectors/test-vectors.json",
        "../../../vectors/test-vectors.json",
        "vectors/test-vectors.json",
    };
    int loaded = 0;

    memset(&vectors, 0, sizeof(vectors));

    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
        if (load_vectors(paths[i], &vectors) == 0) {
            loaded = 1;
            break;
        }
        free_vectors(&vectors);
        memset(&vectors, 0, sizeof(vectors));
    }

    if (!loaded) {
        fprintf(stderr, "failed to load test vectors\n");
        return 1;
    }

    if (test_encode_body_cases(&vectors.encode_body) != 0) {
        free_vectors(&vectors);
        return 1;
    }
    if (test_decode_body_cases(&vectors.decode_body) != 0) {
        free_vectors(&vectors);
        return 1;
    }
    if (test_decode_body_errors(&vectors.decode_body_errors) != 0) {
        free_vectors(&vectors);
        return 1;
    }
    if (test_encode_profile_cases(&vectors.encode_profile) != 0) {
        free_vectors(&vectors);
        return 1;
    }
    if (test_encode_profile_hash_cases(&vectors.encode_profile_hash) != 0) {
        free_vectors(&vectors);
        return 1;
    }
    if (test_encode_profile_errors(&vectors.encode_profile_errors) != 0) {
        free_vectors(&vectors);
        return 1;
    }
    if (test_decode_profile_cases(&vectors.decode_profile) != 0) {
        free_vectors(&vectors);
        return 1;
    }
    if (test_decode_profile_errors(&vectors.decode_profile_errors) != 0) {
        free_vectors(&vectors);
        return 1;
    }

    free_vectors(&vectors);
    return 0;
}
