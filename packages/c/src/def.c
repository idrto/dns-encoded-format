#include "def.h"

#include <stdint.h>
#include <string.h>

static int is_literal_byte(unsigned char byte) {
    return (byte >= 0x61 && byte <= 0x7a) || (byte >= 0x30 && byte <= 0x39);
}

static def_status append_char(char *output, size_t *len, size_t cap, char ch) {
    if (*len + 1 > DEF_MAX_LABEL_LENGTH) {
        return DEF_ERR_LABEL_TOO_LONG;
    }
    if (*len + 1 >= cap) {
        return DEF_ERR_BUFFER_TOO_SMALL;
    }
    output[*len] = ch;
    (*len)++;
    return DEF_OK;
}

static def_status append_escape(char *output, size_t *len, size_t cap, unsigned char byte) {
    char esc[4];
    static const char hex[] = "0123456789abcdef";
    esc[0] = '-';
    esc[1] = hex[(byte >> 4) & 0x0f];
    esc[2] = hex[byte & 0x0f];
    esc[3] = '\0';

    if (*len + 3 > DEF_MAX_LABEL_LENGTH) {
        return DEF_ERR_LABEL_TOO_LONG;
    }
    if (*len + 3 >= cap) {
        return DEF_ERR_BUFFER_TOO_SMALL;
    }

    output[*len] = esc[0];
    output[*len + 1] = esc[1];
    output[*len + 2] = esc[2];
    *len += 3;
    return DEF_OK;
}

static def_status canonicalize_copy(
    const char *input,
    char *output,
    size_t output_capacity,
    size_t *output_length
) {
    size_t in_len = strlen(input);
    size_t out_len = 0;

    for (size_t i = 0; i < in_len; i++) {
        unsigned char ch = (unsigned char)input[i];
        if (ch >= 'A' && ch <= 'Z') {
            ch = (unsigned char)(ch + 32);
        }
        if (out_len + 1 >= output_capacity) {
            return DEF_ERR_BUFFER_TOO_SMALL;
        }
        output[out_len++] = (char)ch;
    }

    output[out_len] = '\0';
    *output_length = out_len;
    return DEF_OK;
}

static int parse_hex_byte(unsigned char h1, unsigned char h2, unsigned char *out) {
    int hi;
    int lo;

    if (h1 >= '0' && h1 <= '9') {
        hi = h1 - '0';
    } else if (h1 >= 'a' && h1 <= 'f') {
        hi = h1 - 'a' + 10;
    } else {
        return 0;
    }

    if (h2 >= '0' && h2 <= '9') {
        lo = h2 - '0';
    } else if (h2 >= 'a' && h2 <= 'f') {
        lo = h2 - 'a' + 10;
    } else {
        return 0;
    }

    *out = (unsigned char)(hi * 16 + lo);
    return 1;
}

static int utf8_validate(const unsigned char *bytes, size_t len) {
    size_t i = 0;

    while (i < len) {
        unsigned char b = bytes[i];

        if (b <= 0x7f) {
            i++;
        } else if ((b & 0xe0) == 0xc0) {
            if (i + 1 >= len || (bytes[i + 1] & 0xc0) != 0x80 || b < 0xc2) {
                return 0;
            }
            i += 2;
        } else if ((b & 0xf0) == 0xe0) {
            if (i + 2 >= len || (bytes[i + 1] & 0xc0) != 0x80 || (bytes[i + 2] & 0xc0) != 0x80) {
                return 0;
            }
            if (b == 0xe0 && bytes[i + 1] < 0xa0) {
                return 0;
            }
            if (b == 0xed && bytes[i + 1] >= 0xa0) {
                return 0;
            }
            i += 3;
        } else if ((b & 0xf8) == 0xf0) {
            if (i + 3 >= len || (bytes[i + 1] & 0xc0) != 0x80 || (bytes[i + 2] & 0xc0) != 0x80 ||
                (bytes[i + 3] & 0xc0) != 0x80) {
                return 0;
            }
            if (b == 0xf0 && bytes[i + 1] < 0x90) {
                return 0;
            }
            if (b == 0xf4 && bytes[i + 1] > 0x8f) {
                return 0;
            }
            if (b > 0xf4) {
                return 0;
            }
            i += 4;
        } else {
            return 0;
        }
    }

    return 1;
}

def_status def_encode(
    const char *input,
    char *output,
    size_t output_capacity,
    size_t *output_length
) {
    char canonical[4096];
    size_t canonical_len = 0;
    def_status status = canonicalize_copy(input, canonical, sizeof(canonical), &canonical_len);
    if (status != DEF_OK) {
        return status;
    }

    size_t out_len = 0;
    for (size_t i = 0; i < canonical_len; i++) {
        unsigned char byte = (unsigned char)canonical[i];
        if (is_literal_byte(byte)) {
            status = append_char(output, &out_len, output_capacity, (char)byte);
        } else {
            status = append_escape(output, &out_len, output_capacity, byte);
        }
        if (status != DEF_OK) {
            return status;
        }
    }

    if (out_len >= output_capacity) {
        return DEF_ERR_BUFFER_TOO_SMALL;
    }

    output[out_len] = '\0';
    *output_length = out_len;
    return DEF_OK;
}

def_status def_decode(
    const char *encoded,
    char *output,
    size_t output_capacity,
    size_t *output_length
) {
    size_t in_len = strlen(encoded);
    unsigned char bytes[4096];
    size_t byte_len = 0;

    if (in_len > DEF_MAX_LABEL_LENGTH) {
        return DEF_ERR_LABEL_TOO_LONG;
    }

    for (size_t i = 0; i < in_len;) {
        unsigned char ch = (unsigned char)encoded[i];
        if (is_literal_byte(ch)) {
            if (byte_len >= sizeof(bytes)) {
                return DEF_ERR_BUFFER_TOO_SMALL;
            }
            bytes[byte_len++] = ch;
            i++;
            continue;
        }

        if (ch != '-') {
            return DEF_ERR_INVALID_ESCAPE;
        }
        if (i + 3 > in_len) {
            return DEF_ERR_INVALID_ESCAPE;
        }

        unsigned char value;
        if (!parse_hex_byte((unsigned char)encoded[i + 1], (unsigned char)encoded[i + 2], &value)) {
            return DEF_ERR_INVALID_ESCAPE;
        }

        if (byte_len >= sizeof(bytes)) {
            return DEF_ERR_BUFFER_TOO_SMALL;
        }
        bytes[byte_len++] = value;
        i += 3;
    }

    if (!utf8_validate(bytes, byte_len)) {
        return DEF_ERR_INVALID_UTF8;
    }

    if (byte_len >= output_capacity) {
        return DEF_ERR_BUFFER_TOO_SMALL;
    }

    memcpy(output, bytes, byte_len);
    output[byte_len] = '\0';
    *output_length = byte_len;
    return DEF_OK;
}
