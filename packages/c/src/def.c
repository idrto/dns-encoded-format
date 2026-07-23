#include "def.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static const char BASE36_ALPHABET[] = "0123456789abcdefghijklmnopqrstuvwxyz";
static const char HEX_ENCODE[] = "0123456789abcdef";

static int is_literal_byte(unsigned char byte) {
    return (byte >= 0x61 && byte <= 0x7a) || (byte >= 0x30 && byte <= 0x39);
}

static int is_base36_byte(unsigned char byte) {
    return (byte >= 0x30 && byte <= 0x39) || (byte >= 0x61 && byte <= 0x7a);
}

static int starts_with(const char *text, const char *prefix) {
    return strncmp(text, prefix, strlen(prefix)) == 0;
}

static def_status append_body_char(char *output, size_t *len, size_t cap, char ch) {
    if (*len + 1 >= cap) {
        return DEF_ERR_BUFFER_TOO_SMALL;
    }
    output[*len] = ch;
    (*len)++;
    return DEF_OK;
}

static def_status append_body_escape(char *output, size_t *len, size_t cap, unsigned char byte) {
    if (*len + 3 >= cap) {
        return DEF_ERR_BUFFER_TOO_SMALL;
    }

    output[*len] = '-';
    output[*len + 1] = HEX_ENCODE[(byte >> 4) & 0x0f];
    output[*len + 2] = HEX_ENCODE[byte & 0x0f];
    *len += 3;
    return DEF_OK;
}

