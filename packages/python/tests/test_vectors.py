import json
from pathlib import Path

import pytest

from dns_encoded_format import (
    DefError,
    IDRTO_HASH_MARKER,
    decode,
    decode_body,
    decode_component,
    decode_profile,
    encode,
    encode_body,
    encode_component,
    encode_profile,
)

VECTORS = json.loads(
    Path(__file__).resolve().parents[3].joinpath("vectors", "test-vectors.json").read_text(
        encoding="utf-8"
    )
)


def reason_to_error(reason: str) -> DefError:
    return DefError("", reason)


@pytest.mark.parametrize(
    "case", VECTORS["encode_component"], ids=lambda c: repr(c["input"])
)
def test_encode_component(case: dict[str, str]) -> None:
    assert encode_component(case["input"]) == case["encoded"]


@pytest.mark.parametrize(
    "case", VECTORS["decode_component"], ids=lambda c: repr(c["input"])
)
def test_decode_component(case: dict[str, str]) -> None:
    assert decode_component(case["input"]) == case["decoded"]


def test_decode_component_rejects_structural_separator() -> None:
    with pytest.raises(DefError) as exc:
        decode_component("a--b")
    assert exc.value.code == "invalid_escape"


@pytest.mark.parametrize("case", VECTORS["encode_body"], ids=lambda c: repr(c["input"]))
def test_encode_body(case: dict[str, str]) -> None:
    assert encode_body(case["input"]) == case["encoded"]


@pytest.mark.parametrize("case", VECTORS["decode_body"], ids=lambda c: repr(c["input"]))
def test_decode_body(case: dict[str, str]) -> None:
    assert decode_body(case["input"]) == case["decoded"]


@pytest.mark.parametrize(
    "case", VECTORS["decode_body_errors"], ids=lambda c: repr(c["input"])
)
def test_decode_body_errors(case: dict[str, str]) -> None:
    with pytest.raises(DefError) as exc:
        decode_body(case["input"])
    assert exc.value.code == case["reason"]


@pytest.mark.parametrize("case", VECTORS["encode_profile"], ids=lambda c: repr(c["input"]))
def test_encode_profile(case: dict[str, str]) -> None:
    assert encode_profile(case["input"], IDRTO_HASH_MARKER) == case["encoded"]


@pytest.mark.parametrize(
    "case", VECTORS["encode_profile_hash"], ids=lambda c: repr(c["input"])
)
def test_encode_profile_hash(case: dict[str, str]) -> None:
    assert encode_profile(case["input"], IDRTO_HASH_MARKER) == case["encoded"]


@pytest.mark.parametrize(
    "case", VECTORS["encode_profile_errors"], ids=lambda c: repr(c["input"])
)
def test_encode_profile_errors(case: dict[str, str]) -> None:
    with pytest.raises(DefError) as exc:
        encode_profile(case["input"], IDRTO_HASH_MARKER)
    assert exc.value.code == case["reason"]


@pytest.mark.parametrize("case", VECTORS["decode_profile"], ids=lambda c: repr(c["input"]))
def test_decode_profile(case: dict[str, str]) -> None:
    assert decode_profile(case["input"], IDRTO_HASH_MARKER) == case["decoded"]


@pytest.mark.parametrize(
    "case", VECTORS["decode_profile_errors"], ids=lambda c: repr(c["input"])
)
def test_decode_profile_errors(case: dict[str, str]) -> None:
    with pytest.raises(DefError) as exc:
        decode_profile(case["input"], IDRTO_HASH_MARKER)
    assert exc.value.code == case["reason"]


@pytest.mark.parametrize("case", VECTORS["encode_profile"], ids=lambda c: repr(c["input"]))
def test_idrto_encode(case: dict[str, str]) -> None:
    assert encode(case["input"]) == case["encoded"]


@pytest.mark.parametrize("case", VECTORS["decode_profile"], ids=lambda c: repr(c["input"]))
def test_idrto_decode(case: dict[str, str]) -> None:
    assert decode(case["input"]) == case["decoded"]


def _property_values() -> list[str]:
    alphabet = ["a", "B", "0", "-", "@", ".", "é", "用", "😊"]
    values = ["", "--", "a---b", "a-----b"]
    state = 0x5EED1234
    for case_index in range(200):
        value: list[str] = []
        for _ in range(case_index % 24):
            state = (state * 1664525 + 1013904223) & 0xFFFFFFFF
            value.append(alphabet[state % len(alphabet)])
        values.append("".join(value))
    return values


@pytest.mark.parametrize("value", _property_values(), ids=repr)
def test_deterministic_core_properties(value: str) -> None:
    components = value.split("--")
    encoded = encode_body(value)
    canonical = value.translate(
        str.maketrans("ABCDEFGHIJKLMNOPQRSTUVWXYZ", "abcdefghijklmnopqrstuvwxyz")
    )

    assert decode_body(encoded) == canonical
    assert len(encoded.split("--")) == len(components)
    assert all("--" not in encode_component(component) for component in components)


def test_profile_allows_xn_structure() -> None:
    assert encode_profile("xn--value") == "xn--value"
    assert decode_profile("xn--value") == "xn--value"


def test_profile_hashes_marker_prefixed_reversible_output() -> None:
    encoded = encode_profile("x--value", "x--")
    assert encoded.startswith("x--")
    assert len(encoded.removeprefix("x--")) == 50
    with pytest.raises(DefError) as exc:
        decode_profile(encoded, "x--")
    assert exc.value.code == "not_decodable"
