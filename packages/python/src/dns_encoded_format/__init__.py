"""DNS Encoded Format (DEF) encoder and decoder."""

from __future__ import annotations

MAX_LABEL_LENGTH = 63


class DefError(Exception):
    def __init__(self, message: str, code: str) -> None:
        super().__init__(message)
        self.code = code


def _is_literal_byte(byte: int) -> bool:
    return (0x61 <= byte <= 0x7A) or (0x30 <= byte <= 0x39)


def _canonicalize(text: str) -> str:
    return "".join(ch.lower() if "A" <= ch <= "Z" else ch for ch in text)


def _ensure_fits(current_len: int, add_len: int) -> None:
    if current_len + add_len > MAX_LABEL_LENGTH:
        raise DefError("encoded label exceeds 63 characters", "label_too_long")


def encode(text: str) -> str:
    out: list[str] = []
    current_len = 0

    for byte in _canonicalize(text).encode("utf-8"):
        if _is_literal_byte(byte):
            _ensure_fits(current_len, 1)
            out.append(chr(byte))
            current_len += 1
        else:
            escape = f"-{byte:02x}"
            _ensure_fits(current_len, len(escape))
            out.append(escape)
            current_len += len(escape)

    return "".join(out)


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


def decode(encoded: str) -> str:
    if len(encoded) > MAX_LABEL_LENGTH:
        raise DefError("encoded label exceeds 63 characters", "label_too_long")

    out = bytearray()
    i = 0
    while i < len(encoded):
        code = ord(encoded[i])
        if (0x61 <= code <= 0x7A) or (0x30 <= code <= 0x39):
            out.append(code)
            i += 1
            continue

        if code != 0x2D:
            raise DefError("invalid character in encoded input", "invalid_escape")

        if i + 3 > len(encoded):
            raise DefError("truncated escape sequence", "invalid_escape")

        value = _parse_hex_byte(ord(encoded[i + 1]), ord(encoded[i + 2]))
        if value is None:
            raise DefError("invalid escape sequence", "invalid_escape")

        out.append(value)
        i += 3

    try:
        return out.decode("utf-8")
    except UnicodeDecodeError as exc:
        raise DefError("invalid utf-8 byte sequence", "invalid_utf8") from exc
