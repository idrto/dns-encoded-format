package def

import (
	"errors"
	"fmt"
	"strings"
	"unicode/utf8"
)

const MaxLabelLength = 63

var (
	ErrLabelTooLong   = errors.New("encoded label exceeds 63 characters")
	ErrInvalidEscape  = errors.New("invalid escape sequence")
	ErrInvalidUTF8    = errors.New("invalid utf-8 byte sequence")
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

func ensureFits(currentLen, addLen int) error {
	if currentLen+addLen > MaxLabelLength {
		return ErrLabelTooLong
	}
	return nil
}

func Encode(input string) (string, error) {
	var out strings.Builder
	currentLen := 0

	for _, r := range canonicalize(input) {
		buf := make([]byte, 4)
		n := utf8.EncodeRune(buf, r)
		for _, b := range buf[:n] {
			if isLiteralByte(b) {
				if err := ensureFits(currentLen, 1); err != nil {
					return "", err
				}
				out.WriteByte(b)
				currentLen++
			} else {
				esc := fmt.Sprintf("-%02x", b)
				if err := ensureFits(currentLen, len(esc)); err != nil {
					return "", err
				}
				out.WriteString(esc)
				currentLen += len(esc)
			}
		}
	}

	return out.String(), nil
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

func Decode(encoded string) (string, error) {
	if len(encoded) > MaxLabelLength {
		return "", ErrLabelTooLong
	}

	bytes := []byte(encoded)
	out := make([]byte, 0, len(bytes))

	for i := 0; i < len(bytes); {
		b := bytes[i]
		if isLiteralByte(b) {
			out = append(out, b)
			i++
			continue
		}
		if b != '-' {
			return "", ErrInvalidEscape
		}
		if i+3 > len(bytes) {
			return "", ErrInvalidEscape
		}
		value, ok := parseHexByte(bytes[i+1], bytes[i+2])
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
