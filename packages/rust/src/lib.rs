use sha2::{Digest, Sha256};

pub const MAX_LABEL_LENGTH: usize = 63;
pub const IDRTO_HASH_MARKER: &str = "idrto-h1--";
pub const HASH_BODY_LENGTH: usize = 50;
pub const STRUCTURAL_SEPARATOR: &str = "--";
pub const STRUCTURAL_SEPARATOR_ESCAPED: &str = "-2d-2d";

const BASE36_ALPHABET: &[u8; 36] = b"0123456789abcdefghijklmnopqrstuvwxyz";
const HEX: &[u8; 16] = b"0123456789abcdef";

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum DefError {
    LabelTooLong,
    InvalidEscape,
    InvalidUtf8,
    InvalidEncoding,
    NotDecodable,
}

impl std::fmt::Display for DefError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            DefError::LabelTooLong => write!(f, "encoded label exceeds 63 characters"),
            DefError::InvalidEscape => write!(f, "invalid escape sequence"),
            DefError::InvalidUtf8 => write!(f, "invalid utf-8 byte sequence"),
            DefError::InvalidEncoding => write!(f, "invalid profile label or marker"),
            DefError::NotDecodable => write!(f, "profile hash label is not decodable"),
        }
    }
}

impl std::error::Error for DefError {}

#[inline]
fn is_literal_byte(byte: u8) -> bool {
    (0x61..=0x7a).contains(&byte) || (0x30..=0x39).contains(&byte)
}

#[inline]
fn canonicalize_in_place(bytes: &mut [u8]) {
    for byte in bytes.iter_mut() {
        if (0x41..=0x5a).contains(byte) {
            *byte += 32;
        }
    }
}

pub fn encode_component(input: &str) -> String {
    let mut bytes = input.as_bytes().to_vec();
    canonicalize_in_place(&mut bytes);
    encode_bytes(&bytes)
}

pub fn encode_body(input: &str) -> String {
    input
        .split(STRUCTURAL_SEPARATOR)
        .map(encode_component)
        .collect::<Vec<_>>()
        .join(STRUCTURAL_SEPARATOR)
}

fn encode_bytes(bytes: &[u8]) -> String {
    let mut out = String::with_capacity(bytes.len() * 2);
    for &byte in bytes {
        if is_literal_byte(byte) {
            out.push(byte as char);
        } else {
            out.push('-');
            out.push(HEX[(byte >> 4) as usize] as char);
            out.push(HEX[(byte & 0x0f) as usize] as char);
        }
    }
    out
}

#[inline]
fn parse_hex_byte(h1: u8, h2: u8) -> Option<u8> {
    let to_nibble = |c: u8| match c {
        b'0'..=b'9' => Some(c - b'0'),
        b'a'..=b'f' => Some(c - b'a' + 10),
        _ => None,
    };
    Some(to_nibble(h1)? * 16 + to_nibble(h2)?)
}

pub fn decode_component(component: &str) -> Result<String, DefError> {
    let bytes = component.as_bytes();
    let mut out = Vec::with_capacity(bytes.len());
    let mut i = 0;

    while i < bytes.len() {
        let byte = bytes[i];
        if is_literal_byte(byte) {
            out.push(byte);
            i += 1;
            continue;
        }

        if byte != b'-' {
            return Err(DefError::InvalidEscape);
        }
        if i + 3 > bytes.len() {
            return Err(DefError::InvalidEscape);
        }

        out.push(parse_hex_byte(bytes[i + 1], bytes[i + 2]).ok_or(DefError::InvalidEscape)?);
        i += 3;
    }

    String::from_utf8(out).map_err(|_| DefError::InvalidUtf8)
}

pub fn decode_body(body: &str) -> Result<String, DefError> {
    body.split(STRUCTURAL_SEPARATOR)
        .map(decode_component)
        .collect::<Result<Vec<_>, _>>()
        .map(|components| components.join(STRUCTURAL_SEPARATOR))
}

fn base36(data: &[u8]) -> String {
    let mut buf = data.to_vec();
    let mut digits = vec![b'0'; HASH_BODY_LENGTH];

    for i in (0..HASH_BODY_LENGTH).rev() {
        let mut rem: u32 = 0;
        for byte in buf.iter_mut() {
            let cur = (rem << 8) | u32::from(*byte);
            *byte = (cur / 36) as u8;
            rem = cur % 36;
        }
        digits[i] = BASE36_ALPHABET[rem as usize];
    }

    String::from_utf8(digits).expect("base36 alphabet is ascii")
}

fn validate_marker(marker: &str) -> Result<(), DefError> {
    if marker.len() < 3
        || marker.len() > 13
        || !marker.ends_with(STRUCTURAL_SEPARATOR)
        || marker.starts_with("xn--")
        || !marker
            .bytes()
            .all(|byte| byte.is_ascii_lowercase() || byte.is_ascii_digit() || byte == b'-')
    {
        return Err(DefError::InvalidEncoding);
    }
    Ok(())
}

fn is_base36_byte(byte: u8) -> bool {
    (b'0'..=b'9').contains(&byte) || (b'a'..=b'z').contains(&byte)
}

pub fn encode_profile(input: &str, marker: &str) -> Result<String, DefError> {
    validate_marker(marker)?;

    let label = encode_body(input);
    if label.is_empty() || label.starts_with('-') || label.ends_with('-') {
        return Err(DefError::InvalidEncoding);
    }

    if label.len() <= MAX_LABEL_LENGTH && !label.starts_with(marker) {
        return Ok(label);
    }

    let hash_input = encode_component(input);
    let digest = Sha256::digest(hash_input.as_bytes());
    let encoded = format!("{marker}{}", base36(&digest));
    if encoded.len() > MAX_LABEL_LENGTH {
        Err(DefError::LabelTooLong)
    } else {
        Ok(encoded)
    }
}

