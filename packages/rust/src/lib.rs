use sha2::{Digest, Sha256};

pub const MAX_LABEL_LENGTH: usize = 63;
pub const IDRTO_HASH_MARKER: &str = "idrto-h1--";
pub const IDRTO_MARKER_HOST: &str = "idrto-h1";
pub const RESERVED_HOST_XN: &str = "xn";
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
    InvalidLocator,
    NotDecodable,
}

impl std::fmt::Display for DefError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            DefError::LabelTooLong => write!(f, "encoded label exceeds 63 characters"),
            DefError::InvalidEscape => write!(f, "invalid escape sequence"),
            DefError::InvalidUtf8 => write!(f, "invalid utf-8 byte sequence"),
            DefError::InvalidEncoding => write!(f, "invalid profile label or marker"),
            DefError::InvalidLocator => write!(f, "invalid profile locator"),
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
fn is_host_start(byte: u8) -> bool {
    is_literal_byte(byte)
}

fn canonicalize_in_place(bytes: &mut [u8]) {
    for byte in bytes.iter_mut() {
        if (0x41..=0x5a).contains(byte) {
            *byte += 32;
        }
    }
}

pub fn encode_body(input: &str) -> String {
    let mut bytes = input.as_bytes().to_vec();
    canonicalize_in_place(&mut bytes);
    encode_bytes(&bytes)
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

pub fn decode_body(body: &str) -> Result<String, DefError> {
    let bytes = body.as_bytes();
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

fn marker_host_prefix(marker: &str) -> Result<&str, DefError> {
    marker
        .strip_suffix(STRUCTURAL_SEPARATOR)
        .ok_or(DefError::InvalidEncoding)
}

fn validate_host(host: &str, marker: &str) -> Result<(), DefError> {
    let marker_host = marker_host_prefix(marker)?;
    if host == RESERVED_HOST_XN || host == marker_host {
        return Err(DefError::InvalidLocator);
    }
    Ok(())
}

fn split_locator<'a>(locator: &'a str, marker: &str) -> Result<(&'a str, &'a str), DefError> {
    let sep = locator
        .find(STRUCTURAL_SEPARATOR)
        .ok_or(DefError::InvalidLocator)?;
    if sep == 0 || sep + 2 >= locator.len() {
        return Err(DefError::InvalidLocator);
    }

    let host = &locator[..sep];
    let entity = &locator[sep + 2..];
    if entity.is_empty() {
        return Err(DefError::InvalidLocator);
    }

    let host_bytes = host.as_bytes();
    if host_bytes.is_empty() || !is_host_start(host_bytes[0]) {
        return Err(DefError::InvalidLocator);
    }

    validate_host(host, marker)?;
    Ok((host, entity))
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
    {
        return Err(DefError::InvalidEncoding);
    }
    Ok(())
}

fn is_base36_byte(byte: u8) -> bool {
    (b'0'..=b'9').contains(&byte) || (b'a'..=b'z').contains(&byte)
}

pub fn encode_profile(locator: &str, marker: &str) -> Result<String, DefError> {
    validate_marker(marker)?;

    let mut canonical = locator.as_bytes().to_vec();
    canonicalize_in_place(&mut canonical);
    let canonical_text =
        std::str::from_utf8(&canonical).map_err(|_| DefError::InvalidUtf8)?;
    let (host, entity) = split_locator(canonical_text, marker)?;

    let host_body = encode_bytes(host.as_bytes());
    let entity_body = encode_bytes(entity.as_bytes());
    let label = format!("{host_body}{STRUCTURAL_SEPARATOR}{entity_body}");

    if label.len() <= MAX_LABEL_LENGTH {
        return Ok(label);
    }

    let hash_input = format!("{host_body}{STRUCTURAL_SEPARATOR_ESCAPED}{entity_body}");
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

    if label.len() > MAX_LABEL_LENGTH {
        return Err(DefError::LabelTooLong);
    }

    if label.starts_with(marker) {
        let digest = &label[marker.len()..];
        if digest.len() != HASH_BODY_LENGTH || !digest.bytes().all(is_base36_byte) {
            return Err(DefError::InvalidEncoding);
        }
        return Err(DefError::NotDecodable);
    }

    if label.starts_with("xn--") {
        return Err(DefError::InvalidEncoding);
    }

    let sep = label
        .find(STRUCTURAL_SEPARATOR)
        .ok_or(DefError::InvalidEncoding)?;
    if sep == 0 || sep + 2 > label.len() {
        return Err(DefError::InvalidEncoding);
    }

    let host = decode_body(&label[..sep])?;
    let entity = decode_body(&label[sep + 2..])?;

    if host.is_empty() || entity.is_empty() || host.contains(STRUCTURAL_SEPARATOR) {
        return Err(DefError::InvalidLocator);
    }

    validate_host(&host, marker)?;

    Ok(format!("{host}{STRUCTURAL_SEPARATOR}{entity}"))
}

pub fn encode(locator: &str) -> Result<String, DefError> {
    encode_profile(locator, IDRTO_HASH_MARKER)
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
        let path = PathBuf::from(env!("CARGO_MANIFEST_DIR"))
            .join("../../vectors/test-vectors.json");
        serde_json::from_str(&fs::read_to_string(path).expect("read vectors")).expect("parse vectors")
    }

    fn err(reason: &str) -> DefError {
        match reason {
            "label_too_long" => DefError::LabelTooLong,
            "invalid_escape" => DefError::InvalidEscape,
            "invalid_utf8" => DefError::InvalidUtf8,
            "invalid_encoding" => DefError::InvalidEncoding,
            "invalid_locator" => DefError::InvalidLocator,
            "not_decodable" => DefError::NotDecodable,
            other => panic!("unknown reason: {other}"),
        }
    }

    #[test]
    fn vectors() {
        let v = load_vectors();

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
}
