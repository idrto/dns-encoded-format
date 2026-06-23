package def

import (
	"crypto/sha256"
	"errors"
	"fmt"
	"strings"
	"unicode/utf8"
)

const (
	MaxLabelLength    = 63
	MaxDefBodyLength  = 62
	DefPrefix         = "d"
	HashPrefix        = "h"
)

const crockfordAlphabet = "0123456789abcdefghjkmnpqrstvwxyz"

var (
	ErrLabelTooLong     = errors.New("encoded label exceeds 63 characters")
	ErrInvalidEscape    = errors.New("invalid escape sequence")
	ErrInvalidUTF8      = errors.New("invalid utf-8 byte sequence")
	ErrInvalidEncoding  = errors.New("unrecognized or missing encoding prefix")
	ErrNotDecodable     = errors.New("hash-encoded label is not decodable")
)

func isLiteralByte(b byte) bool {
	return (b >= 0x61 && b <= 0x7a) || (b >= 0x30 && b <= 0x39)
}

func canonicalize(input string) string {
	var b strings.Builder
	b.Grow(len(input))
	for _, r := range input {
		if r >= 'A' && r <= 'Z' {
			b.WriteRune(r + ('a' - 'A'))
		} else {
			b.WriteRune(r)
		}
	}
	return b.String()
}

func encodeDefBody(bytes []byte) string {
	var out strings.Builder
	for _, b := range bytes {
		if isLiteralByte(b) {
			out.WriteByte(b)
		} else {
			out.WriteString(fmt.Sprintf("-%02x", b))
		}
	}
	return out.String()
}

func crockfordBase32(data []byte) string {
	var out strings.Builder
	bits := 0
	value := 0

	for _, b := range data {
		value = (value << 8) | int(b)
		bits += 8
		for bits >= 5 {
			out.WriteByte(crockfordAlphabet[(value>>(bits-5))&0x1f])
			bits -= 5
		}
	}

	if bits > 0 {
		out.WriteByte(crockfordAlphabet[(value<<(5-bits))&0x1f])
	}

	return out.String()
}

func encodeHash(canonicalBytes []byte) (string, error) {
	sum := sha256.Sum256(canonicalBytes)
	encoded := HashPrefix + crockfordBase32(sum[:])
	if len(encoded) > MaxLabelLength {
		return "", ErrLabelTooLong
	}
	return encoded, nil
}

func Encode(input string) (string, error) {
	canonical := canonicalize(input)
	bytes := []byte(canonical)
	body := encodeDefBody(bytes)

	if len(body) <= MaxDefBodyLength {
		encoded := DefPrefix + body
		if len(encoded) > MaxLabelLength {
			return "", ErrLabelTooLong
		}
		return encoded, nil
	}

	return encodeHash([]byte(body))
}

func parseHexByte(h1, h2 byte) (byte, bool) {
	toNibble := func(c byte) (byte, bool) {
		switch {
		case c >= '0' && c <= '9':
			return c - '0', true
		case c >= 'a' && c <= 'f':
			return c - 'a' + 10, true
		default:
			return 0, false
		}
	}
	hi, ok := toNibble(h1)
	if !ok {
		return 0, false
	}
	lo, ok := toNibble(h2)
	if !ok {
		return 0, false
	}
	return hi*16 + lo, true
}

func decodeDefBody(body string) (string, error) {
	out := make([]byte, 0, len(body))

	for i := 0; i < len(body); {
		b := body[i]
		if isLiteralByte(b) {
			out = append(out, b)
			i++
			continue
		}
		if b != '-' {
			return "", ErrInvalidEscape
		}
		if i+3 > len(body) {
			return "", ErrInvalidEscape
		}
		value, ok := parseHexByte(body[i+1], body[i+2])
		if !ok {
			return "", ErrInvalidEscape
		}
		out = append(out, value)
		i += 3
	}

	if !utf8.Valid(out) {
		return "", ErrInvalidUTF8
	}
	return string(out), nil
}

func Decode(encoded string) (string, error) {
	if len(encoded) > MaxLabelLength {
		return "", ErrLabelTooLong
	}

	if len(encoded) == 0 {
		return "", ErrInvalidEncoding
	}

	switch encoded[0] {
	case HashPrefix[0]:
		return "", ErrNotDecodable
	case DefPrefix[0]:
		return decodeDefBody(encoded[1:])
	default:
		return "", ErrInvalidEncoding
	}
}
