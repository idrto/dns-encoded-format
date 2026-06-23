"""DNS Encoded Format (DEF) encoder and decoder."""

from __future__ import annotations

import hashlib

MAX_LABEL_LENGTH = 63
MAX_DEF_BODY_LENGTH = 62
DEF_PREFIX = "d"
HASH_PREFIX = "h"

CROCKFORD_ALPHABET = "0123456789abcdefghjkmnpqrstvwxyz"


class DefError(Exception):
    def __init__(self, message: str, code: str) -> None:
        super().__init__(message)
        self.code = code


def _is_literal_byte(byte: int) -> bool:
    return (0x61 <= byte <= 0x7A) or (0x30 <= byte <= 0x39)


def _canonicalize(text: str) -> str:
    return "".join(ch.lower() if "A" <= ch <= "Z" else ch for ch in text)


def _encode_def_body(data: bytes) -> str:
    out: list[str] = []
    for byte in data:
        if _is_literal_byte(byte):
            out.append(chr(byte))
        else:
            out.append(f"-{byte:02x}")
    return "".join(out)


def _crockford_base32(data: bytes) -> str:
    bits = 0
    value = 0
    out: list[str] = []

    for byte in data:
        value = (value << 8) | byte
        bits += 8
        while bits >= 5:
            out.append(CROCKFORD_ALPHABET[(value >> (bits - 5)) & 0x1F])
            bits -= 5

    if bits > 0:
        out.append(CROCKFORD_ALPHABET[(value << (5 - bits)) & 0x1F])

    return "".join(out)


def _encode_hash(canonical_bytes: bytes) -> str:
    digest = hashlib.sha256(canonical_bytes).digest()
    encoded = HASH_PREFIX + _crockford_base32(digest)
    if len(encoded) > MAX_LABEL_LENGTH:
        raise DefError("encoded label exceeds 63 characters", "label_too_long")
    return encoded


def encode(text: str) -> str:
    canonical = _canonicalize(text)
    canonical_bytes = canonical.encode("utf-8")
    body = _encode_def_body(canonical_bytes)

    if len(body) <= MAX_DEF_BODY_LENGTH:
        encoded = DEF_PREFIX + body
        if len(encoded) > MAX_LABEL_LENGTH:
            raise DefError("encoded label exceeds 63 characters", "label_too_long")
        return encoded

    return _encode_hash(body.encode("utf-8"))


def _parse_hex_byte(h1: int, h2: int) -> int | None:
    def to_nibble(value: int) -> int | None:
        if 0x30 <= value <= 0x39:
            return value - 0x30
        if 0x61 <= value <= 0x66:
            return value - 0x61 + 10
        return None

    hi = to_nibble(h1)
    lo = to_nibble(h2)
    if hi is None or lo is None:
        return None
    return hi * 16 + lo


def _decode_def_body(body: str) -> str:
    out = bytearray()
    i = 0
    while i < len(body):
        code = ord(body[i])
        if (0x61 <= code <= 0x7A) or (0x30 <= code <= 0x39):
            out.append(code)
            i += 1
            continue

        if code != 0x2D:
            raise DefError("invalid character in encoded input", "invalid_escape")

        if i + 3 > len(body):
            raise DefError("truncated escape sequence", "invalid_escape")

        value = _parse_hex_byte(ord(body[i + 1]), ord(body[i + 2]))
        if value is None:
            raise DefError("invalid escape sequence", "invalid_escape")

        out.append(value)
        i += 3

    try:
        return out.decode("utf-8")
    except UnicodeDecodeError as exc:
        raise DefError("invalid utf-8 byte sequence", "invalid_utf8") from exc


def decode(encoded: str) -> str:
    if len(encoded) > MAX_LABEL_LENGTH:
        raise DefError("encoded label exceeds 63 characters", "label_too_long")

    if not encoded:
        raise DefError("missing encoding prefix", "invalid_encoding")

    prefix = encoded[0]
    if prefix == HASH_PREFIX:
        raise DefError("hash-encoded label is not decodable", "not_decodable")
    if prefix != DEF_PREFIX:
        raise DefError("unrecognized encoding prefix", "invalid_encoding")

    return _decode_def_body(encoded[1:])
