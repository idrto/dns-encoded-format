import json
from pathlib import Path

import pytest

from dns_encoded_format import DefError, decode, encode

VECTORS = json.loads(
    Path(__file__).resolve().parents[3].joinpath("vectors", "test-vectors.json").read_text(
        encoding="utf-8"
    )
)


def reason_to_error(reason: str) -> DefError:
    return DefError("", reason)


@pytest.mark.parametrize("case", VECTORS["encode"], ids=lambda c: repr(c["input"]))
def test_encode(case: dict[str, str]) -> None:
    assert encode(case["input"]) == case["encoded"]


@pytest.mark.parametrize("case", VECTORS["encode_hash"], ids=lambda c: repr(c["input"]))
def test_encode_hash(case: dict[str, str]) -> None:
    assert encode(case["input"]) == case["encoded"]


@pytest.mark.parametrize("case", VECTORS["encode_errors"], ids=lambda c: repr(c["input"]))
def test_encode_errors(case: dict[str, str]) -> None:
    with pytest.raises(DefError) as exc:
        encode(case["input"])
    assert exc.value.code == case["reason"]


@pytest.mark.parametrize("case", VECTORS["decode"], ids=lambda c: repr(c["input"]))
def test_decode(case: dict[str, str]) -> None:
    assert decode(case["input"]) == case["decoded"]


@pytest.mark.parametrize("case", VECTORS["decode_errors"], ids=lambda c: repr(c["input"]))
def test_decode_errors(case: dict[str, str]) -> None:
    with pytest.raises(DefError) as exc:
        decode(case["input"])
    assert exc.value.code == case["reason"]