static int parse_hex_byte(unsigned char h1, unsigned char h2, unsigned char *out) {
    int hi = h1 >= '0' && h1 <= '9' ? h1 - '0' :
             h1 >= 'a' && h1 <= 'f' ? h1 - 'a' + 10 : -1;
    int lo = h2 >= '0' && h2 <= '9' ? h2 - '0' :
             h2 >= 'a' && h2 <= 'f' ? h2 - 'a' + 10 : -1;

    if (hi < 0 || lo < 0) {
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

typedef struct {
    uint32_t state[8];
    uint64_t count;
    unsigned char buffer[64];
} sha256_ctx;

static const uint32_t SHA256_K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

static uint32_t sha256_rotr(uint32_t x, uint32_t n) {
    return (x >> n) | (x << (32 - n));
}

static void sha256_transform(sha256_ctx *ctx, const unsigned char data[64]) {
    uint32_t w[64];
    uint32_t a, b, c, d, e, f, g, h;
    size_t i;

    for (i = 0; i < 16; i++) {
        w[i] = ((uint32_t)data[i * 4] << 24) | ((uint32_t)data[i * 4 + 1] << 16) |
               ((uint32_t)data[i * 4 + 2] << 8) | (uint32_t)data[i * 4 + 3];
    }
    for (i = 16; i < 64; i++) {
        uint32_t s0 = sha256_rotr(w[i - 15], 7) ^ sha256_rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = sha256_rotr(w[i - 2], 17) ^ sha256_rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];

    for (i = 0; i < 64; i++) {
        uint32_t s1 = sha256_rotr(e, 6) ^ sha256_rotr(e, 11) ^ sha256_rotr(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t temp1 = h + s1 + ch + SHA256_K[i] + w[i];
        uint32_t s0 = sha256_rotr(a, 2) ^ sha256_rotr(a, 13) ^ sha256_rotr(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = s0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

static void sha256_init(sha256_ctx *ctx) {
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;
    ctx->count = 0;
}

static void sha256_update(sha256_ctx *ctx, const unsigned char *data, size_t len) {
    size_t i = 0;
    size_t idx = (size_t)((ctx->count >> 3) & 0x3f);

    ctx->count += (uint64_t)len * 8;

    if (idx + len >= 64) {
        if (idx > 0) {
            size_t fill = 64 - idx;
            memcpy(ctx->buffer + idx, data, fill);
            sha256_transform(ctx, ctx->buffer);
            i = fill;
            idx = 0;
        }
        while (i + 64 <= len) {
            sha256_transform(ctx, data + i);
            i += 64;
        }
    }

    if (i < len) {
        memcpy(ctx->buffer + idx, data + i, len - i);
    }
}

static void sha256_final(sha256_ctx *ctx, unsigned char out[32]) {
    unsigned char pad[64];
    size_t idx = (size_t)((ctx->count >> 3) & 0x3f);
    size_t pad_len = (idx < 56) ? (56 - idx) : (120 - idx);
    uint64_t bitcount = ctx->count;
    size_t i;

    memset(pad, 0, sizeof(pad));
    pad[0] = 0x80;
    sha256_update(ctx, pad, pad_len);

    pad[0] = (unsigned char)((bitcount >> 56) & 0xff);
    pad[1] = (unsigned char)((bitcount >> 48) & 0xff);
    pad[2] = (unsigned char)((bitcount >> 40) & 0xff);
    pad[3] = (unsigned char)((bitcount >> 32) & 0xff);
    pad[4] = (unsigned char)((bitcount >> 24) & 0xff);
    pad[5] = (unsigned char)((bitcount >> 16) & 0xff);
    pad[6] = (unsigned char)((bitcount >> 8) & 0xff);
    pad[7] = (unsigned char)(bitcount & 0xff);
    sha256_update(ctx, pad, 8);

    for (i = 0; i < 8; i++) {
        out[i * 4] = (unsigned char)((ctx->state[i] >> 24) & 0xff);
        out[i * 4 + 1] = (unsigned char)((ctx->state[i] >> 16) & 0xff);
        out[i * 4 + 2] = (unsigned char)((ctx->state[i] >> 8) & 0xff);
        out[i * 4 + 3] = (unsigned char)(ctx->state[i] & 0xff);
    }
}

static void sha256_digest(const unsigned char *data, size_t len, unsigned char out[32]) {
    sha256_ctx ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, data, len);
    sha256_final(&ctx, out);
}

static def_status base36_encode(
    const unsigned char *data,
    size_t len,
    char *output,
    size_t output_capacity,
    size_t *output_length
) {
    unsigned char buf[32];
    size_t i;

    if (len != 32) {
        return DEF_ERR_BUFFER_TOO_SMALL;
    }
    if (output_capacity < DEF_HASH_BODY_LENGTH + 1) {
        return DEF_ERR_BUFFER_TOO_SMALL;
    }

    memcpy(buf, data, len);

    for (i = DEF_HASH_BODY_LENGTH; i > 0; i--) {
        unsigned int rem = 0;
        for (size_t j = 0; j < len; j++) {
            unsigned int cur = (rem << 8) | buf[j];
            buf[j] = (unsigned char)(cur / 36);
            rem = cur % 36;
        }
        output[i - 1] = BASE36_ALPHABET[rem];
    }

    output[DEF_HASH_BODY_LENGTH] = '\0';
    *output_length = DEF_HASH_BODY_LENGTH;
    return DEF_OK;
}

static def_status encode_bytes(
    const unsigned char *bytes,
    size_t byte_len,
    char *output,
    size_t output_capacity,
    size_t *output_length
) {
    size_t out_len = 0;
    def_status status;

    if (!utf8_validate(bytes, byte_len)) {
        return DEF_ERR_INVALID_UTF8;
    }

    for (size_t i = 0; i < byte_len; i++) {
        unsigned char byte = bytes[i];
        if (byte >= 'A' && byte <= 'Z') {
            byte = (unsigned char)(byte + ('a' - 'A'));
        }
        if (is_literal_byte(byte)) {
            status = append_body_char(output, &out_len, output_capacity, (char)byte);
        } else {
            status = append_body_escape(output, &out_len, output_capacity, byte);
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

static def_status decode_bytes(
    const char *body,
    size_t body_len,
    char *output,
    size_t output_capacity,
    size_t *output_length
) {
    size_t byte_len = 0;

    for (size_t i = 0; i < body_len;) {
        unsigned char ch = (unsigned char)body[i];
        if (is_literal_byte(ch)) {
            if (byte_len + 1 >= output_capacity) {
                return DEF_ERR_BUFFER_TOO_SMALL;
            }
            output[byte_len++] = (char)ch;
            i++;
            continue;
        }

        if (ch != '-') {
            return DEF_ERR_INVALID_ESCAPE;
        }
        if (i + 3 > body_len) {
            return DEF_ERR_INVALID_ESCAPE;
        }

        unsigned char value;
        if (!parse_hex_byte((unsigned char)body[i + 1], (unsigned char)body[i + 2], &value)) {
            return DEF_ERR_INVALID_ESCAPE;
        }

        if (byte_len + 1 >= output_capacity) {
            return DEF_ERR_BUFFER_TOO_SMALL;
        }
        output[byte_len++] = (char)value;
        i += 3;
    }

    if (!utf8_validate((const unsigned char *)output, byte_len)) {
        return DEF_ERR_INVALID_UTF8;
    }

    output[byte_len] = '\0';
    *output_length = byte_len;
    return DEF_OK;
}

static def_status validate_marker(const char *marker) {
    size_t marker_len = strlen(marker);

    if (marker_len < 3 || marker_len > 13) {
        return DEF_ERR_INVALID_ENCODING;
    }
    if (marker[marker_len - 2] != '-' || marker[marker_len - 1] != '-') {
        return DEF_ERR_INVALID_ENCODING;
    }
    if (starts_with(marker, "xn--")) {
        return DEF_ERR_INVALID_ENCODING;
    }
    for (size_t i = 0; i < marker_len; i++) {
        unsigned char ch = (unsigned char)marker[i];
        if (!((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '-')) {
            return DEF_ERR_INVALID_ENCODING;
        }
    }
    return DEF_OK;
}

static int checked_encoded_capacity(size_t input_len, size_t *capacity) {
    if (input_len > (SIZE_MAX - 1) / 3) {
        return 0;
    }
    *capacity = input_len * 3 + 1;
    return 1;
}

static def_status encode_profile_hash(
    const char *canonical_component,
    size_t canonical_component_len,
    const char *marker,
    char *output,
    size_t output_capacity,
    size_t *output_length
) {
    unsigned char digest[32];
    size_t marker_len = strlen(marker);
    size_t hash_len = 0;
    def_status status;

    if (marker_len + DEF_HASH_BODY_LENGTH > DEF_MAX_LABEL_LENGTH) {
        return DEF_ERR_LABEL_TOO_LONG;
    }
    if (output_capacity < marker_len + DEF_HASH_BODY_LENGTH + 1) {
        return DEF_ERR_BUFFER_TOO_SMALL;
    }

    sha256_digest((const unsigned char *)canonical_component, canonical_component_len, digest);
    memcpy(output, marker, marker_len);
    status = base36_encode(
        digest,
        sizeof(digest),
        output + marker_len,
        output_capacity - marker_len,
        &hash_len
    );
    if (status != DEF_OK) {
        return status;
    }

    *output_length = marker_len + hash_len;
    output[*output_length] = '\0';
    return DEF_OK;
}

def_status def_encode_component(
    const char *input,
    char *output,
    size_t output_capacity,
    size_t *output_length
) {
    return encode_bytes(
        (const unsigned char *)input,
        strlen(input),
        output,
        output_capacity,
        output_length
    );
}

def_status def_decode_component(
    const char *component,
    char *output,
    size_t output_capacity,
    size_t *output_length
) {
    if (output_capacity == 0) {
        return DEF_ERR_BUFFER_TOO_SMALL;
    }
    return decode_bytes(component, strlen(component), output, output_capacity, output_length);
}

def_status def_encode_body(
    const char *input,
    char *output,
    size_t output_capacity,
    size_t *output_length
) {
    size_t input_len = strlen(input);
    size_t segment_start = 0;
    size_t out_len = 0;

    for (size_t i = 0; i <= input_len;) {
        int at_separator = i + 1 < input_len && input[i] == '-' && input[i + 1] == '-';
        if (i == input_len || at_separator) {
            size_t component_len = 0;
            def_status status = encode_bytes(
                (const unsigned char *)input + segment_start,
                i - segment_start,
                output + out_len,
                output_capacity > out_len ? output_capacity - out_len : 0,
                &component_len
            );
            if (status != DEF_OK) {
                return status;
            }
            out_len += component_len;

            if (at_separator) {
                if (out_len + 2 >= output_capacity) {
                    return DEF_ERR_BUFFER_TOO_SMALL;
                }
                output[out_len++] = '-';
                output[out_len++] = '-';
                output[out_len] = '\0';
                i += 2;
                segment_start = i;
            } else {
                *output_length = out_len;
                return DEF_OK;
            }
        } else {
            i++;
        }
    }

    return DEF_OK;
}

def_status def_decode_body(
    const char *body,
    char *output,
    size_t output_capacity,
    size_t *output_length
) {
    size_t body_len = strlen(body);
    size_t segment_start = 0;
    size_t out_len = 0;

    if (output_capacity == 0) {
        return DEF_ERR_BUFFER_TOO_SMALL;
    }

    for (size_t i = 0; i <= body_len;) {
        int at_separator = i + 1 < body_len && body[i] == '-' && body[i + 1] == '-';
        if (i == body_len || at_separator) {
            size_t component_len = 0;
            def_status status = decode_bytes(
                body + segment_start,
                i - segment_start,
                output + out_len,
                output_capacity - out_len,
                &component_len
            );
            if (status != DEF_OK) {
                return status;
            }
            out_len += component_len;

            if (at_separator) {
                if (out_len + 2 >= output_capacity) {
                    return DEF_ERR_BUFFER_TOO_SMALL;
                }
                output[out_len++] = '-';
                output[out_len++] = '-';
                output[out_len] = '\0';
                i += 2;
                segment_start = i;
            } else {
                *output_length = out_len;
                return DEF_OK;
            }
        } else {
            i++;
        }
    }

    return DEF_OK;
}

def_status def_encode_profile(
    const char *input,
    const char *marker,
    char *output,
    size_t output_capacity,
    size_t *output_length
) {
    size_t input_len = strlen(input);
    size_t temporary_capacity;
    char *core;
    char *canonical_component;
    size_t core_len = 0;
    size_t canonical_component_len = 0;
    def_status status;

    status = validate_marker(marker);
    if (status != DEF_OK) {
        return status;
    }
    if (!checked_encoded_capacity(input_len, &temporary_capacity)) {
        return DEF_ERR_BUFFER_TOO_SMALL;
    }

    core = (char *)malloc(temporary_capacity);
    canonical_component = (char *)malloc(temporary_capacity);
    if (core == NULL || canonical_component == NULL) {
        free(core);
        free(canonical_component);
        return DEF_ERR_BUFFER_TOO_SMALL;
    }

    status = def_encode_body(input, core, temporary_capacity, &core_len);
    if (status != DEF_OK) {
        free(core);
        free(canonical_component);
        return status;
    }
    if (core_len == 0 || core[0] == '-' || core[core_len - 1] == '-') {
        free(core);
        free(canonical_component);
        return DEF_ERR_INVALID_ENCODING;
    }

    if (core_len <= DEF_MAX_LABEL_LENGTH && !starts_with(core, marker)) {
        if (core_len + 1 > output_capacity) {
            free(core);
            free(canonical_component);
            return DEF_ERR_BUFFER_TOO_SMALL;
        }
        memcpy(output, core, core_len + 1);
        *output_length = core_len;
        free(core);
        free(canonical_component);
        return DEF_OK;
    }

    status = def_encode_component(input, canonical_component, temporary_capacity, &canonical_component_len);
    if (status == DEF_OK) {
        status = encode_profile_hash(
            canonical_component,
            canonical_component_len,
            marker,
            output,
            output_capacity,
            output_length
        );
    }
    free(core);
    free(canonical_component);
    return status;
}

def_status def_decode_profile(
    const char *label,
    const char *marker,
    char *output,
    size_t output_capacity,
    size_t *output_length
) {
    size_t label_len = strlen(label);
    size_t marker_len = strlen(marker);
    def_status status = validate_marker(marker);

    if (status != DEF_OK) {
        return status;
    }

    if (starts_with(label, marker)) {
        const char *digest = label + marker_len;
        size_t digest_len = label_len - marker_len;

        if (digest_len != DEF_HASH_BODY_LENGTH) {
            return DEF_ERR_INVALID_ENCODING;
        }
        for (size_t i = 0; i < digest_len; i++) {
            if (!is_base36_byte((unsigned char)digest[i])) {
                return DEF_ERR_INVALID_ENCODING;
            }
        }
        return DEF_ERR_NOT_DECODABLE;
    }

    if (label_len > DEF_MAX_LABEL_LENGTH) {
        return DEF_ERR_LABEL_TOO_LONG;
    }
    if (label_len == 0 || label[0] == '-' || label[label_len - 1] == '-') {
        return DEF_ERR_INVALID_ENCODING;
    }

    return def_decode_body(label, output, output_capacity, output_length);
}

def_status def_encode(
    const char *input,
    char *output,
    size_t output_capacity,
    size_t *output_length
) {
    return def_encode_profile(input, DEF_IDRTO_HASH_MARKER, output, output_capacity, output_length);
}

def_status def_decode(
    const char *label,
    char *output,
    size_t output_capacity,
    size_t *output_length
) {
    return def_decode_profile(label, DEF_IDRTO_HASH_MARKER, output, output_capacity, output_length);
}
