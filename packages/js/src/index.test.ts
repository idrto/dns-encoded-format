import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { describe, expect, it } from "vitest";
import {
  decode,
  decodeBody,
  decodeComponent,
  decodeProfile,
  encode,
  encodeBody,
  encodeComponent,
  encodeProfile,
  IDRTO_HASH_MARKER,
} from "./index.js";

const vectorsPath = join(
  dirname(fileURLToPath(import.meta.url)),
  "..",
  "..",
  "..",
  "vectors",
  "test-vectors.json",
);
const vectors = JSON.parse(readFileSync(vectorsPath, "utf8")) as {
  encode_component: Array<{ input: string; encoded: string }>;
  decode_component: Array<{ input: string; decoded: string }>;
  encode_body: Array<{ input: string; encoded: string }>;
  decode_body: Array<{ input: string; decoded: string }>;
  decode_body_errors: Array<{ input: string; reason: string }>;
  encode_profile: Array<{ input: string; encoded: string }>;
  encode_profile_hash: Array<{ input: string; encoded: string }>;
  encode_profile_errors: Array<{ input: string; reason: string }>;
  decode_profile: Array<{ input: string; decoded: string }>;
  decode_profile_errors: Array<{ input: string; reason: string }>;
};

describe("components", () => {
  for (const { input, encoded } of vectors.encode_component) {
    it(`encodes ${JSON.stringify(input)}`, () => {
      expect(encodeComponent(input)).toBe(encoded);
    });
  }

  for (const { input, decoded } of vectors.decode_component) {
    it(`decodes ${JSON.stringify(input)}`, () => {
      expect(decodeComponent(input)).toBe(decoded);
    });
  }

  it("rejects a raw structural separator", () => {
    expect(() => decodeComponent("a--b")).toThrowError(
      expect.objectContaining({ code: "invalid_escape" }),
    );
  });
});

describe("encodeBody", () => {
  for (const { input, encoded } of vectors.encode_body) {
    it(`encodes ${JSON.stringify(input)}`, () => {
      expect(encodeBody(input)).toBe(encoded);
    });
  }
});

describe("decodeBody", () => {
  for (const { input, decoded } of vectors.decode_body) {
    it(`decodes ${JSON.stringify(input)}`, () => {
      expect(decodeBody(input)).toBe(decoded);
    });
  }

  for (const { input, reason } of vectors.decode_body_errors) {
    it(`rejects decodeBody ${JSON.stringify(input)} (${reason})`, () => {
      expect(() => decodeBody(input)).toThrowError(
        expect.objectContaining({ code: reason }),
      );
    });
  }
});

describe("encodeProfile", () => {
  for (const { input, encoded } of vectors.encode_profile) {
    it(`encodes ${JSON.stringify(input)}`, async () => {
      await expect(encodeProfile(input, IDRTO_HASH_MARKER)).resolves.toBe(
        encoded,
      );
    });
  }

  for (const { input, encoded } of vectors.encode_profile_hash) {
    it(`hash-encodes ${JSON.stringify(input)}`, async () => {
      await expect(encodeProfile(input, IDRTO_HASH_MARKER)).resolves.toBe(
        encoded,
      );
    });
  }

  for (const { input, reason } of vectors.encode_profile_errors) {
    it(`rejects encodeProfile ${JSON.stringify(input)} (${reason})`, async () => {
      await expect(encodeProfile(input, IDRTO_HASH_MARKER)).rejects.toMatchObject(
        { code: reason },
      );
    });
  }
});

describe("decodeProfile", () => {
  for (const { input, decoded } of vectors.decode_profile) {
    it(`decodes ${JSON.stringify(input)}`, () => {
      expect(decodeProfile(input, IDRTO_HASH_MARKER)).toBe(decoded);
    });
  }

  for (const { input, reason } of vectors.decode_profile_errors) {
    it(`rejects decodeProfile ${JSON.stringify(input)} (${reason})`, () => {
      expect(() => decodeProfile(input, IDRTO_HASH_MARKER)).toThrowError(
        expect.objectContaining({ code: reason }),
      );
    });
  }
});

describe("idr.to shortcuts", () => {
  for (const { input, encoded } of vectors.encode_profile) {
    it(`encode ${JSON.stringify(input)}`, async () => {
      await expect(encode(input)).resolves.toBe(encoded);
    });
  }

  for (const { input, decoded } of vectors.decode_profile) {
    it(`decode ${JSON.stringify(input)}`, () => {
      expect(decode(input)).toBe(decoded);
    });
  }
});

describe("round trip", () => {
  for (const { input } of vectors.encode_profile) {
    it(`round trips ${JSON.stringify(input)}`, async () => {
      expect(decode(await encode(input))).toBe(
        input.replace(/[A-Z]/g, (ch) => ch.toLowerCase()),
      );
    });
  }
});

describe("deterministic core properties", () => {
  const alphabet = ["a", "B", "0", "-", "@", ".", "é", "用", "😊"];
  const values = ["", "--", "a---b", "a-----b"];
  let state = 0x5eed1234;
  for (let caseIndex = 0; caseIndex < 200; caseIndex++) {
    let value = "";
    const length = caseIndex % 24;
    for (let i = 0; i < length; i++) {
      state = (Math.imul(state, 1664525) + 1013904223) >>> 0;
      value += alphabet[state % alphabet.length]!;
    }
    values.push(value);
  }

  for (const value of values) {
    it(`preserves structure for ${JSON.stringify(value)}`, () => {
      const components = value.split("--");
      const encoded = encodeBody(value);
      const canonical = value.replace(/[A-Z]/g, (ch) => ch.toLowerCase());

      expect(decodeBody(encoded)).toBe(canonical);
      expect(encoded.split("--")).toHaveLength(components.length);
      for (const component of components) {
        expect(encodeComponent(component)).not.toContain("--");
      }
    });
  }
});

describe("generic profile behavior", () => {
  it("allows xn-- as ordinary DEF structure", async () => {
    await expect(encodeProfile("xn--value")).resolves.toBe("xn--value");
    expect(decodeProfile("xn--value")).toBe("xn--value");
  });

  it("hashes a reversible marker-prefixed output", async () => {
    const encoded = await encodeProfile("x--value", "x--");
    expect(encoded).toMatch(/^x--[0-9a-z]{50}$/);
    expect(() => decodeProfile(encoded, "x--")).toThrowError(
      expect.objectContaining({ code: "not_decodable" }),
    );
  });
});
