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
	IdrtoMarkerHost             = "idrto-h1"
	ReservedHostXn              = "xn"
	HashBodyLength              = 50
	StructuralSeparator         = "--"
	StructuralSeparatorEscaped  = "-2d-2d"
)

const base36Alphabet = "0123456789abcdefghijklmnopqrstuvwxyz"

var (
	ErrLabelTooLong    = errors.New("encoded label exceeds 63 characters")
	ErrInvalidEscape   = errors.New("invalid escape sequence")
	ErrInvalidUTF8     = errors.New("invalid utf-8 byte sequence")
	ErrInvalidEncoding = errors.New("invalid profile label or marker")
	ErrInvalidLocator  = errors.New("invalid profile locator")
	ErrNotDecodable    = errors.New("profile hash label is not decodable")
)

var hexDigits = [256]byte{
	'0': 0, '1': 1, '2': 2, '3': 3, '4': 4, '5': 5, '6': 6, '7': 7, '8': 8, '9': 9,
	'a': 10, 'b': 11, 'c': 12, 'd': 13, 'e': 14, 'f': 15,
	'A': 10, 'B': 11, 'C': 12, 'D': 13, 'E': 14, 'F': 15,
}

var hexChars = [16]byte{'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'}

func isLiteralByte(b byte) bool {
	return (b >= 0x61 && b <= 0x7a) || (b >= 0x30 && b <= 0x39)
}

func isHostStart(b byte) bool {
	return isLiteralByte(b)
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

func EncodeBody(input string) string {
	return encodeBytes(canonicalizeBytes([]byte(input)))
}

func parseHexByte(h1, h2 byte) (byte, bool) {
	hi, ok1 := hexDigits[h1]
	lo, ok2 := hexDigits[h2]
	if !ok1 || !ok2 || hi > 15 || lo > 15 {
		return 0, false
	}
	return hi*16 + lo, true
}

func DecodeBody(body string) (string, error) {
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

func markerHostPrefix(marker string) (string, error) {
	if !strings.HasSuffix(marker, StructuralSeparator) {
		return "", ErrInvalidEncoding
	}
	return marker[:len(marker)-len(StructuralSeparator)], nil
}

func validateHost(host, marker string) error {
	markerHost, err := markerHostPrefix(marker)
	if err != nil {
		return err
	}
	if host == ReservedHostXn || host == markerHost {
		return ErrInvalidLocator
	}
	return nil
}

func splitLocator(locator, marker string) (string, string, error) {
	sep := strings.Index(locator, StructuralSeparator)
	if sep <= 0 || sep+2 >= len(locator) {
		return "", "", ErrInvalidLocator
	}
	host := locator[:sep]
	entity := locator[sep+2:]
	if entity == "" {
		return "", "", ErrInvalidLocator
	}
	if len(host) == 0 || !isHostStart(host[0]) {
		return "", "", ErrInvalidLocator
	}
	if err := validateHost(host, marker); err != nil {
		return "", "", err
	}
	return host, entity, nil
}

func base36(data []byte) string {
	n := new(big.Int).SetBytes(data)
	if n.Sign() == 0 {
		return strings.Repeat("0", HashBodyLength)
	}

	base := big.NewInt(36)
	zero := big.NewInt(0)
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
	return nil
}

func isBase36Byte(b byte) bool {
	return (b >= '0' && b <= '9') || (b >= 'a' && b <= 'z')
}

func EncodeProfile(locator, marker string) (string, error) {
	if err := validateMarker(marker); err != nil {
		return "", err
	}

	canonical := string(canonicalizeBytes([]byte(locator)))
	host, entity, err := splitLocator(canonical, marker)
	if err != nil {
		return "", err
	}

	hostBody := encodeBytes([]byte(host))
	entityBody := encodeBytes([]byte(entity))
	label := hostBody + StructuralSeparator + entityBody

	if len(label) <= MaxLabelLength {
		return label, nil
	}

	hashInput := hostBody + StructuralSeparatorEscaped + entityBody
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
	if len(label) > MaxLabelLength {
		return "", ErrLabelTooLong
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

	if strings.HasPrefix(label, "xn--") {
		return "", ErrInvalidEncoding
	}

	sep := strings.Index(label, StructuralSeparator)
	if sep <= 0 || sep+2 > len(label) {
		return "", ErrInvalidEncoding
	}

	host, err := DecodeBody(label[:sep])
	if err != nil {
		return "", err
	}
	entity, err := DecodeBody(label[sep+2:])
	if err != nil {
		return "", err
	}
	if host == "" || entity == "" || strings.Contains(host, StructuralSeparator) {
		return "", ErrInvalidLocator
	}
	if err := validateHost(host, marker); err != nil {
		return "", err
	}
	return host + StructuralSeparator + entity, nil
}

func Encode(locator string) (string, error) {
	return EncodeProfile(locator, IdrtoHashMarker)
}

func Decode(label string) (string, error) {
	return DecodeProfile(label, IdrtoHashMarker)
}

type VectorsFile struct {
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
