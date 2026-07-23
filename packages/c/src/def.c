#include "def.h"

#include <stdint.h>
#include <string.h>

static const char BASE36_ALPHABET[] = "0123456789abcdefghijklmnopqrstuvwxyz";
static const char HEX_ENCODE[] = "0123456789abcdef";

static int8_t HEX_DECODE[256];
static int hex_decode_initialized = 0;

static void init_hex_decode(void) {
    if (hex_decode_initialized) {
        return;
    }

    memset(HEX_DECODE, -1, sizeof(HEX_DECODE));
    HEX_DECODE['0'] = 0;
    HEX_DECODE['1'] = 1;
    HEX_DECODE['2'] = 2;
    HEX_DECODE['3'] = 3;
    HEX_DECODE['4'] = 4;
    HEX_DECODE['5'] = 5;
    HEX_DECODE['6'] = 6;
    HEX_DECODE['7'] = 7;
    HEX_DECODE['8'] = 8;
    HEX_DECODE['9'] = 9;
    HEX_DECODE['a'] = 10;
    HEX_DECODE['b'] = 11;
    HEX_DECODE['c'] = 12;
    HEX_DECODE['d'] = 13;
    HEX_DECODE['e'] = 14;
    HEX_DECODE['f'] = 15;
    hex_decode_initialized = 1;
}

static int is_literal_byte(unsigned char byte) {
    return (byte >= 0x61 && byte <= 0x7a) || (byte >= 0x30 && byte <= 0x39);
}

static int is_host_start(unsigned char byte) {
    return is_literal_byte(byte);
}

static int is_base36_byte(unsigned char byte) {
    return (byte >= 0x30 && byte <= 0x39) || (byte >= 0x61 && byte <= 0x7a);
}

static int starts_with(const char *text, const char *prefix) {
    return strncmp(text, prefix, strlen(prefix)) == 0;
}

static const char *find_first_separator(const char *text) {
    return strstr(text, DEF_STRUCTURAL_SEPARATOR);
}

