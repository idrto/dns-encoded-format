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
	case "invalid_locator":
		return def.ErrInvalidLocator
	case "not_decodable":
		return def.ErrNotDecodable
	default:
		panic("unknown reason: " + reason)
	}
}
