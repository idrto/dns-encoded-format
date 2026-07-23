package def

import (
	"crypto/sha256"
	"encoding/json"
	"errors"
	"math/big"
	"os"
	"strings"
	"unicode/utf8"
)

const (
	MaxLabelLength              = 63
	IdrtoHashMarker             = "idrto-h1--"
	HashBodyLength              = 50
	StructuralSeparator         = "--"
	StructuralSeparatorEscaped = "-2d-2d"
)

const base36Alphabet = "0123456789abcdefghijklmnopqrstuvwxyz"

var (
	ErrLabelTooLong    = errors.New("encoded label exceeds 63 characters")
	ErrInvalidEscape   = errors.New("invalid escape sequence")
	ErrInvalidUTF8     = errors.New("invalid utf-8 byte sequence")
	ErrInvalidEncoding = errors.New("invalid profile label or marker")
	ErrNotDecodable    = errors.New("profile hash label is not decodable")
)

var hexChars = [16]byte{'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'}

func isLiteralByte(b byte) bool {
	return (b >= 0x61 && b <= 0x7a) || (b >= 0x30 && b <= 0x39)
}

func canonicalizeBytes(input []byte) []byte {
	out := make([]byte, len(input))
	copy(out, input)
	for i, b := range out {
		if b >= 'A' && b <= 'Z' {
			out[i] = b + ('a' - 'A')
		}
	}
	return out
}

func encodeBytes(bytes []byte) string {
	var out strings.Builder
	out.Grow(len(bytes) * 2)
	for _, b := range bytes {
		if isLiteralByte(b) {
			out.WriteByte(b)
		} else {
			out.WriteByte('-')
			out.WriteByte(hexChars[b>>4])
			out.WriteByte(hexChars[b&0x0f])
		}
	}
	return out.String()
}

func EncodeComponent(input string) string {
	return encodeBytes(canonicalizeBytes([]byte(input)))
}

func EncodeBody(input string) string {
	components := strings.Split(input, StructuralSeparator)
	for i := range components {
		components[i] = EncodeComponent(components[i])
	}
	return strings.Join(components, StructuralSeparator)
}

func parseHexByte(h1, h2 byte) (byte, bool) {
	nibble := func(b byte) (byte, bool) {
		switch {
		case b >= '0' && b <= '9':
			return b - '0', true
		case b >= 'a' && b <= 'f':
			return b - 'a' + 10, true
		default:
			return 0, false
		}
	}
	hi, ok1 := nibble(h1)
	lo, ok2 := nibble(h2)
	if !ok1 || !ok2 {
		return 0, false
	}
	return hi*16 + lo, true
}

func DecodeComponent(component string) (string, error) {
	out := make([]byte, 0, len(component))
	for i := 0; i < len(component); {
		b := component[i]
		if isLiteralByte(b) {
			out = append(out, b)
			i++
			continue
		}
		if b != '-' {
			return "", ErrInvalidEscape
		}
		if i+3 > len(component) {
			return "", ErrInvalidEscape
		}
		value, ok := parseHexByte(component[i+1], component[i+2])
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

func DecodeBody(body string) (string, error) {
	components := strings.Split(body, StructuralSeparator)
	for i := range components {
		decoded, err := DecodeComponent(components[i])
		if err != nil {
			return "", err
		}
		components[i] = decoded
	}
	return strings.Join(components, StructuralSeparator), nil
}

func base36(data []byte) string {
	n := new(big.Int).SetBytes(data)
	if n.Sign() == 0 {
		return strings.Repeat("0", HashBodyLength)
	}

	base := big.NewInt(36)
	rem := new(big.Int)
	var digits [HashBodyLength]byte

	for i := HashBodyLength - 1; i >= 0; i-- {
		n.DivMod(n, base, rem)
		digits[i] = base36Alphabet[rem.Int64()]
	}
	return string(digits[:])
}

func validateMarker(marker string) error {
	if len(marker) < 3 || len(marker) > 13 || !strings.HasSuffix(marker, StructuralSeparator) || strings.HasPrefix(marker, "xn--") {
		return ErrInvalidEncoding
	}
	for i := 0; i < len(marker); i++ {
		b := marker[i]
		if !((b >= 'a' && b <= 'z') || (b >= '0' && b <= '9') || b == '-') {
			return ErrInvalidEncoding
		}
	}
	return nil
}

func isBase36Byte(b byte) bool {
	return (b >= '0' && b <= '9') || (b >= 'a' && b <= 'z')
}

func EncodeProfile(input, marker string) (string, error) {
	if err := validateMarker(marker); err != nil {
		return "", err
	}

	label := EncodeBody(input)
	if label == "" || strings.HasPrefix(label, "-") || strings.HasSuffix(label, "-") {
		return "", ErrInvalidEncoding
	}

	if len(label) <= MaxLabelLength && !strings.HasPrefix(label, marker) {
		return label, nil
	}

	hashInput := EncodeComponent(input)
	sum := sha256.Sum256([]byte(hashInput))
	encoded := marker + base36(sum[:])
	if len(encoded) > MaxLabelLength {
		return "", ErrLabelTooLong
	}
	return encoded, nil
}

func DecodeProfile(label, marker string) (string, error) {
	if err := validateMarker(marker); err != nil {
		return "", err
	}
	if strings.HasPrefix(label, marker) {
		digest := label[len(marker):]
		if len(digest) != HashBodyLength {
			return "", ErrInvalidEncoding
		}
		for i := 0; i < len(digest); i++ {
			if !isBase36Byte(digest[i]) {
				return "", ErrInvalidEncoding
			}
		}
		return "", ErrNotDecodable
	}

	if len(label) > MaxLabelLength {
		return "", ErrLabelTooLong
	}
	if label == "" || strings.HasPrefix(label, "-") || strings.HasSuffix(label, "-") {
		return "", ErrInvalidEncoding
	}

	return DecodeBody(label)
}

func Encode(input string) (string, error) {
	return EncodeProfile(input, IdrtoHashMarker)
}

func Decode(label string) (string, error) {
	return DecodeProfile(label, IdrtoHashMarker)
}

type VectorsFile struct {
	EncodeComponent     []struct{ Input, Encoded string } `json:"encode_component"`
	DecodeComponent     []struct{ Input, Decoded string } `json:"decode_component"`
	EncodeBody          []struct{ Input, Encoded string } `json:"encode_body"`
	DecodeBody          []struct{ Input, Decoded string } `json:"decode_body"`
	DecodeBodyErrors    []struct{ Input, Reason string }  `json:"decode_body_errors"`
	EncodeProfile       []struct{ Input, Encoded string } `json:"encode_profile"`
	EncodeProfileHash   []struct{ Input, Encoded string } `json:"encode_profile_hash"`
	EncodeProfileErrors []struct{ Input, Reason string }  `json:"encode_profile_errors"`
	DecodeProfile       []struct{ Input, Decoded string } `json:"decode_profile"`
	DecodeProfileErrors []struct{ Input, Reason string }  `json:"decode_profile_errors"`
}

func LoadVectorsForTest() (VectorsFile, error) {
	return loadVectors()
}

func loadVectors() (VectorsFile, error) {
	data, err := os.ReadFile("../../../vectors/test-vectors.json")
	if err != nil {
		return VectorsFile{}, err
	}
	var v VectorsFile
	if err := json.Unmarshal(data, &v); err != nil {
		return VectorsFile{}, err
	}
	return v, nil
}
