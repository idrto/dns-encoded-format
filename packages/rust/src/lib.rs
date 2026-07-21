use sha2::{Digest, Sha256};

pub const MAX_LABEL_LENGTH: usize = 63;
pub const MAX_DEF_BODY_LENGTH: usize = 62;
pub const DEF_PREFIX: char = 'd';
pub const HASH_PREFIX: char = 'h';

const BASE36_ALPHABET: &[u8; 36] = b"0123456789abcdefghijklmnopqrstuvwxyz";
const HASH_BODY_LENGTH: usize = 50;

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
            DefError::InvalidEncoding => write!(f, "unrecognized or missing encoding prefix"),
            DefError::NotDecodable => write!(f, "hash-encoded label is not decodable"),
        }
    }
}

impl std::error::Error for DefError {}

fn is_literal_byte(byte: u8) -> bool {
    (0x61..=0x7a).contains(&byte) || (0x30..=0x39).contains(&byte)
}

fn canonicalize(input: &str) -> String {
    input
        .chars()
        .map(|ch| {
            if ('A'..='Z').contains(&ch) {
                ((ch as u8) + 32) as char
            } else {
                ch
            }
        })
        .collect()
}

fn encode_def_body(bytes: &[u8]) -> String {
    let mut out = String::new();
    for byte in bytes {
        if is_literal_byte(*byte) {
            out.push(*byte as char);
        } else {
            out.push_str(&format!("-{byte:02x}"));
        }
    }
    out
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

fn encode_hash(canonical_bytes: &[u8]) -> Result<String, DefError> {
    let digest = Sha256::digest(canonical_bytes);
    let encoded = format!("{HASH_PREFIX}{}", base36(&digest));
    if encoded.len() > MAX_LABEL_LENGTH {
        Err(DefError::LabelTooLong)
    } else {
        Ok(encoded)
    }
}

pub fn encode(input: &str) -> Result<String, DefError> {
    let canonical = canonicalize(input);
    let bytes = canonical.as_bytes();
    let body = encode_def_body(bytes);

    if body.len() <= MAX_DEF_BODY_LENGTH {
        let encoded = format!("{DEF_PREFIX}{body}");
        if encoded.len() > MAX_LABEL_LENGTH {
            Err(DefError::LabelTooLong)
        } else {
            Ok(encoded)
        }
    } else {
        encode_hash(body.as_bytes())
    }
}

fn parse_hex_byte(h1: u8, h2: u8) -> Option<u8> {
    let to_nibble = |c: u8| match c {
        b'0'..=b'9' => Some(c - b'0'),
        b'a'..=b'f' => Some(c - b'a' + 10),
        _ => None,
    };
    Some(to_nibble(h1)? * 16 + to_nibble(h2)?)
}

fn decode_def_body(body: &str) -> Result<String, DefError> {
    let bytes = body.as_bytes();
    let mut out = Vec::new();
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

        let value = parse_hex_byte(bytes[i + 1], bytes[i + 2]).ok_or(DefError::InvalidEscape)?;
        out.push(value);
        i += 3;
    }

    String::from_utf8(out).map_err(|_| DefError::InvalidUtf8)
}

pub fn decode(encoded: &str) -> Result<String, DefError> {
    if encoded.len() > MAX_LABEL_LENGTH {
        return Err(DefError::LabelTooLong);
    }

    let mut chars = encoded.chars();
    let prefix = chars.next().ok_or(DefError::InvalidEncoding)?;

    match prefix {
        HASH_PREFIX => Err(DefError::NotDecodable),
        DEF_PREFIX => decode_def_body(&encoded[prefix.len_utf8()..]),
        _ => Err(DefError::InvalidEncoding),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn encode_vectors() {
        let cases = [
            ("alice", "dalice"),
            ("Alice", "dalice"),
            ("USER@example.COM", "duser-40example-2ecom"),
            ("Laptop.US-East", "dlaptop-2eus-2deast"),
            ("alice-1", "dalice-2d1"),
            ("ssh", "dssh"),
            ("postgres", "dpostgres"),
            (
                "laptop.us-east~user@example.com",
                "dlaptop-2eus-2deast-7euser-40example-2ecom",
            ),
            ("用户", "d-e7-94-a8-e6-88-b7"),
            ("😊", "d-f0-9f-98-8a"),
            ("", "d"),
            (
                "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                "daaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
            ),
        ];

        for (input, expected) in cases {
            assert_eq!(encode(input).unwrap(), expected);
        }
    }

    #[test]
    fn encode_hash_vectors() {
        let cases = [
            (
                "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                "h34dk0ez8tm7vyf659gc3tm7tfyv1n4fz6iqhx4wv7dte31ztx0",
            ),
            (
                "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                "h6dl5vt15rq84vholfyw8vfmqsogsg52nitlm62npyv8po233zf",
            ),
            (
                "😊😊😊😊😊😊😊😊😊😊😊😊😊😊😊😊",
                "h6a5aelpmn14767b6y03tbo5tqkun7xcvazojsebqj825zwztsv",
            ),
        ];

        for (input, expected) in cases {
            assert_eq!(encode(input).unwrap(), expected);
        }
    }

    #[test]
    fn decode_vectors() {
        let cases = [
            ("dalice", "alice"),
            ("duser-40example-2ecom", "user@example.com"),
            (
                "dlaptop-2eus-2deast-7euser-40example-2ecom",
                "laptop.us-east~user@example.com",
            ),
            ("d-e7-94-a8-e6-88-b7", "用户"),
            ("d", ""),
            (
                "daaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
            ),
        ];

        for (input, expected) in cases {
            assert_eq!(decode(input).unwrap(), expected);
        }
    }

    #[test]
    fn decode_errors() {
        let cases = [
            ("alice", DefError::InvalidEncoding),
            ("dabc-", DefError::InvalidEscape),
            ("d-gg", DefError::InvalidEscape),
            ("d-c0-80", DefError::InvalidUtf8),
            (
                "daaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                DefError::LabelTooLong,
            ),
            (
                "h34dk0ez8tm7vyf659gc3tm7tfyv1n4fz6iqhx4wv7dte31ztx0",
                DefError::NotDecodable,
            ),
        ];

        for (input, expected) in cases {
            assert_eq!(decode(input).unwrap_err(), expected);
        }
    }
}
