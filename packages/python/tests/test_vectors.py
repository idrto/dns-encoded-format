import json
from pathlib import Path

import pytest

from dns_encoded_format import (
    DefError,
    IDRTO_HASH_MARKER,
    decode,
    decode_body,
    decode_profile,
    encode,
    encode_body,
    encode_profile,
)

VECTORS = json.loads(
    Path(__file__).resolve().parents[3].joinpath("vectors", "test-vectors.json").read_text(
        encoding="utf-8"
    )
)


def reason_to_error(reason: str) -> DefError:
    return DefError("", reason)


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
