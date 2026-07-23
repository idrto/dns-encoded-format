"""DNS Encoded Format (DEF) encoder and decoder."""

from __future__ import annotations

import hashlib

MAX_LABEL_LENGTH = 63
IDRTO_HASH_MARKER = "idrto-h1--"
HASH_BODY_LENGTH = 50
STRUCTURAL_SEPARATOR = "--"
STRUCTURAL_SEPARATOR_ESCAPED = "-2d-2d"

BASE36_ALPHABET = "0123456789abcdefghijklmnopqrstuvwxyz"
_HEX = "0123456789abcdef"


class DefError(Exception):
    def __init__(self, message: str, code: str) -> None:
        super().__init__(message)
        self.code = code


def _is_literal_byte(byte: int) -> bool:
    return (0x61 <= byte <= 0x7A) or (0x30 <= byte <= 0x39)


def _canonicalize_bytes(data: bytes) -> bytes:
    table = bytes.maketrans(
        b"ABCDEFGHIJKLMNOPQRSTUVWXYZ",
        b"abcdefghijklmnopqrstuvwxyz",
    )
    return data.translate(table)


def _encode_bytes(data: bytes) -> str:
    parts: list[str] = []
    append = parts.append
    for byte in data:
        if _is_literal_byte(byte):
            append(chr(byte))
        else:
            append(f"-{_HEX[byte >> 4]}{_HEX[byte & 0x0F]}")
    return "".join(parts)


def encode_component(text: str) -> str:
    return _encode_bytes(_canonicalize_bytes(text.encode("utf-8")))


def encode_body(text: str) -> str:
    return STRUCTURAL_SEPARATOR.join(
        encode_component(component) for component in text.split(STRUCTURAL_SEPARATOR)
    )


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


def decode_component(component: str) -> str:
    if STRUCTURAL_SEPARATOR in component:
        raise DefError("structural separator in component", "invalid_escape")

    out = bytearray()
    i = 0
    length = len(component)
    while i < length:
        code = ord(component[i])
        if (0x61 <= code <= 0x7A) or (0x30 <= code <= 0x39):
            out.append(code)
            i += 1
            continue

        if code != 0x2D:
            raise DefError("invalid character in encoded input", "invalid_escape")

        if i + 3 > length:
            raise DefError("truncated escape sequence", "invalid_escape")

        value = _parse_hex_byte(ord(component[i + 1]), ord(component[i + 2]))
        if value is None:
            raise DefError("invalid escape sequence", "invalid_escape")

        out.append(value)
        i += 3

    try:
        return out.decode("utf-8")
    except UnicodeDecodeError as exc:
        raise DefError("invalid utf-8 byte sequence", "invalid_utf8") from exc


def decode_body(body: str) -> str:
    return STRUCTURAL_SEPARATOR.join(
        decode_component(component) for component in body.split(STRUCTURAL_SEPARATOR)
    )


def _base36(data: bytes) -> str:
    n = int.from_bytes(data, "big")
    if n == 0:
        return "0".rjust(HASH_BODY_LENGTH, "0")

    digits: list[str] = []
    while n:
        n, rem = divmod(n, 36)
        digits.append(BASE36_ALPHABET[rem])
    return "".join(reversed(digits)).rjust(HASH_BODY_LENGTH, "0")


def _validate_marker(marker: str) -> None:
    if (
        len(marker) < 3
        or len(marker) > 13
        or not marker.endswith(STRUCTURAL_SEPARATOR)
        or marker.startswith("xn--")
        or any(
            not ("a" <= char <= "z" or "0" <= char <= "9" or char == "-")
            for char in marker
        )
    ):
        raise DefError("invalid provider hash marker", "invalid_encoding")


def encode_profile(value: str, marker: str = IDRTO_HASH_MARKER) -> str:
    _validate_marker(marker)

    label = encode_body(value)
    if not label or label.startswith("-") or label.endswith("-"):
        raise DefError("invalid profile encoding", "invalid_encoding")

    if len(label) <= MAX_LABEL_LENGTH and not label.startswith(marker):
        return label

    hash_input = encode_component(value)
    digest = hashlib.sha256(hash_input.encode("utf-8")).digest()
    encoded = marker + _base36(digest)
    if len(encoded) > MAX_LABEL_LENGTH:
        raise DefError("encoded label exceeds 63 characters", "label_too_long")
    return encoded


def decode_profile(label: str, marker: str = IDRTO_HASH_MARKER) -> str:
    _validate_marker(marker)

    if label.startswith(marker):
        digest = label[len(marker) :]
        if len(digest) != HASH_BODY_LENGTH or not all(
            ch in BASE36_ALPHABET for ch in digest
        ):
            raise DefError("invalid profile hash label", "invalid_encoding")
        raise DefError("profile hash label is not decodable", "not_decodable")

    if not label or label.startswith("-") or label.endswith("-"):
        raise DefError("invalid profile encoding", "invalid_encoding")

    if len(label) > MAX_LABEL_LENGTH:
        raise DefError("encoded label exceeds 63 characters", "label_too_long")

    return decode_body(label)


def encode(value: str) -> str:
    return encode_profile(value, IDRTO_HASH_MARKER)


def decode(label: str) -> str:
    return decode_profile(label, IDRTO_HASH_MARKER)
