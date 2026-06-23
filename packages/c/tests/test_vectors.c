#include <stdio.h>
#include <string.h>

#include "def.h"

typedef struct {
    const char *input;
    const char *encoded;
} encode_case;

typedef struct {
    const char *input;
    const char *decoded;
} decode_case;

typedef struct {
    const char *input;
    def_status expected;
} error_case;

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
    if (streq(reason, "not_decodable")) {
        return DEF_ERR_NOT_DECODABLE;
    }
    return DEF_OK;
}

static int test_encode_cases(void) {
    encode_case cases[] = {
        {"alice", "dalice"},
        {"Alice", "dalice"},
        {"USER@example.COM", "duser-40example-2ecom"},
        {"Laptop.US-East", "dlaptop-2eus-2deast"},
        {"alice-1", "dalice-2d1"},
        {"ssh", "dssh"},
        {"postgres", "dpostgres"},
        {"user@example.com/db1.us-east/accounts-db",
         "duser-40example-2ecom-2fdb1-2eus-2deast-2faccounts-2ddb"},
        {"用户", "d-e7-94-a8-e6-88-b7"},
        {"😊", "d-f0-9f-98-8a"},
        {"", "d"},
        {"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
         "daaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        char out[128];
        size_t out_len = 0;
        def_status status = def_encode(cases[i].input, out, sizeof(out), &out_len);
        if (status != DEF_OK) {
            fprintf(stderr, "encode failed for %s: %d\n", cases[i].input, status);
            return 1;
        }
        if (!streq(out, cases[i].encoded)) {
            fprintf(stderr, "encode(%s) = %s, want %s\n", cases[i].input, out, cases[i].encoded);
            return 1;
        }
    }
    return 0;
}

static int test_encode_hash_cases(void) {
    encode_case cases[] = {
        {"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
         "hfmz7982xfprnqkjav7p0cp7ak3hz0vqeswbb9hqzybd4azew5wt0"},
        {"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
         "hzzg59zktw35pvhjw7bwvc7aj17t3k18xpgygq9csecvxy5a6d3ng"},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        char out[128];
        size_t out_len = 0;
        def_status status = def_encode(cases[i].input, out, sizeof(out), &out_len);
        if (status != DEF_OK) {
            fprintf(stderr, "encode failed for %s: %d\n", cases[i].input, status);
            return 1;
        }
        if (!streq(out, cases[i].encoded)) {
            fprintf(stderr, "encode(%s) = %s, want %s\n", cases[i].input, out, cases[i].encoded);
            return 1;
        }
    }
    return 0;
}

static int test_decode_cases(void) {
    decode_case cases[] = {
        {"dalice", "alice"},
        {"duser-40example-2ecom", "user@example.com"},
        {"duser-40example-2ecom-2fdb1-2eus-2deast-2faccounts-2ddb",
         "user@example.com/db1.us-east/accounts-db"},
        {"d-e7-94-a8-e6-88-b7", "用户"},
        {"d", ""},
        {"daaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
         "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        char out[128];
        size_t out_len = 0;
        def_status status = def_decode(cases[i].input, out, sizeof(out), &out_len);
        if (status != DEF_OK) {
            fprintf(stderr, "decode failed for %s: %d\n", cases[i].input, status);
            return 1;
        }
        if (!streq(out, cases[i].decoded)) {
            fprintf(stderr, "decode(%s) = %s, want %s\n", cases[i].input, out, cases[i].decoded);
            return 1;
        }
    }
    return 0;
}

static int test_decode_errors(void) {
    struct {
        const char *input;
        def_status expected;
    } cases[] = {
        {"alice", DEF_ERR_INVALID_ENCODING},
        {"dabc-", DEF_ERR_INVALID_ESCAPE},
        {"d-gg", DEF_ERR_INVALID_ESCAPE},
        {"d-c0-80", DEF_ERR_INVALID_UTF8},
        {"daaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
         DEF_ERR_LABEL_TOO_LONG},
        {"hfmz7982xfprnqkjav7p0cp7ak3hz0vqeswbb9hqzybd4azew5wt0",
         DEF_ERR_NOT_DECODABLE},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        char out[128];
        size_t out_len = 0;
        def_status status = def_decode(cases[i].input, out, sizeof(out), &out_len);
        if (status != cases[i].expected) {
            fprintf(stderr, "decode error mismatch for %s: got %d want %d\n", cases[i].input, status,
                    cases[i].expected);
            return 1;
        }
    }
    return 0;
}

int main(void) {
    if (test_encode_cases() != 0) {
        return 1;
    }
    if (test_encode_hash_cases() != 0) {
        return 1;
    }
    if (test_decode_cases() != 0) {
        return 1;
    }
    if (test_decode_errors() != 0) {
        return 1;
    }
    return 0;
}
