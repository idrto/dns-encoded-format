package def_test

import (
	"encoding/json"
	"errors"
	"os"
	"testing"

	"github.com/idrto/dns-encoded-format/go/def"
)

type vectorsFile struct {
	Encode       []struct{ Input, Encoded string }
	EncodeHash   []struct{ Input, Encoded string } `json:"encode_hash"`
	EncodeErrors []struct{ Input, Reason string }
	Decode       []struct{ Input, Decoded string }
	DecodeErrors []struct{ Input, Reason string }
}

func loadVectors(t *testing.T) vectorsFile {
	t.Helper()
	data, err := os.ReadFile("../../../vectors/test-vectors.json")
	if err != nil {
		t.Fatal(err)
	}
	var v vectorsFile
	if err := json.Unmarshal(data, &v); err != nil {
		t.Fatal(err)
	}
	return v
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
		t.Fatalf("unknown reason: %s", reason)
		return nil
	}
}

func TestEncodeVectors(t *testing.T) {
	for _, c := range loadVectors(t).Encode {
		got, err := def.Encode(c.Input)
		if err != nil {
			t.Fatalf("encode(%q): %v", c.Input, err)
		}
		if got != c.Encoded {
			t.Fatalf("encode(%q) = %q, want %q", c.Input, got, c.Encoded)
		}
	}
}

func TestEncodeHashVectors(t *testing.T) {
	for _, c := range loadVectors(t).EncodeHash {
		got, err := def.Encode(c.Input)
		if err != nil {
			t.Fatalf("encode(%q): %v", c.Input, err)
		}
		if got != c.Encoded {
			t.Fatalf("encode(%q) = %q, want %q", c.Input, got, c.Encoded)
		}
	}
}

func TestEncodeErrors(t *testing.T) {
	for _, c := range loadVectors(t).EncodeErrors {
		_, err := def.Encode(c.Input)
		if !errors.Is(err, reasonToError(c.Reason)) {
			t.Fatalf("encode(%q): got %v, want %v", c.Input, err, c.Reason)
		}
	}
}

func TestDecodeVectors(t *testing.T) {
	for _, c := range loadVectors(t).Decode {
		got, err := def.Decode(c.Input)
		if err != nil {
			t.Fatalf("decode(%q): %v", c.Input, err)
		}
		if got != c.Decoded {
			t.Fatalf("decode(%q) = %q, want %q", c.Input, got, c.Decoded)
		}
	}
}

func TestDecodeErrors(t *testing.T) {
	for _, c := range loadVectors(t).DecodeErrors {
		_, err := def.Decode(c.Input)
		if !errors.Is(err, reasonToError(c.Reason)) {
			t.Fatalf("decode(%q): got %v, want %v", c.Input, err, c.Reason)
		}
	}
}
