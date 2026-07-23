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


def _is_host_start(byte: int) -> bool:
    return _is_literal_byte(byte)


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


def encode_body(text: str) -> str:
    return _encode_bytes(_canonicalize_bytes(text.encode("utf-8")))


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


def decode_body(body: str) -> str:
    out = bytearray()
    i = 0
    length = len(body)
    while i < length:
        code = ord(body[i])
        if (0x61 <= code <= 0x7A) or (0x30 <= code <= 0x39):
            out.append(code)
            i += 1
            continue

        if code != 0x2D:
            raise DefError("invalid character in encoded input", "invalid_escape")

        if i + 3 > length:
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


def _split_locator(locator: str) -> tuple[str, str]:
    sep = locator.find(STRUCTURAL_SEPARATOR)
    if sep <= 0 or sep + 2 >= len(locator):
        raise DefError("invalid profile locator", "invalid_locator")

    host = locator[:sep]
    entity = locator[sep + 2 :]
    if not entity:
        raise DefError("invalid profile locator", "invalid_locator")

    host_bytes = host.encode("utf-8")
    if not host_bytes or not _is_host_start(host_bytes[0]):
        raise DefError("invalid profile host", "invalid_locator")

    return host, entity


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
    ):
        raise DefError("invalid provider hash marker", "invalid_encoding")


def encode_profile(locator: str, marker: str = IDRTO_HASH_MARKER) -> str:
    _validate_marker(marker)

    canonical = _canonicalize_bytes(locator.encode("utf-8")).decode("utf-8")
    host, entity = _split_locator(canonical)

    host_body = _encode_bytes(host.encode("utf-8"))
    entity_body = _encode_bytes(entity.encode("utf-8"))
    label = f"{host_body}{STRUCTURAL_SEPARATOR}{entity_body}"

    if len(label) <= MAX_LABEL_LENGTH and not label.startswith("xn--"):
        return label

    hash_input = f"{host_body}{STRUCTURAL_SEPARATOR_ESCAPED}{entity_body}"
    digest = hashlib.sha256(hash_input.encode("utf-8")).digest()
    encoded = marker + _base36(digest)
    if len(encoded) > MAX_LABEL_LENGTH:
        raise DefError("encoded label exceeds 63 characters", "label_too_long")
    return encoded


def decode_profile(label: str, marker: str = IDRTO_HASH_MARKER) -> str:
    _validate_marker(marker)

    if len(label) > MAX_LABEL_LENGTH:
        raise DefError("encoded label exceeds 63 characters", "label_too_long")

    if label.startswith(marker):
        digest = label[len(marker) :]
        if len(digest) != HASH_BODY_LENGTH or not all(
            ch in BASE36_ALPHABET for ch in digest
        ):
            raise DefError("invalid profile hash label", "invalid_encoding")
        raise DefError("profile hash label is not decodable", "not_decodable")

    if label.startswith("xn--"):
        raise DefError("invalid profile label", "invalid_encoding")

    sep = label.find(STRUCTURAL_SEPARATOR)
    if sep <= 0 or sep + 2 > len(label):
        raise DefError("missing profile separator", "invalid_encoding")

    host = decode_body(label[:sep])
    entity = decode_body(label[sep + 2 :])

    if not host or not entity or STRUCTURAL_SEPARATOR in host:
        raise DefError("invalid decoded profile locator", "invalid_locator")

    return f"{host}{STRUCTURAL_SEPARATOR}{entity}"


def encode(locator: str) -> str:
    return encode_profile(locator, IDRTO_HASH_MARKER)


def decode(label: str) -> str:
    return decode_profile(label, IDRTO_HASH_MARKER)
