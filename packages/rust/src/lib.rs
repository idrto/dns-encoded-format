pub const MAX_LABEL_LENGTH: usize = 63;

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum DefError {
    LabelTooLong,
    InvalidEscape,
    InvalidUtf8,
}

impl std::fmt::Display for DefError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            DefError::LabelTooLong => write!(f, "encoded label exceeds 63 characters"),
            DefError::InvalidEscape => write!(f, "invalid escape sequence"),
            DefError::InvalidUtf8 => write!(f, "invalid utf-8 byte sequence"),
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

fn ensure_fits(current_len: usize, add_len: usize) -> Result<(), DefError> {
    if current_len + add_len > MAX_LABEL_LENGTH {
        Err(DefError::LabelTooLong)
    } else {
        Ok(())
    }
}

pub fn encode(input: &str) -> Result<String, DefError> {
    let canonical = canonicalize(input);
    let mut out = String::new();

    for byte in canonical.as_bytes() {
        if is_literal_byte(*byte) {
            ensure_fits(out.len(), 1)?;
            out.push(*byte as char);
        } else {
            let escape = format!("-{byte:02x}");
            ensure_fits(out.len(), escape.len())?;
            out.push_str(&escape);
        }
    }

    Ok(out)
}

fn parse_hex_byte(h1: u8, h2: u8) -> Option<u8> {
    let to_nibble = |c: u8| match c {
        b'0'..=b'9' => Some(c - b'0'),
        b'a'..=b'f' => Some(c - b'a' + 10),
        _ => None,
    };
    Some(to_nibble(h1)? * 16 + to_nibble(h2)?)
}

pub fn decode(encoded: &str) -> Result<String, DefError> {
    if encoded.len() > MAX_LABEL_LENGTH {
        return Err(DefError::LabelTooLong);
    }

    let bytes = encoded.as_bytes();
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

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn encode_vectors() {
        let cases = [
            ("alice", "alice"),
            ("Alice", "alice"),
            ("USER@example.COM", "user-40example-2ecom"),
            ("Laptop.US-East", "laptop-2eus-2deast"),
            ("alice-1", "alice-2d1"),
            ("ssh", "ssh"),
            ("postgres", "postgres"),
            ("用户", "-e7-94-a8-e6-88-b7"),
            ("😊", "-f0-9f-98-8a"),
            ("", ""),
            (
                "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
            ),
        ];

        for (input, expected) in cases {
            assert_eq!(encode(input).unwrap(), expected);
        }
    }

    #[test]
    fn encode_errors() {
        let cases = [(
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
            DefError::LabelTooLong,
        )];

        for (input, expected) in cases {
            assert_eq!(encode(input).unwrap_err(), expected);
        }
    }

    #[test]
    fn decode_vectors() {
        let cases = [
            ("alice", "alice"),
            ("user-40example-2ecom", "user@example.com"),
            ("-e7-94-a8-e6-88-b7", "用户"),
            ("", ""),
            (
                "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
            ),
        ];

        for (input, expected) in cases {
            assert_eq!(decode(input).unwrap(), expected);
        }
    }

    #[test]
    fn decode_errors() {
        let cases = [
            ("abc-", DefError::InvalidEscape),
            ("-gg", DefError::InvalidEscape),
            ("-c0-80", DefError::InvalidUtf8),
            (
                "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                DefError::LabelTooLong,
            ),
        ];

        for (input, expected) in cases {
            assert_eq!(decode(input).unwrap_err(), expected);
        }
    }
}
