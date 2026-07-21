package def

import (
	"crypto/sha256"
	"errors"
	"fmt"
	"math/big"
	"strings"
	"unicode/utf8"
)

const (
	MaxLabelLength   = 63
	MaxDefBodyLength = 62
	DefPrefix        = "d"
	HashPrefix       = "h"
	hashBodyLength   = 50
)

const base36Alphabet = "0123456789abcdefghijklmnopqrstuvwxyz"

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

func base36(data []byte) string {
	n := new(big.Int).SetBytes(data)
	if n.Sign() == 0 {
		return strings.Repeat("0", hashBodyLength)
	}

	base := big.NewInt(36)
	zero := big.NewInt(0)
	rem := new(big.Int)
	var digits []byte

	for n.Cmp(zero) > 0 {
		n.DivMod(n, base, rem)
		digits = append(digits, base36Alphabet[rem.Int64()])
	}

	for i, j := 0, len(digits)-1; i < j; i, j = i+1, j-1 {
		digits[i], digits[j] = digits[j], digits[i]
	}

	out := string(digits)
	if len(out) < hashBodyLength {
		out = strings.Repeat("0", hashBodyLength-len(out)) + out
	}
	return out
}

func encodeHash(canonicalBytes []byte) (string, error) {
	sum := sha256.Sum256(canonicalBytes)
	encoded := HashPrefix + base36(sum[:])
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