pub fn decode_profile(label: &str, marker: &str) -> Result<String, DefError> {
    validate_marker(marker)?;

    if label.starts_with(marker) {
        let digest = &label[marker.len()..];
        if digest.len() != HASH_BODY_LENGTH || !digest.bytes().all(is_base36_byte) {
            return Err(DefError::InvalidEncoding);
        }
        return Err(DefError::NotDecodable);
    }

    if label.len() > MAX_LABEL_LENGTH {
        return Err(DefError::LabelTooLong);
    }
    if label.is_empty() || label.starts_with('-') || label.ends_with('-') {
        return Err(DefError::InvalidEncoding);
    }

    decode_body(label)
}

pub fn encode(input: &str) -> Result<String, DefError> {
    encode_profile(input, IDRTO_HASH_MARKER)
}

pub fn decode(label: &str) -> Result<String, DefError> {
    decode_profile(label, IDRTO_HASH_MARKER)
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;
    use std::path::PathBuf;

    #[derive(serde::Deserialize)]
    struct Vectors {
        encode_component: Vec<Case>,
        decode_component: Vec<DecodeCase>,
        encode_body: Vec<Case>,
        decode_body: Vec<DecodeCase>,
        decode_body_errors: Vec<ErrorCase>,
        encode_profile: Vec<Case>,
        encode_profile_hash: Vec<Case>,
        encode_profile_errors: Vec<ErrorCase>,
        decode_profile: Vec<DecodeCase>,
        decode_profile_errors: Vec<ErrorCase>,
    }

    #[derive(serde::Deserialize)]
    struct Case {
        input: String,
        encoded: String,
    }

    #[derive(serde::Deserialize)]
    struct DecodeCase {
        input: String,
        decoded: String,
    }

    #[derive(serde::Deserialize)]
    struct ErrorCase {
        input: String,
        reason: String,
    }

    fn load_vectors() -> Vectors {
        let path =
            PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../vectors/test-vectors.json");
        serde_json::from_str(&fs::read_to_string(path).expect("read vectors"))
            .expect("parse vectors")
    }

    fn err(reason: &str) -> DefError {
        match reason {
            "label_too_long" => DefError::LabelTooLong,
            "invalid_escape" => DefError::InvalidEscape,
            "invalid_utf8" => DefError::InvalidUtf8,
            "invalid_encoding" => DefError::InvalidEncoding,
            "not_decodable" => DefError::NotDecodable,
            other => panic!("unknown reason: {other}"),
        }
    }

    #[test]
    fn vectors() {
        let v = load_vectors();

        for case in v.encode_component {
            assert_eq!(encode_component(&case.input), case.encoded);
        }
        for case in v.decode_component {
            assert_eq!(decode_component(&case.input).unwrap(), case.decoded);
        }
        for case in v.encode_body {
            assert_eq!(encode_body(&case.input), case.encoded);
        }
        for case in v.decode_body {
            assert_eq!(decode_body(&case.input).unwrap(), case.decoded);
        }
        for case in v.decode_body_errors {
            assert_eq!(decode_body(&case.input).unwrap_err(), err(&case.reason));
        }
        for case in v.encode_profile {
            assert_eq!(
                encode_profile(&case.input, IDRTO_HASH_MARKER).unwrap(),
                case.encoded
            );
        }
        for case in v.encode_profile_hash {
            assert_eq!(
                encode_profile(&case.input, IDRTO_HASH_MARKER).unwrap(),
                case.encoded
            );
        }
        for case in v.encode_profile_errors {
            assert_eq!(
                encode_profile(&case.input, IDRTO_HASH_MARKER).unwrap_err(),
                err(&case.reason)
            );
        }
        for case in v.decode_profile {
            assert_eq!(
                decode_profile(&case.input, IDRTO_HASH_MARKER).unwrap(),
                case.decoded
            );
        }
        for case in v.decode_profile_errors {
            assert_eq!(
                decode_profile(&case.input, IDRTO_HASH_MARKER).unwrap_err(),
                err(&case.reason)
            );
        }
    }

    #[test]
    fn deterministic_core_properties() {
        let alphabet = ["", "a", "B", "-", "--", "@", "é", "用户", "😊"];
        let mut values = alphabet.iter().map(|s| s.to_string()).collect::<Vec<_>>();
        for left in alphabet {
            for right in alphabet {
                values.push(format!("{left}{right}"));
                values.push(format!("{left}--{right}"));
            }
        }

        for value in values {
            let encoded = encode_body(&value);
            assert_eq!(decode_body(&encoded).unwrap(), value.to_ascii_lowercase());
            assert_eq!(
                encoded.matches(STRUCTURAL_SEPARATOR).count(),
                value.matches(STRUCTURAL_SEPARATOR).count()
            );
            assert!(!encode_component(&value).contains(STRUCTURAL_SEPARATOR));
        }

        assert_eq!(
            decode_component(STRUCTURAL_SEPARATOR),
            Err(DefError::InvalidEscape)
        );
    }

    #[test]
    fn generic_profile_contract() {
        assert_eq!(encode("xn--value").unwrap(), "xn--value");
        assert_eq!(decode("xn--value").unwrap(), "xn--value");

        let marker = "abc--";
        let collision = encode_profile("abc--value", marker).unwrap();
        assert!(collision.starts_with(marker));
        assert_eq!(collision.len(), marker.len() + HASH_BODY_LENGTH);
        assert_eq!(
            decode_profile(&collision, marker),
            Err(DefError::NotDecodable)
        );
    }
}