static int host_contains_separator(const char *host) {
    return find_first_separator(host) != NULL;
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

    init_hex_decode();
    hi = HEX_DECODE[h1];
    lo = HEX_DECODE[h2];

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

    for (size_t i = 0; i < byte_len; i++) {
        unsigned char byte = bytes[i];
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
    unsigned char bytes[4096];
    size_t byte_len = 0;

    for (size_t i = 0; i < body_len;) {
        unsigned char ch = (unsigned char)body[i];
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
        if (i + 3 > body_len) {
            return DEF_ERR_INVALID_ESCAPE;
        }

        unsigned char value;
        if (!parse_hex_byte((unsigned char)body[i + 1], (unsigned char)body[i + 2], &value)) {
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

    return DEF_OK;
}

static def_status split_locator(
    const char *locator,
    const char **host,
    size_t *host_len,
    const char **entity,
    size_t *entity_len
) {
    const char *sep = find_first_separator(locator);
    size_t locator_len;

    if (sep == NULL) {
        return DEF_ERR_INVALID_LOCATOR;
    }

    locator_len = strlen(locator);
    if (sep == locator || sep + 2 >= locator + locator_len) {
        return DEF_ERR_INVALID_LOCATOR;
    }

    *host = locator;
    *host_len = (size_t)(sep - locator);
    *entity = sep + 2;
    *entity_len = locator_len - *host_len - 2;

    if (*entity_len == 0) {
        return DEF_ERR_INVALID_LOCATOR;
    }
    if (*host_len == 0 || !is_host_start((unsigned char)(*host)[0])) {
        return DEF_ERR_INVALID_LOCATOR;
    }

    return DEF_OK;
}

static def_status encode_profile_hash(
    const char *host_body,
    size_t host_body_len,
    const char *entity_body,
    size_t entity_body_len,
    const char *marker,
    char *output,
    size_t output_capacity,
    size_t *output_length
) {
    unsigned char hash_input[4096];
    size_t hash_input_len = 0;
    unsigned char digest[32];
    size_t marker_len = strlen(marker);
    size_t hash_len = 0;
    def_status status;

    if (host_body_len + 6 + entity_body_len > sizeof(hash_input)) {
        return DEF_ERR_BUFFER_TOO_SMALL;
    }

    memcpy(hash_input, host_body, host_body_len);
    hash_input_len = host_body_len;
    memcpy(hash_input + hash_input_len, "-2d-2d", 6);
    hash_input_len += 6;
    memcpy(hash_input + hash_input_len, entity_body, entity_body_len);
    hash_input_len += entity_body_len;

    sha256_digest(hash_input, hash_input_len, digest);

    if (output_capacity < marker_len + DEF_HASH_BODY_LENGTH + 1) {
        return DEF_ERR_BUFFER_TOO_SMALL;
    }

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
    if (*output_length > DEF_MAX_LABEL_LENGTH) {
        return DEF_ERR_LABEL_TOO_LONG;
    }

    return DEF_OK;
}

def_status def_encode_body(
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

    return encode_bytes(
        (const unsigned char *)canonical,
        canonical_len,
        output,
        output_capacity,
        output_length
    );
}

def_status def_decode_body(
    const char *body,
    char *output,
    size_t output_capacity,
    size_t *output_length
) {
    return decode_bytes(body, strlen(body), output, output_capacity, output_length);
}

def_status def_encode_profile(
    const char *locator,
    const char *marker,
    char *output,
    size_t output_capacity,
    size_t *output_length
) {
    char canonical[4096];
    size_t canonical_len = 0;
    const char *host;
    size_t host_len = 0;
    const char *entity;
    size_t entity_len = 0;
    char host_body[4096];
    size_t host_body_len = 0;
    char entity_body[4096];
    size_t entity_body_len = 0;
    size_t label_len = 0;
    def_status status;

    status = validate_marker(marker);
    if (status != DEF_OK) {
        return status;
    }

    status = canonicalize_copy(locator, canonical, sizeof(canonical), &canonical_len);
    if (status != DEF_OK) {
        return status;
    }

    status = split_locator(canonical, &host, &host_len, &entity, &entity_len);
    if (status != DEF_OK) {
        return status;
    }

    status = encode_bytes((const unsigned char *)host, host_len, host_body, sizeof(host_body), &host_body_len);
    if (status != DEF_OK) {
        return status;
    }

    status = encode_bytes(
        (const unsigned char *)entity,
        entity_len,
        entity_body,
        sizeof(entity_body),
        &entity_body_len
    );
    if (status != DEF_OK) {
        return status;
    }

    label_len = host_body_len + 2 + entity_body_len;
    if (label_len + 1 > output_capacity) {
        return DEF_ERR_BUFFER_TOO_SMALL;
    }

    memcpy(output, host_body, host_body_len);
    output[host_body_len] = '-';
    output[host_body_len + 1] = '-';
    memcpy(output + host_body_len + 2, entity_body, entity_body_len);
    output[label_len] = '\0';

    if (label_len <= DEF_MAX_LABEL_LENGTH && !starts_with(output, "xn--")) {
        *output_length = label_len;
        return DEF_OK;
    }

    return encode_profile_hash(
        host_body,
        host_body_len,
        entity_body,
        entity_body_len,
        marker,
        output,
        output_capacity,
        output_length
    );
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
    const char *sep;
    char host[4096];
    size_t host_len = 0;
    char entity[4096];
    size_t entity_len = 0;
    size_t out_len = 0;
    def_status status;

    status = validate_marker(marker);
    if (status != DEF_OK) {
        return status;
    }

    if (label_len > DEF_MAX_LABEL_LENGTH) {
        return DEF_ERR_LABEL_TOO_LONG;
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

    if (starts_with(label, "xn--")) {
        return DEF_ERR_INVALID_ENCODING;
    }

    sep = find_first_separator(label);
    if (sep == NULL || sep == label || sep + 2 > label + label_len) {
        return DEF_ERR_INVALID_ENCODING;
    }

    status = decode_bytes(label, (size_t)(sep - label), host, sizeof(host), &host_len);
    if (status != DEF_OK) {
        return status;
    }

    status = decode_bytes(sep + 2, label_len - (size_t)(sep - label) - 2, entity, sizeof(entity), &entity_len);
    if (status != DEF_OK) {
        return status;
    }

    if (host_len == 0 || entity_len == 0 || host_contains_separator(host)) {
        return DEF_ERR_INVALID_LOCATOR;
    }

    out_len = host_len + 2 + entity_len;
    if (out_len + 1 > output_capacity) {
        return DEF_ERR_BUFFER_TOO_SMALL;
    }

    memcpy(output, host, host_len);
    output[host_len] = '-';
    output[host_len + 1] = '-';
    memcpy(output + host_len + 2, entity, entity_len);
    output[out_len] = '\0';
    *output_length = out_len;
    return DEF_OK;
}

def_status def_encode(
    const char *locator,
    char *output,
    size_t output_capacity,
    size_t *output_length
) {
    return def_encode_profile(locator, DEF_IDRTO_HASH_MARKER, output, output_capacity, output_length);
}

def_status def_decode(
    const char *label,
    char *output,
    size_t output_capacity,
    size_t *output_length
) {
    return def_decode_profile(label, DEF_IDRTO_HASH_MARKER, output, output_capacity, output_length);
}
