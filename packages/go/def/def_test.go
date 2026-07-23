package def_test

import (
	"errors"
	"testing"

	"github.com/idrto/dns-encoded-format/go/def"
)

func TestVectors(t *testing.T) {
	v, err := def.LoadVectorsForTest()
	if err != nil {
		t.Fatal(err)
	}

	for _, c := range v.EncodeComponent {
		got := def.EncodeComponent(c.Input)
		if got != c.Encoded {
			t.Fatalf("EncodeComponent(%q) = %q, want %q", c.Input, got, c.Encoded)
		}
	}

	for _, c := range v.DecodeComponent {
		got, err := def.DecodeComponent(c.Input)
		if err != nil {
			t.Fatalf("DecodeComponent(%q): %v", c.Input, err)
		}
		if got != c.Decoded {
			t.Fatalf("DecodeComponent(%q) = %q, want %q", c.Input, got, c.Decoded)
		}
	}

	for _, c := range v.EncodeBody {
		got := def.EncodeBody(c.Input)
		if got != c.Encoded {
			t.Fatalf("EncodeBody(%q) = %q, want %q", c.Input, got, c.Encoded)
		}
	}

	for _, c := range v.DecodeBody {
		got, err := def.DecodeBody(c.Input)
		if err != nil {
			t.Fatalf("DecodeBody(%q): %v", c.Input, err)
		}
		if got != c.Decoded {
			t.Fatalf("DecodeBody(%q) = %q, want %q", c.Input, got, c.Decoded)
		}
	}

	for _, c := range v.DecodeBodyErrors {
		_, err := def.DecodeBody(c.Input)
		if !errors.Is(err, reasonToError(c.Reason)) {
			t.Fatalf("DecodeBody(%q): got %v, want %v", c.Input, err, c.Reason)
		}
	}

	for _, c := range v.EncodeProfile {
		got, err := def.EncodeProfile(c.Input, def.IdrtoHashMarker)
		if err != nil {
			t.Fatalf("EncodeProfile(%q): %v", c.Input, err)
		}
		if got != c.Encoded {
			t.Fatalf("EncodeProfile(%q) = %q, want %q", c.Input, got, c.Encoded)
		}
	}

	for _, c := range v.EncodeProfileHash {
		got, err := def.EncodeProfile(c.Input, def.IdrtoHashMarker)
		if err != nil {
			t.Fatalf("EncodeProfile(%q): %v", c.Input, err)
		}
		if got != c.Encoded {
			t.Fatalf("EncodeProfile(%q) = %q, want %q", c.Input, got, c.Encoded)
		}
	}

	for _, c := range v.EncodeProfileErrors {
		_, err := def.EncodeProfile(c.Input, def.IdrtoHashMarker)
		if !errors.Is(err, reasonToError(c.Reason)) {
			t.Fatalf("EncodeProfile(%q): got %v, want %v", c.Input, err, c.Reason)
		}
	}

	for _, c := range v.DecodeProfile {
		got, err := def.DecodeProfile(c.Input, def.IdrtoHashMarker)
		if err != nil {
			t.Fatalf("DecodeProfile(%q): %v", c.Input, err)
		}
		if got != c.Decoded {
			t.Fatalf("DecodeProfile(%q) = %q, want %q", c.Input, got, c.Decoded)
		}
	}

	for _, c := range v.DecodeProfileErrors {
		_, err := def.DecodeProfile(c.Input, def.IdrtoHashMarker)
		if !errors.Is(err, reasonToError(c.Reason)) {
			t.Fatalf("DecodeProfile(%q): got %v, want %v", c.Input, err, c.Reason)
		}
	}
}

func TestDeterministicCoreProperties(t *testing.T) {
	alphabet := []string{"", "a", "B", "-", "--", "@", "é", "用户", "😊"}
	values := append([]string(nil), alphabet...)
	for _, left := range alphabet {
		for _, right := range alphabet {
			values = append(values, left+right, left+"--"+right)
		}
	}

	for _, value := range values {
		encoded := def.EncodeBody(value)
		decoded, err := def.DecodeBody(encoded)
		if err != nil {
			t.Fatalf("DecodeBody(EncodeBody(%q)): %v", value, err)
		}
		if decoded != asciiLower(value) {
			t.Fatalf("DecodeBody(EncodeBody(%q)) = %q", value, decoded)
		}
		if countSeparators(encoded) != countSeparators(value) {
			t.Fatalf("separator count changed for %q: %q", value, encoded)
		}
		if countSeparators(def.EncodeComponent(value)) != 0 {
			t.Fatalf("EncodeComponent(%q) emitted a raw separator", value)
		}
	}

	if _, err := def.DecodeComponent(def.StructuralSeparator); !errors.Is(err, def.ErrInvalidEscape) {
		t.Fatalf("DecodeComponent(%q): got %v", def.StructuralSeparator, err)
	}
}

func TestGenericProfileContract(t *testing.T) {
	encoded, err := def.Encode("xn--value")
	if err != nil || encoded != "xn--value" {
		t.Fatalf("Encode(%q) = %q, %v", "xn--value", encoded, err)
	}
	decoded, err := def.Decode(encoded)
	if err != nil || decoded != "xn--value" {
		t.Fatalf("Decode(%q) = %q, %v", encoded, decoded, err)
	}

	marker := "abc--"
	collision, err := def.EncodeProfile("abc--value", marker)
	if err != nil {
		t.Fatalf("Encode marker collision: %v", err)
	}
	if len(collision) != len(marker)+def.HashBodyLength {
		t.Fatalf("marker collision did not hash: %q", collision)
	}
	if _, err := def.DecodeProfile(collision, marker); !errors.Is(err, def.ErrNotDecodable) {
		t.Fatalf("Decode marker collision: got %v", err)
	}
}

func asciiLower(value string) string {
	bytes := []byte(value)
	for i, b := range bytes {
		if b >= 'A' && b <= 'Z' {
			bytes[i] = b + ('a' - 'A')
		}
	}
	return string(bytes)
}

func countSeparators(value string) int {
	count := 0
	for i := 0; i+1 < len(value); {
		if value[i:i+2] == def.StructuralSeparator {
			count++
			i += 2
		} else {
			i++
		}
	}
	return count
}

func reasonToError(reason string) error {
	switch reason {
	case "label_too_long":
		return def.ErrLabelTooLong
	case "invalid_escape":
		return def.ErrInvalidEscape
	case "invalid_utf8":
		return def.ErrInvalidUTF8
	case "invalid_encoding":
		return def.ErrInvalidEncoding
	case "not_decodable":
		return def.ErrNotDecodable
	default:
		panic("unknown reason: " + reason)
	}
}
