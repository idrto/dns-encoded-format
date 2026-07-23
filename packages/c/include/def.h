#ifndef DEF_H
#define DEF_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DEF_MAX_LABEL_LENGTH 63
#define DEF_IDRTO_HASH_MARKER "idrto-h1--"
#define DEF_HASH_BODY_LENGTH 50
#define DEF_STRUCTURAL_SEPARATOR "--"

typedef enum {
    DEF_OK = 0,
    DEF_ERR_LABEL_TOO_LONG = 1,
    DEF_ERR_INVALID_ESCAPE = 2,
    DEF_ERR_INVALID_UTF8 = 3,
    DEF_ERR_BUFFER_TOO_SMALL = 4,
    DEF_ERR_INVALID_ENCODING = 5,
    DEF_ERR_INVALID_LOCATOR = 6,
    DEF_ERR_NOT_DECODABLE = 7
} def_status;

def_status def_encode_body(
    const char *input,
    char *output,
    size_t output_capacity,
    size_t *output_length
);

def_status def_decode_body(
    const char *body,
    char *output,
    size_t output_capacity,
    size_t *output_length
);

def_status def_encode_profile(
    const char *locator,
    const char *marker,
    char *output,
    size_t output_capacity,
    size_t *output_length
);

def_status def_decode_profile(
    const char *label,
    const char *marker,
    char *output,
    size_t output_capacity,
    size_t *output_length
);

def_status def_encode(
    const char *locator,
    char *output,
    size_t output_capacity,
    size_t *output_length
);

def_status def_decode(
    const char *label,
    char *output,
    size_t output_capacity,
    size_t *output_length
);

#ifdef __cplusplus
}
#endif

#endif
