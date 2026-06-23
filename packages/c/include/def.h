#ifndef DEF_H
#define DEF_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DEF_MAX_LABEL_LENGTH 63

typedef enum {
    DEF_OK = 0,
    DEF_ERR_LABEL_TOO_LONG = 1,
    DEF_ERR_INVALID_ESCAPE = 2,
    DEF_ERR_INVALID_UTF8 = 3,
    DEF_ERR_BUFFER_TOO_SMALL = 4
} def_status;

def_status def_encode(
    const char *input,
    char *output,
    size_t output_capacity,
    size_t *output_length
);

def_status def_decode(
    const char *encoded,
    char *output,
    size_t output_capacity,
    size_t *output_length
);

#ifdef __cplusplus
}
#endif

#endif
